#include "yakka.hpp"
#include "yakka_workspace.hpp"
#include "yakka_project.hpp"
#include "utilities.hpp"
#include "cxxopts.hpp"
#include "subprocess.hpp"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "semver.hpp"
#include <indicators/dynamic_progress.hpp>
#include <indicators/progress_bar.hpp>
#include <indicators/cursor_control.hpp>
#include <indicators/termcolor.hpp>
#include "taskflow.hpp"
#include <iostream>
#include <fstream>
#include <chrono>
#include <future>
#include <algorithm>

using namespace indicators;
using namespace std::chrono_literals;

static void evaluate_project_dependencies(yakka::workspace &workspace, yakka::project &project);
static void download_unknown_components(yakka::workspace &workspace, yakka::project &project);
static void print_project_choice_errors(yakka::project &project);
static void run_taskflow(yakka::project &project);

tf::Task &create_tasks(yakka::project &project, const std::string &name, std::map<std::string, tf::Task> &tasks, tf::Taskflow &taskflow);
static const semver::version yakka_version{
#include "yakka_version.h"
};

int main(int argc, char **argv)
{
  auto yakka_start_time = fs::file_time_type::clock::now();

  // Setup logging
  std::error_code error_code;
  fs::remove("yakka.log", error_code);

  auto console = spdlog::stdout_color_mt("console");

  auto console_error = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
  console_error->set_level(spdlog::level::warn);
  console_error->set_pattern("[%^%l%$]: %v");
  std::shared_ptr<spdlog::sinks::basic_file_sink_mt> file_log;
  try {
    file_log = std::make_shared<spdlog::sinks::basic_file_sink_mt>("yakka.log", true);
  } catch (...) {
    try {
      auto time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
      file_log  = std::make_shared<spdlog::sinks::basic_file_sink_mt>("yakka-" + std::to_string(time) + ".log", true);
    } catch (...) {
      std::cerr << "Cannot open yakka.log";
      exit(1);
    }
  }
  file_log->set_level(spdlog::level::trace);

  auto yakkalog = std::make_shared<spdlog::logger>("yakkalog", spdlog::sinks_init_list{ console_error, file_log });
  spdlog::set_default_logger(yakkalog);

  // Create a workspace
  yakka::workspace workspace;
  workspace.init(".");

  cxxopts::Options options("yakka", "Yakka the embedded builder. Ver " + yakka_version.to_string());
  options.allow_unrecognised_options();
  options.positional_help("<action> [optional args]");
  // clang-format off
  options.add_options()("h,help", "Print usage")("r,refresh", "Refresh component database", cxxopts::value<bool>()->default_value("false"))
                       ("n,no-eval", "Skip the dependency and choice evaluation", cxxopts::value<bool>()->default_value("false"))
                       ("i,ignore-eval", "Ignore dependency and choice evaluation errors", cxxopts::value<bool>()->default_value("false"))
                       ("o,no-output", "Do not generate output folder", cxxopts::value<bool>()->default_value("false"))
                       ("f,fetch", "Automatically fetch missing components", cxxopts::value<bool>()->default_value("false"))
                       ("p,project-name", "Set the project name", cxxopts::value<std::string>()->default_value(""))
                       ("w,with", "Additional SLC feature", cxxopts::value<std::vector<std::string>>())
                       ("d,data", "Additional data", cxxopts::value<std::string>())
                       ("no-slcc", "Ignore SLC files", cxxopts::value<bool>()->default_value("false"))
                       ("no-yakka", "Ignore Yakka files", cxxopts::value<bool>()->default_value("false"))
                       ("action", "Select from 'register', 'list', 'update', 'git', 'remove' or a command", cxxopts::value<std::string>());
  // clang-format on

  options.parse_positional({ "action" });
  auto result = options.parse(argc, argv);
  if (result.count("help") || argc == 1) {
    std::cout << options.help() << std::endl;
    return 0;
  }
  if (result["refresh"].as<bool>()) {
    workspace.local_database.erase();
    workspace.local_database.clear();

    std::cout << "Scanning '.' for components\n";
    workspace.local_database.scan_for_components();
    workspace.local_database.save();
    std::cout << "Scan complete.\n";
  }

  // Check if there is no action. If so, print the help
  if (!result.count("action")) {
    std::cout << options.help() << std::endl;
    return 0;
  }

  auto action = result["action"].as<std::string>();
  if (action == "register") {
    if (result.unmatched().size() == 0) {
      spdlog::error("Must provide URL of component registry");
      return -1;
    }
    // Ensure the BOB registries directory exists
    fs::create_directories(".yakka/registries");
    spdlog::info("Adding component registry...");
    if (workspace.add_component_registry(result.unmatched()[0]) != yakka::yakka_status::SUCCESS) {
      spdlog::error("Failed to add component registry. See yakka.log for details");
      return -1;
    }
    spdlog::info("Complete");
    return 0;
  } else if (action == "list") {
    workspace.load_component_registries();
    for (auto registry: workspace.registries) {
      std::cout << registry.second["name"] << "\n";
      for (auto c: registry.second["provides"]["components"])
        std::cout << "  - " << c.first << "\n";
    }
    return 0;
  } else if (action == "update") {
    // Find all the component repos in .yakka
    //for (auto d: fs::directory_iterator(".yakka/repos"))
    for (auto &i: result.unmatched()) {
      // const auto name = d.path().filename().generic_string();
      std::cout << "Updating: " << i << "\n";
      workspace.update_component(i);
    }

    std::cout << "Complete\n";
    return 0;
  } else if (action == "remove") {
    // Find all the component repos in .yakka
    for (auto &i: result.unmatched()) {
      auto optional_location = workspace.find_component(i);
      if (optional_location) {
        auto [path, package] = optional_location.value();
        spdlog::info("Removing {}", path.string());
        fs::remove_all(path);
      }
    }

    std::cout << "Complete\n";
    return 0;
  } else if (action == "git") {
    auto iter                 = result.unmatched().begin();
    const auto component_name = *iter;
    std::string git_command   = "--git-dir=.yakka/repos/" + component_name + "/.git --work-tree=components/" + component_name;
    for (iter++; iter != result.unmatched().end(); ++iter)
      if (iter->find(' ') == std::string::npos)
        git_command.append(" " + *iter);
      else
        git_command.append(" \"" + *iter + "\"");

    auto [output, result] = yakka::exec("git", git_command);
    std::cout << output;
    return 0;
  } else if (action.back() != '!') {
    std::cout << "Must provide an action or a command (commands end with !)\n";
    return 0;
  }

  // Action must be a command. Drop the !
  action.pop_back();

  // Process the command line options
  std::string project_name;
  std::string feature_suffix;
  std::vector<std::string> components;
  std::vector<std::string> features;
  std::unordered_set<std::string> commands;
  for (auto s: result.unmatched()) {
    // Identify features, commands, and components
    if (s.front() == '+') {
      feature_suffix += s;
      features.push_back(s.substr(1));
    } else if (s.back() == '!')
      commands.insert(s.substr(0, s.size() - 1));
    else {
      components.push_back(s);

      // Compose the project name by concatenation all the components in CLI order.
      // The features will be added at the end
      project_name += s + "-";
    }
  }

  if (components.size() == 0) {
    spdlog::error("No components identified");
    return -1;
  }

  // Remove the extra "-" and add the feature suffix
  project_name.pop_back();
  project_name += feature_suffix;

  auto cli_set_project_name = result["project-name"].as<std::string>();
  if (!cli_set_project_name.empty())
    project_name = cli_set_project_name;

  // Create a project and output
  yakka::project project(project_name, workspace);

  // Move the CLI parsed data to the project
  // project.unprocessed_components = std::move(components);
  // project.unprocessed_features = std::move(features);
  project.commands = std::move(commands);

  // Add the action as a command
  project.commands.insert(action);

  // Init the project
  project.init_project(components, features);

  // Check if we don't want Yakka files
  if (result["no-yakka"].count() != 0) {
    project.component_flags = yakka::component_database::flag::IGNORE_YAKKA;
  }

  // Check if SLC needs to be supported
  if (result["no-slcc"].count() != 0) {
    project.component_flags = yakka::component_database::flag::IGNORE_SLCC;
  } else {
    // Add SLC features
    if (result["with"].count() != 0) {
      const auto slc_features = result["with"].as<std::vector<std::string>>();
      for (const auto &f: slc_features)
        project.slc_required.insert(f);
    }
  }

  if (!result["no-eval"].as<bool>()) {
    evaluate_project_dependencies(workspace, project);

    if (!project.unknown_components.empty()) {
      if (result["fetch"].as<bool>()) {
        download_unknown_components(workspace, project);
      } else {
        for (const auto &i: project.unknown_components)
          spdlog::error("Missing component '{}'", i);
        spdlog::error("Try adding the '-f' command line option to automatically fetch components");
        spdlog::shutdown();
        exit(0);
      }
    }

    project.evaluate_choices();
    if (!result["ignore-eval"].as<bool>() && (!project.incomplete_choices.empty() || !project.multiple_answer_choices.empty()))
      print_project_choice_errors(project);
  } else {
    spdlog::info("Skipping project evalutaion");

    for (const auto &i: components) {
      // Convert string to id
      const auto component_id = yakka::component_dotname_to_id(i);
      // Find the component in the project component database
      auto component_location = workspace.find_component(component_id, project.component_flags);
      if (!component_location) {
        // log->info("{}: Couldn't find it", c);
        continue;
      }

      // Add component to the required list and continue if this is not a new component
      // Insert component and continue if this is not new
      if (project.required_components.insert(component_id).second == false)
        continue;

      auto [component_path, package_path]             = component_location.value();
      std::shared_ptr<yakka::component> new_component = std::make_shared<yakka::component>();
      if (new_component->parse_file(component_path, package_path) == yakka::yakka_status::SUCCESS) {
        project.components.push_back(new_component);
      } else {
        if (!result["ignore-eval"].as<bool>()) {
          spdlog::error("Failed to parse {}", component_path.generic_string());
          exit(-1);
        }
      }
    }
  }

  if (result["no-slcc"].count() == 0)
    project.process_slc_rules();

  spdlog::info("Required features:");
  for (auto f: project.required_features)
    spdlog::info("- {}", f);

  project.generate_project_summary();
  project.save_summary();

  auto t1 = std::chrono::high_resolution_clock::now();
  project.validate_schema();
  auto t2       = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
  spdlog::info("{}ms to validate schemas", duration);

  if (project.current_state != yakka::project::state::PROJECT_VALID)
    exit(-1);

  // Insert additional command line data before processing blueprints
  if (result["data"].count() != 0) {
    spdlog::info("Processing additional data: {}", result["data"].as<std::string>());
    const auto additional_data = "{" + result["data"].as<std::string>() + "}";
    YAML::Node yaml_data       = YAML::Load(additional_data);
    nlohmann::json json_data   = yaml_data.as<nlohmann::json>();
    spdlog::info("Additional data: {}", json_data.dump());
    yakka::json_node_merge(project.project_summary["data"], json_data);
  }

  t1 = std::chrono::high_resolution_clock::now();
  project.parse_blueprints();
  project.generate_target_database();
  t2 = std::chrono::high_resolution_clock::now();

  duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
  spdlog::info("{}ms to process blueprints", duration);
  project.load_common_commands();

  run_taskflow(project);

  auto yakka_end_time = fs::file_time_type::clock::now();
  std::cout << "Complete in " << std::chrono::duration_cast<std::chrono::milliseconds>(yakka_end_time - yakka_start_time).count() << " milliseconds" << std::endl;

  spdlog::shutdown();
  show_console_cursor(true);

  if (project.abort_build)
    return -1;
  else
    return 0;
}

