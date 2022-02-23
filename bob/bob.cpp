#include "bob.hpp"
#include "bob_project.hpp"
#include "bob_workspace.hpp"
#include "cxxopts.hpp"
#include "subprocess.hpp"
#include "spdlog/spdlog.h"
#include <indicators/dynamic_progress.hpp>
#include <indicators/progress_bar.hpp>
#include <indicators/cursor_control.hpp>
#include <indicators/termcolor.hpp>
#include "taskflow.hpp"
#include <iostream>
#include <fstream>
#include <chrono>
#include <future>

using namespace indicators;
using namespace std::chrono_literals;

tf::Task& create_tasks(bob::project& project, const std::string& name, std::map<std::string, tf::Task>& tasks, tf::Taskflow& taskflow);

int main(int argc, char **argv)
{
    auto bob_start_time = fs::file_time_type::clock::now();

    // Setup logging
    std::error_code error_code;
    fs::remove("bob.log", error_code);

    auto console = spdlog::stderr_color_mt("bobconsole");
    console->flush_on(spdlog::level::level_enum::off);
    console->set_pattern("[%^%l%$]: %v");
    //spdlog::set_async_mode(4096);
    auto boblog = spdlog::basic_logger_mt("boblog", "bob.log");

    cxxopts::Options options("bob", "BOB the universal builder");
    options.positional_help("<action> [optional args]");
    options.add_options()
        ("h,help", "Print usage")
        ("l,list", "List known components", cxxopts::value<bool>()->default_value("false"))
        ("r,refresh", "Refresh component database", cxxopts::value<bool>()->default_value("false"))
        ("f,fetch", "Fetch missing components", cxxopts::value<bool>()->default_value("false"))
        ("action",  "action", cxxopts::value<std::string>());

    options.parse_positional({"action"});
    auto result = options.parse(argc, argv);
    if (result.count("help") || argc == 1)
    {
        std::cout << options.help() << std::endl;
        return 0;
    }
    if (result["refresh"].as<bool>())
    {
        fs::remove(bob::component_database::database_filename);

        std::cout << "Scanning '.' for components" << std::endl;
        bob::component_database db;
        db.save();
        std::cout << "Scan complete. " << bob::component_database::database_filename << " has been updated" << std::endl;
    }
    if (result["list"].as<bool>())
    {
        bob::component_database db;
        for (const auto& c: db)
        {
            std::cout << c.first << ":\n  " << c.second << std::endl;
        }
    }
    if (result["fetch"].as<bool>())
    {
      // Ensure the BOB repos directory exists
      fs::create_directory(".bob/repos");
    }

    if (!result.count("action"))
        return 0;

    auto t1 = std::chrono::high_resolution_clock::now();

    auto [components, features, commands] = bob::parse_arguments(result.unmatched());

    // Create a workspace
    bob::workspace workspace;

    // Create a project
    bob::project project(boblog);

    // Move the CLI parsed data to the project
    project.unprocessed_components = std::move(components);
    project.unprocessed_features = std::move(features);
    project.commands = std::move(commands);

    if (result["action"].as<std::string>() == "build")
    {
        std::cout << "Building projects not supported yet\n";
        // Identify the project to build
        // There should only be one component. This is the particular project to build.
        // The user can add features to "flavour" the build
        return 0;
    }
    else
    {
        // The action, if not one of the built-in actions, is interpreted as a command to be processed by a blueprint
        project.commands.insert(result["action"].as<std::string>());

        // Init the project
        project.init_project();
    }

    if (project.evaluate_dependencies() != bob::project::state::PROJECT_VALID)
        return 1;

    // Check if the project depends on components we don't have yet 
    if (!project.unknown_components.empty())
    {
        workspace.load_component_registries();

        show_console_cursor(false);
        DynamicProgress<ProgressBar> fetch_progress_ui;
        std::vector<std::shared_ptr<ProgressBar>> fetch_progress_bars;

        std::map<std::string, std::future<void>> fetch_list;
        do
        {
            // Ask the workspace to fetch them
            for (const auto& i: project.unknown_components)
            {
                if (fetch_list.find(i) != fetch_list.end())
                    continue;
                
                // Check if component is in the registry
                auto node = workspace.find_registry_component(i);
                if (node)
                {
                    std::shared_ptr<ProgressBar> new_progress_bar = std::make_shared<ProgressBar>(option::BarWidth{ 50 }, option::ShowPercentage{ true }, option::PrefixText{ "Fetching " + i + " " }, option::SavedStartTime{ true });
                    fetch_progress_bars.push_back(new_progress_bar);
                    size_t id = fetch_progress_ui.push_back(*new_progress_bar);
                    fetch_progress_ui.print_progress();
                    auto result = workspace.fetch_component(i, *node, [&fetch_progress_ui,id](size_t number) {
                            if (number >= 100)
                            {
                                fetch_progress_ui[id].set_progress(100);
                                fetch_progress_ui[id].mark_as_completed();
                            }
                            else
                                fetch_progress_ui[id].set_progress(number);
                        });
                    if (result.valid())
                        fetch_list.insert({i, std::move(result)});
                }
            }

            // Check if we haven't been able to fetch any of the unknown components
            if (fetch_list.empty())
            {
                for (const auto& i: project.unknown_components)
                    console->error("Cannot fetch {}", i);
                return 0;
            }

            // Wait for one of the components to be complete
            decltype(fetch_list)::iterator completed_fetch;
            do {
                completed_fetch = std::find_if(fetch_list.begin(), fetch_list.end(), [](auto& fetch_item){ return fetch_item.second.wait_for(100ms) == std::future_status::ready; } );
            } while (completed_fetch == fetch_list.end());

            // Update the component database
            project.component_database.scan_for_components("./components/" + completed_fetch->first);

            // Check if any of our unknown components have been found
            for (auto i = project.unknown_components.cbegin(); i != project.unknown_components.cend();)
            {
                if (project.component_database[*i])
                {
                    // Remove component from the unknown list and add it to the unprocessed list
                    project.unprocessed_components.insert(*i);
                    i = project.unknown_components.erase(i);
                }
                else
                    ++i;
            }

            // Remove the item from the fetch list
            fetch_list.erase(completed_fetch);

            // Re-evaluate the project dependencies
            project.evaluate_dependencies();
        } while(!project.unprocessed_components.empty() || !project.unknown_components.empty( ) || !fetch_list.empty());
    }

    auto t2 = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
    boblog->info("{}ms to process components", duration);

    boblog->info("Required features:");
    for (auto f: project.required_features)
        boblog->info("- {}", f);

    project.generate_project_summary();
    project.save_summary();

    t1 = std::chrono::high_resolution_clock::now();
    project.parse_blueprints();
    project.generate_target_database();
    t2 = std::chrono::high_resolution_clock::now();

    duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
    boblog->info("{}ms to process blueprints", duration);

    project.load_common_commands();

    // Task flow test
    project.work_task_count = 0;
    std::atomic<int> execution_progress = 0;
    tf::Executor executor;
    auto finish = project.taskflow.emplace([&]() { execution_progress = 100; } );
    for (auto& i: project.commands)
        project.create_tasks(i, finish);

    
    ProgressBar building_bar {
        option::BarWidth{ 50 },
        option::ShowPercentage{ true },
        option::PrefixText{ "Building " },
        option::MaxProgress{project.work_task_count}
    };

    project.task_complete_handler = [&]() {
        ++execution_progress;
    };
     
    auto execution_future = executor.run(project.taskflow);
    do
    {
        building_bar.set_option(option::PostfixText{
            std::to_string(execution_progress) + "/" + std::to_string(project.work_task_count)
        });
        building_bar.set_progress(execution_progress);
    } while (execution_future.wait_for(100ms) != std::future_status::ready);

    building_bar.set_progress(project.work_task_count);

    auto bob_end_time = fs::file_time_type::clock::now();
    std::cout << "Complete in " << std::chrono::duration_cast<std::chrono::milliseconds>(bob_end_time - bob_start_time).count() << " milliseconds" << std::endl;

#if 0

    project.load_common_commands();

    if (project.todo_list.size() != 0)
    {
        ProgressBar building_bar {
            option::BarWidth{ 50 },
            option::ShowPercentage{ true },
            option::PrefixText{ "Building " }
        };

        // auto construction_start_time = fs::file_time_type::clock::now();

        project.process_construction(building_bar);

        // auto construction_end_time = fs::file_time_type::clock::now();

        // std::cout << "Building complete in " << std::chrono::duration_cast<std::chrono::milliseconds>(construction_end_time - construction_start_time).count() << " milliseconds" << std::endl;
    }

    auto bob_end_time = fs::file_time_type::clock::now();
    std::cout << "Complete in " << std::chrono::duration_cast<std::chrono::milliseconds>(bob_end_time - bob_start_time).count() << " milliseconds" << std::endl;
#endif

    console->flush();
    show_console_cursor(true);
    return 0;
}
