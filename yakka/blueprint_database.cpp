#include "blueprint_database.hpp"
#include "utilities.hpp"
#include "yakka.hpp"
#include "inja.hpp"
#include "glob/glob.h"
#include "spdlog/spdlog.h"
#include <regex>

namespace yakka {
std::vector<std::shared_ptr<blueprint_match>> blueprint_database::find_match(const std::string target, const nlohmann::json &project_summary)
{
  bool blueprint_match_found = false;
  std::vector<std::shared_ptr<blueprint_match>> result;

  for (const auto &blueprint: blueprints) {
    auto match = std::make_shared<blueprint_match>();

    // Check if rule is a regex, otherwise do a string comparison
    if (blueprint.second->regex.has_value()) {
      std::smatch s;
      if (!std::regex_match(target, s, std::regex{ blueprint.first }))
        continue;

      // arg_count starts at 0 as the first match is the entire string
      for (auto &regex_match: s)
        match->regex_matches.push_back(regex_match.str());
    } else {
      if (target != blueprint.first)
        continue;

      match->regex_matches.push_back(target);
    }

    // Found a match. Create a blueprint match object
    blueprint_match_found = true;
    match->blueprint      = blueprint.second;

    inja::Environment local_inja_env;
    local_inja_env.add_callback("$", 1, [&match](const inja::Arguments &args) {
      return match->regex_matches[args[0]->get<int>()];
    });
    local_inja_env.add_callback("curdir", 0, [&match](const inja::Arguments &args) {
      return match->blueprint->parent_path;
    });
    local_inja_env.add_callback("dir", 1, [](inja::Arguments &args) {
      auto path = std::filesystem::path{ args.at(0)->get<std::string>() }.relative_path();
      if (path.has_filename())
        return path.parent_path().string();
      else
        return path.string();
    });
    local_inja_env.add_callback("glob", [](inja::Arguments &args) {
      nlohmann::json aggregate = nlohmann::json::array();
      std::vector<std::string> string_args;
      for (const auto &i: args)
        string_args.push_back(i->get<std::string>());
      for (auto &p: glob::rglob(string_args))
        aggregate.push_back(p.generic_string());
      return aggregate;
    });
    local_inja_env.add_callback("notdir", 1, [](inja::Arguments &args) {
      return std::filesystem::path{ args.at(0)->get<std::string>() }.filename();
    });
    local_inja_env.add_callback("absolute_dir", 1, [](inja::Arguments &args) {
      return std::filesystem::absolute(args.at(0)->get<std::string>());
    });
    local_inja_env.add_callback("extension", 1, [](inja::Arguments &args) {
      return std::filesystem::path{ args.at(0)->get<std::string>() }.extension().string().substr(1);
    });
    local_inja_env.add_callback("render", 1, [&](const inja::Arguments &args) {
      return local_inja_env.render(args[0]->get<std::string>(), project_summary);
    });
    local_inja_env.add_callback("select", 1, [&](const inja::Arguments &args) {
      nlohmann::json choice;
      for (const auto &option: args.at(0)->items()) {
        const auto option_type = option.key();
        const auto option_name = option.value();
        if ((option_type == "feature" && project_summary["features"].contains(option_name)) || (option_type == "component" && project_summary["components"].contains(option_name))) {
          assert(choice.is_null());
          choice = option_name;
        }
      }
      return choice;
    });
    local_inja_env.add_callback("read_file", 1, [&](const inja::Arguments &args) {
      auto file = std::ifstream(args[0]->get<std::string>());
      return std::string{ std::istreambuf_iterator<char>{ file }, {} };
    });
    local_inja_env.add_callback("load_yaml", 1, [&](const inja::Arguments &args) {
      auto yaml_data = YAML::LoadFile(args[0]->get<std::string>());
      return yaml_data.as<nlohmann::json>();
    });
    local_inja_env.add_callback("load_json", 1, [&](const inja::Arguments &args) {
      std::ifstream file_stream(args[0]->get<std::string>());
      return nlohmann::json::parse(file_stream);
    });
    local_inja_env.add_callback("aggregate", 1, [&](const inja::Arguments &args) {
      nlohmann::json aggregate;
      auto path = json_pointer(args[0]->get<std::string>());
      // Loop through components, check if object path exists, if so add it to the aggregate
      for (const auto &[c_key, c_value]: project_summary["components"].items()) {
        // auto v = json_path(c.value(), path);
        if (!c_value.contains(path))
          continue;

        auto v = c_value[path];
        if (v.is_object())
          for (const auto &[i_key, i_value]: v.items())
            aggregate[i_key] = i_value; //local_inja_env.render(i.second.as<std::string>(), this->project_summary);
        else if (v.is_array())
          for (const auto &i: v)
            aggregate.push_back(local_inja_env.render(i.get<std::string>(), project_summary));
        else
          aggregate.push_back(local_inja_env.render(v.get<std::string>(), project_summary));
      }

      // Check project data
      if (project_summary["data"].contains(path)) {
        auto v = project_summary["data"][path];
        if (v.is_object())
          for (const auto &[i_key, i_value]: v.items())
            aggregate[i_key] = i_value;
        else if (v.is_array())
          for (const auto &i: v)
            aggregate.push_back(local_inja_env.render(i.get<std::string>(), project_summary));
        else
          aggregate.push_back(local_inja_env.render(v.get<std::string>(), project_summary));
      }
      return aggregate;
    });

    // Run template engine on dependencies
    for (auto d: blueprint.second->dependencies) {
      switch (d.type) {
        case blueprint::dependency::DEPENDENCY_FILE_DEPENDENCY: {
          const std::string generated_dependency_file = yakka::try_render(local_inja_env, d.name, project_summary);
          auto dependencies                           = parse_gcc_dependency_file(generated_dependency_file);
          match->dependencies.insert(std::end(match->dependencies), std::begin(dependencies), std::end(dependencies));
          continue;
        }
        case blueprint::dependency::DATA_DEPENDENCY: {
          std::string data_name = yakka::try_render(local_inja_env, d.name, project_summary);
          if (data_name.front() != yakka::data_dependency_identifier)
            data_name.insert(0, 1, yakka::data_dependency_identifier);
          match->dependencies.push_back(data_name);
          continue;
        }
        default:
          break;
      }

      // Generate full dependency string by applying template engine
      std::string generated_depend;
      try {
        generated_depend = local_inja_env.render(d.name, project_summary);
      } catch (std::exception &e) {
        spdlog::error("Error evaluating dependency for {}\r\nCouldn't apply template: '{}'\n{}", blueprint.first, d.name, e.what());
        return result;
      }

      // Check if the input was a YAML array construct
      if (generated_depend.front() == '[' && generated_depend.back() == ']') {
        // Load the generated dependency string as YAML and push each item individually
        try {
          auto generated_node = YAML::Load(generated_depend);
          for (auto i: generated_node) {
            auto temp = i.Scalar();
            match->dependencies.push_back(temp.starts_with("./") ? temp.substr(2) : temp);
          }
        } catch (std::exception &e) {
          std::cerr << "Failed to parse dependency: " << d.name << "\n";
        }
      } else {
        match->dependencies.push_back(generated_depend.starts_with("./") ? generated_depend.substr(2) : generated_depend);
      }
    }

    result.push_back(match);
    // return match;
  }

  if (!blueprint_match_found) {
    if (!fs::exists(target))
      spdlog::info("No blueprint for '{}'", target);
  }
  return result;
}
} // namespace yakka