void run_taskflow(yakka::project &project)
{
  project.work_task_count             = 0;
  std::atomic<int> execution_progress = 0;
  int last_progress_update            = 0;
  tf::Executor executor(std::min(32U, std::thread::hardware_concurrency()));
  auto finish = project.taskflow.emplace([&]() {
    execution_progress = 100;
  });
  for (auto &i: project.commands)
    project.create_tasks(i, finish);

  ProgressBar building_bar{ option::BarWidth{ 50 }, option::ShowPercentage{ true }, option::PrefixText{ "Building " }, option::MaxProgress{ project.work_task_count } };

  project.task_complete_handler = [&]() {
    ++execution_progress;
  };

  auto execution_future = executor.run(project.taskflow);

  do {
    if (execution_progress != last_progress_update) {
      building_bar.set_option(option::PostfixText{ std::to_string(execution_progress) + "/" + std::to_string(project.work_task_count) });
      building_bar.set_progress(execution_progress);
      last_progress_update = execution_progress;
    }
  } while (execution_future.wait_for(500ms) != std::future_status::ready);

  building_bar.set_option(option::PostfixText{ std::to_string(project.work_task_count) + "/" + std::to_string(project.work_task_count) });
  building_bar.set_progress(project.work_task_count);
}

static void download_unknown_components(yakka::workspace &workspace, yakka::project &project)
{
  auto t1 = std::chrono::high_resolution_clock::now();

  // If there are still missing components, try and download them
  if (!project.unknown_components.empty()) {
    workspace.load_component_registries();

    show_console_cursor(false);
    DynamicProgress<ProgressBar> fetch_progress_ui;
    std::vector<std::shared_ptr<ProgressBar>> fetch_progress_bars;

    std::map<std::string, std::future<fs::path>> fetch_list;
    do {
      // Ask the workspace to fetch them
      for (const auto &i: project.unknown_components) {
        if (fetch_list.find(i) != fetch_list.end())
          continue;

        // Check if component is in the registry
        auto node = workspace.find_registry_component(i);
        if (node) {
          std::shared_ptr<ProgressBar> new_progress_bar = std::make_shared<ProgressBar>(option::BarWidth{ 50 }, option::ShowPercentage{ true }, option::PrefixText{ "Fetching " + i + " " }, option::SavedStartTime{ true });
          fetch_progress_bars.push_back(new_progress_bar);
          size_t id = fetch_progress_ui.push_back(*new_progress_bar);
          fetch_progress_ui.print_progress();
          auto result = workspace.fetch_component(i, *node, [&fetch_progress_ui, id](std::string prefix, size_t number) {
            fetch_progress_ui[id].set_option(option::PrefixText{ prefix });
            if (number >= 100) {
              fetch_progress_ui[id].set_progress(100);
              fetch_progress_ui[id].mark_as_completed();
            } else
              fetch_progress_ui[id].set_progress(number);
          });
          if (result.valid())
            fetch_list.insert({ i, std::move(result) });
        }
      }

      // Check if we haven't been able to fetch any of the unknown components
      if (fetch_list.empty()) {
        for (const auto &i: project.unknown_components)
          spdlog::error("Cannot fetch {}", i);
        spdlog::shutdown();
        exit(0);
      }

      // Wait for one of the components to be complete
      decltype(fetch_list)::iterator completed_fetch;
      do {
        completed_fetch = std::find_if(fetch_list.begin(), fetch_list.end(), [](auto &fetch_item) {
          return fetch_item.second.wait_for(100ms) == std::future_status::ready;
        });
      } while (completed_fetch == fetch_list.end());

      auto new_component_path = completed_fetch->second.get();

      // Check if the fetch worked
      if (new_component_path.empty()) {
        spdlog::error("Failed to fetch {}", completed_fetch->first);
        project.unknown_components.erase(completed_fetch->first);
        fetch_list.erase(completed_fetch);
        continue;
      }

      // Update the component database
      if (new_component_path.string().starts_with(workspace.shared_components_path.string())) {
        spdlog::info("Scanning for new component in shared database");
        workspace.shared_database.scan_for_components(new_component_path);
        workspace.shared_database.save();
      } else {
        spdlog::info("Scanning for new component in local database");
        workspace.local_database.scan_for_components(new_component_path);
        workspace.shared_database.save();
      }

      // Check if any of our unknown components have been found
      for (auto i = project.unknown_components.cbegin(); i != project.unknown_components.cend();) {
        if (!workspace.local_database.get_component(*i, project.component_flags).empty() || !workspace.shared_database.get_component(*i, project.component_flags).empty()) {
          // Remove component from the unknown list and add it to the unprocessed list
          project.unprocessed_components.insert(*i);
          i = project.unknown_components.erase(i);
        } else
          ++i;
      }

      // Remove the item from the fetch list
      fetch_list.erase(completed_fetch);

      // Re-evaluate the project dependencies
      project.evaluate_dependencies();
    } while (!project.unprocessed_components.empty() || !project.unknown_components.empty() || !fetch_list.empty());
  }

  auto t2       = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
  spdlog::info("{}ms to download missing components", duration);
}

