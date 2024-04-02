#include "yakka_project.hpp"
#include "utilities.hpp"
#include "subprocess.hpp"
#include "spdlog/spdlog.h"
#include "glob/glob.h"
#include <fstream>
#include <string>
#include <vector>
#include <ranges>
#include <algorithm>
#include <iomanip>

namespace yakka {

/*
// Execute with admin
if(0 == CreateProcess(argv[2], params, NULL, NULL, false, 0, NULL, NULL, &si, &pi)) {
        //runas word is a hack to require UAC elevation
        ShellExecute(NULL, "runas", argv[2], params, NULL, SW_SHOWNORMAL);
}
*/
std::pair<std::string, int> exec(const std::string &command_text, const std::string &arg_text)
{
  spdlog::info("{} {}", command_text, arg_text);
  try {
    std::string command = command_text;
    if (!arg_text.empty())
      command += " " + arg_text;
#if defined(__USING_WINDOWS__)
    auto p = subprocess::Popen(command, subprocess::output{ subprocess::PIPE }, subprocess::error{ subprocess::STDOUT });
#else
    auto p      = subprocess::Popen(command, subprocess::shell{ true }, subprocess::output{ subprocess::PIPE }, subprocess::error{ subprocess::STDOUT });
#endif
#if defined(__USING_WINDOWS__)
    auto output  = p.communicate().first;
    auto retcode = p.wait();
    retcode      = p.poll();
#else
    auto output = p.communicate().first;
    p.wait();
    auto retcode = p.retcode();
#endif
    std::string output_text = output.buf.data();
    return { output_text, retcode };
  } catch (std::exception &e) {
    spdlog::error("Exception while executing: {}\n{}", command_text, e.what());
    return { "", -1 };
  }
}

int exec(const std::string &command_text, const std::string &arg_text, std::function<void(std::string &)> function)
{
  spdlog::info("{} {}", command_text, arg_text);
  try {
    std::string command = command_text;
    if (!arg_text.empty())
      command += " " + arg_text;
#if defined(__USING_WINDOWS__)
    auto p = subprocess::Popen(command, subprocess::output{ subprocess::PIPE }, subprocess::error{ subprocess::STDOUT });
#else
    auto p       = subprocess::Popen(command, subprocess::shell{ true }, subprocess::output{ subprocess::PIPE }, subprocess::error{ subprocess::STDOUT });
#endif
    auto output = p.output();
    std::array<char, 512> buffer;
    size_t count = 0;
    buffer.fill('\0');
    if (output != nullptr) {
      while (1) {
        buffer[count] = fgetc(output);
        if (feof(output))
          break;

        if (count == buffer.size() - 1 || buffer[count] == '\r' || buffer[count] == '\n') {
          std::string temp(buffer.data());
          try {
            function(temp);
          } catch (std::exception &e) {
            spdlog::debug("exec() data processing threw exception '{}'for the following data:\n{}", e.what(), temp);
          }
          buffer.fill('\0');
          count = 0;
        } else
          ++count;
      };
    }
    auto retcode = p.wait();
#if defined(__USING_WINDOWS__)
    retcode = p.poll();
#endif
    return retcode;
  } catch (std::exception &e) {
    spdlog::error("Exception while executing: {}\n{}", command_text, e.what());
  }
  return -1;
}

bool yaml_diff(const YAML::Node &node1, const YAML::Node &node2)
{
  std::vector<std::pair<const YAML::Node &, const YAML::Node &>> compare_list;
  compare_list.push_back({ node1, node2 });
  for (size_t i = 0; i < compare_list.size(); ++i) {
    const YAML::Node &left  = compare_list[i].first;
    const YAML::Node &right = compare_list[i].second;

    if (left.Type() != right.Type())
      return true;

    switch (left.Type()) {
      case YAML::NodeType::Scalar:
        if (left.Scalar() != right.Scalar())
          return true;
        break;

      case YAML::NodeType::Sequence:
        // Verify the sequences have the same length
        if (left.size() != right.size())
          return true;
        for (size_t a = 0; a < left.size(); ++a)
          compare_list.push_back({ left[a], right[a] });
        break;

      case YAML::NodeType::Map:
        // Verify the maps have the same length
        if (left.size() != right.size())
          return true;
        for (const auto &a: left) {
          auto &key  = a.first.Scalar();
          auto &node = a.second;
          if (!right[key])
            return true;
          compare_list.push_back({ node, right[key] });
        }
        break;
      default:
        break;
    }
  }

  return false;
}

YAML::Node yaml_path(const YAML::Node &node, std::string path)
{
  YAML::Node temp = node;
  std::stringstream ss(path);
  std::string s;
  while (std::getline(ss, s, '.'))
    temp.reset(temp[s]);
  return temp;
}

nlohmann::json::json_pointer json_pointer(std::string path)
{
  if (path.front() != '/') {
    path = "/" + path;
    std::replace(path.begin(), path.end(), '.', '/');
  }
  return nlohmann::json::json_pointer{ path };
}

nlohmann::json json_path(const nlohmann::json &node, std::string path)
{
  nlohmann::json::json_pointer temp(path);
  return node[temp];

  // return node[nlohmann::json_pointer(path)];
  // auto temp = node;
  // std::stringstream ss(path);
  // std::string s;
  // while (std::getline(ss, s, '.'))
  //     temp = temp[s];//.reset(temp[s]);
  // return temp;
}

std::tuple<component_list_t, feature_list_t, command_list_t> parse_arguments(const std::vector<std::string> &argument_string)
{
  component_list_t components;
  feature_list_t features;
  command_list_t commands;

  //    for (auto s = argument_string.begin(); s != argument_string.end(); ++s)
  for (auto s: argument_string) {
    // Identify features, commands, and components
    if (s.front() == '+')
      features.insert(s.substr(1));
    else if (s.back() == '!')
      commands.insert(s.substr(0, s.size() - 1));
    else
      components.insert(s);
  }

  return { std::move(components), std::move(features), std::move(commands) };
}

std::string generate_project_name(const component_list_t &components, const feature_list_t &features)
{
  std::string project_name = "";

  // Generate the project name from the project string
  for (const auto &i: components)
    project_name += i + "-";

  if (!components.empty())
    project_name.pop_back();

  for (const auto &i: features)
    project_name += "-" + i;

  if (project_name.empty())
    project_name = "none";

  return project_name;
}

/**
 * @brief Parses dependency files as output by GCC or Clang generating a vector of filenames as found in the named file
 *
 * @param filename  Name of the dependency file. Typically ending in '.d'
 * @return std::vector<std::string>  Vector of files specified as dependencies
 */
std::vector<std::string> parse_gcc_dependency_file(const std::string &filename)
{
  std::vector<std::string> dependencies;
  std::ifstream infile(filename);

  if (!infile.is_open())
    return {};

  std::string line;

  // Find and ignore the first line with the target. Typically "<target>: \"
  do {
    std::getline(infile, line);
  } while (line.length() > 0 && line.find(':') == std::string::npos);

  while (std::getline(infile, line, ' ')) {
    if (line.empty() || line.compare("\\\n") == 0)
      continue;
    if (line.back() == '\n')
      line.pop_back();
    if (line.back() == '\r')
      line.pop_back();
    dependencies.push_back(line.starts_with("./") ? line.substr(2) : line);
  }

  return dependencies;
}

void json_node_merge(nlohmann::json &merge_target, const nlohmann::json &node)
{
  switch (node.type()) {
    case nlohmann::detail::value_t::object:
      if (merge_target.type() != nlohmann::detail::value_t::object) {
        spdlog::error("Currently not supported");
        return;
      }
      // Iterate through child nodes
      for (auto it = node.begin(); it != node.end(); ++it) {
        // Check if the key is already in merge_target
        auto it2 = merge_target.find(it.key());
        if (it2 != merge_target.end()) {
          json_node_merge(it2.value(), it.value());
          continue;
        } else {
          merge_target[it.key()] = it.value();
        }
      }
      break;

    case nlohmann::detail::value_t::array:
      switch (merge_target.type()) {
        case nlohmann::detail::value_t::object:
          spdlog::error("Cannot merge array into an object");
          break;
        case nlohmann::detail::value_t::array:
        case nlohmann::detail::value_t::null:
          for (auto &i: node)
            merge_target.push_back(i);
          break;
        default:
          merge_target.push_back(node);
          break;
      }
      break;
    default:
      switch (merge_target.type()) {
        case nlohmann::detail::value_t::object:
          spdlog::error("Cannot merge scalar into an object");
          break;
        case nlohmann::detail::value_t::array:
        default:
          merge_target.push_back(node);
          break;
      }
      break;
  }
}

std::string component_dotname_to_id(const std::string dotname)
{
  return dotname.find_last_of(".") != std::string::npos ? dotname.substr(dotname.find_last_of(".") + 1) : dotname;
}

std::string try_render(inja::Environment &env, const std::string &input, const nlohmann::json &data)
{
  try {
    return env.render(input, data);
  } catch (std::exception &e) {
    spdlog::error("Template error: {}\n{}", input, e.what());
    return "";
  }
}

std::string try_render_file(inja::Environment &env, const std::string &filename, const nlohmann::json &data)
{
  try {
    return env.render_file(filename, data);
  } catch (std::exception &e) {
    spdlog::error("Template error: {}\n{}", filename, e.what());
    return "";
  }
}

std::pair<std::string, int> run_command(const std::string target, construction_task *task, project *project)
{
  std::string captured_output = "";
  inja::Environment inja_env  = inja::Environment();
  auto &blueprint             = task->match;

  std::string curdir_path = blueprint->blueprint->parent_path;
  inja_env.add_callback("$", 1, [&blueprint](const inja::Arguments &args) {
    return blueprint->regex_matches[args[0]->get<int>()];
  });
  inja_env.add_callback("curdir", 0, [&](const inja::Arguments &args) {
    return curdir_path;
  });
  inja_env.add_callback("dir", 1, [](inja::Arguments &args) {
    auto path = std::filesystem::path{ args.at(0)->get<std::string>() }.relative_path();
    if (path.has_filename())
      return path.parent_path().string();
    else
      return path.string();
  });
  inja_env.add_callback("notdir", 1, [](inja::Arguments &args) {
    return std::filesystem::path{ args.at(0)->get<std::string>() }.filename();
  });
  inja_env.add_callback("glob", [](inja::Arguments &args) {
    nlohmann::json aggregate = nlohmann::json::array();
    std::vector<std::string> string_args;
    for (const auto &i: args)
      string_args.push_back(i->get<std::string>());
    for (auto &p: glob::rglob(string_args))
      aggregate.push_back(p.generic_string());
    return aggregate;
  });
  inja_env.add_callback("absolute_dir", 1, [](inja::Arguments &args) {
    return std::filesystem::absolute(args.at(0)->get<std::string>());
  });
  inja_env.add_callback("extension", 1, [](inja::Arguments &args) {
    return std::filesystem::path{ args.at(0)->get<std::string>() }.extension().string().substr(1);
  });
  inja_env.add_callback("filesize", 1, [&](const inja::Arguments &args) {
    return fs::file_size(args[0]->get<std::string>());
  });
  inja_env.add_callback("render", 1, [&](const inja::Arguments &args) {
    return try_render(inja_env, args[0]->get<std::string>(), project->project_summary);
  });
  inja_env.add_callback("render", 2, [&curdir_path, &inja_env, &project](const inja::Arguments &args) {
    auto backup               = curdir_path;
    curdir_path               = args[1]->get<std::string>();
    std::string render_output = try_render(inja_env, args[0]->get<std::string>(), project->project_summary);
    curdir_path               = backup;
    return render_output;
  });
  inja_env.add_callback("read_file", 1, [&](const inja::Arguments &args) {
    auto file = std::ifstream(args[0]->get<std::string>());
    return std::string{ std::istreambuf_iterator<char>{ file }, {} };
  });
  inja_env.add_callback("load_yaml", 1, [&](const inja::Arguments &args) {
    auto yaml_data = YAML::LoadFile(args[0]->get<std::string>());
    return yaml_data.as<nlohmann::json>();
  });
  inja_env.add_callback("aggregate", 1, [&](const inja::Arguments &args) {
    nlohmann::json aggregate;
    auto path = json_pointer(args[0]->get<std::string>());
    // Loop through components, check if object path exists, if so add it to the aggregate
    for (const auto &[c_key, c_value]: project->project_summary["components"].items()) {
      // auto v = json_path(c.value(), path);
      if (!c_value.contains(path) || c_value[path].is_null())
        continue;

      auto v = c_value[path];
      if (v.is_object())
        for (const auto &[i_key, i_value]: v.items()) {
          aggregate[i_key] = i_value; //try_render(inja_env, i.second.as<std::string>(), project->project_summary, log);
        }
      else if (v.is_array())
        for (const auto &[i_key, i_value]: v.items())
          if (i_value.is_object())
            aggregate.push_back(i_value);
          else
            aggregate.push_back(try_render(inja_env, i_value.get<std::string>(), project->project_summary));
      else if (!v.is_null())
        aggregate.push_back(try_render(inja_env, v.get<std::string>(), project->project_summary));
    }

    // Check project data
    if (project->project_summary["data"].contains(path)) {
      auto v = project->project_summary["data"][path];
      if (v.is_object())
        for (const auto &[i_key, i_value]: v.items())
          aggregate[i_key] = i_value;
      else if (v.is_array())
        for (const auto &i: v)
          aggregate.push_back(inja_env.render(i.get<std::string>(), project->project_summary));
      else
        aggregate.push_back(inja_env.render(v.get<std::string>(), project->project_summary));
    }
    return aggregate;
  });
  inja_env.add_callback("quote", 1, [&](const inja::Arguments &args) {
    std::stringstream ss;
    ss << std::quoted(args[0]->get<std::string>());
    return ss.str();
  });

  std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();

  // Note: A blueprint process is a sequence of maps
  for (const auto &command_entry: blueprint->blueprint->process) {
    assert(command_entry.is_object());

    if (command_entry.size() != 1) {
      spdlog::error("Command '{}' for target '{}' is malformed", command_entry.begin().key(), target);
      return { "", -1 };
    }

    // Take the first entry in the map as the command
    auto command                   = command_entry.begin();
    const std::string command_name = command.key();
    int retcode                    = 0;

    try {
      if (project->project_summary["tools"].contains(command_name)) {
        auto tool                = project->project_summary["tools"][command_name];
        std::string command_text = "";

        command_text.append(tool);

        std::string arg_text = command.value().get<std::string>();

        // Apply template engine
        arg_text = try_render(inja_env, arg_text, project->project_summary);

        auto [temp_output, temp_retcode] = exec(command_text, arg_text);
        retcode                          = temp_retcode;

        if (retcode != 0)
          spdlog::error("Returned {}\n{}", retcode, temp_output);
        if (retcode < 0)
          return { temp_output, retcode };

        captured_output = temp_output;
        // Echo the output of the command
        // TODO: Note this should be done by the main thread to ensure the outputs from multiple run_command instances don't overlap
        spdlog::info(captured_output);
      }
      // Else check if it is a built-in command
      else if (project->blueprint_commands.contains(command_name)) {
        yakka::process_return test_result = project->blueprint_commands.at(command_name)(target, command.value(), captured_output, project->project_summary, inja_env);
        captured_output                   = test_result.result;
        retcode                           = test_result.retcode;
      } else {
        spdlog::error("{} tool doesn't exist", command_name);
      }

      if (retcode < 0)
        return { captured_output, retcode };
    } catch (std::exception &e) {
      spdlog::error("Failed to run command: '{}' as part of {}", command_name, target);
      spdlog::error("{}", e.what());
      throw e;
    }
  }

  std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
  auto duration                                     = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
  spdlog::info("{}: {} milliseconds", target, duration);
  return { captured_output, 0 };
}

std::pair<std::string, int> download_resource(const std::string url, fs::path destination)
{
  fs::path filename = destination / url.substr(url.find_last_not_of('/'));
#if defined(_WIN64) || defined(_WIN32) || defined(__CYGWIN__)
  return exec("powershell", "Invoke-WebRequest " + url + " -OutFile " + filename.generic_string());
#else
  return exec("curl", url + " -o " + filename.generic_string());
#endif
}

nlohmann::json::json_pointer create_condition_pointer(const nlohmann::json condition)
{
  nlohmann::json::json_pointer pointer;

  for (const auto &item: condition) {
    pointer /= "/supports/features"_json_pointer;
    pointer /= item.get<std::string>();
  }

  return pointer;
}

bool has_data_dependency_changed(std::string data_path, const nlohmann::json left, const nlohmann::json right)
{
  if (data_path[0] != data_dependency_identifier)
    return false;

  assert(data_path[1] == '/');

  // std::lock_guard<std::mutex> lock(project_lock);
  try {
    if (left.is_null() || left["components"].is_null())
      return true;

    // Check for wildcard or component name
    if (data_path[2] == data_wildcard_identifier) {
      if (data_path[3] != '/') {
        spdlog::error("Data dependency malformed: {}", data_path);
        return false;
      }

      data_path = data_path.substr(3);
      nlohmann::json::json_pointer pointer{ data_path };
      for (const auto &[c_key, c_value]: right["components"].items()) {
        std::string component_name = c_key;
        if (!left["components"].contains(component_name))
          return true;
        auto a = left["components"][component_name].contains(pointer) ? left["components"][component_name][pointer] : nlohmann::json{};
        auto b = right["components"][component_name].contains(pointer) ? right["components"][component_name][pointer] : nlohmann::json{};
        if (a != b) {
          return true;
        }
      }
    } else {
      std::string component_name = data_path.substr(2, data_path.find_first_of('/', 2) - 2);
      data_path                  = data_path.substr(data_path.find_first_of('/', 2));
      nlohmann::json::json_pointer pointer{ data_path };
      if (!left["components"].contains(component_name))
        return true;
      auto a = left["components"][component_name].contains(pointer) ? left["components"][component_name][pointer] : nlohmann::json{};
      auto b = right["components"][component_name].contains(pointer) ? right["components"][component_name][pointer] : nlohmann::json{};
      if (a != b) {
        return true;
      }
    }
    return false;
  } catch (const std::exception &e) {
    spdlog::error("Failed to determine data dependency: {}", e.what());
    return false;
  }
}

} // namespace yakka