static void evaluate_project_dependencies(yakka::workspace &workspace, yakka::project &project)
{
  auto t1 = std::chrono::high_resolution_clock::now();

  if (project.evaluate_dependencies() == yakka::project::state::PROJECT_HAS_INVALID_COMPONENT)
    exit(1);

  // If we're missing a component, update the component database and try again
  if (!project.unknown_components.empty()) {
    spdlog::info("Scanning workspace to find missing components");
    workspace.local_database.scan_for_components();
    workspace.shared_database.scan_for_components();
    project.unprocessed_components.swap(project.unknown_components);
    project.evaluate_dependencies();
  }

  auto t2       = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
  spdlog::info("{}ms to process components", duration);
}

static void print_project_choice_errors(yakka::project &project)
{
  for (auto &i: project.incomplete_choices) {
    bool valid_options = false;
    spdlog::error("Component '{}' has a choice '{}' - Must choose from the following", i.first, i.second);
    if (project.project_summary["choices"][i.second].contains("features")) {
      valid_options = true;
      spdlog::error("Features: ");
      for (auto &b: project.project_summary["choices"][i.second]["features"])
        spdlog::error("  - {}", b.get<std::string>());
    }

    if (project.project_summary["choices"][i.second].contains("components")) {
      valid_options = true;
      spdlog::error("Components: ");
      for (auto &b: project.project_summary["choices"][i.second]["components"])
        spdlog::error("  - {}", b.get<std::string>());
    }

    if (!valid_options) {
      spdlog::error("ERROR: Choice data is invalid");
    }
    project.current_state = yakka::project::state::PROJECT_HAS_INCOMPLETE_CHOICES;
  }

  for (auto a: project.multiple_answer_choices) {
    spdlog::error("Choice {} - Has multiple selections", a);
    project.current_state = yakka::project::state::PROJECT_HAS_MULTIPLE_ANSWERS_FOR_CHOICES;
  }
}
