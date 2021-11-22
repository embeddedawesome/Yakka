#include "bob.h"
#include "bob_project.h"
#include "bob_workspace.h"
#include "cxxopts.hpp"
#include "subprocess.hpp"
#include "spdlog/spdlog.h"
#include <indicators/dynamic_progress.hpp>
#include <indicators/progress_bar.hpp>
#include <indicators/cursor_control.hpp>
#include <indicators/termcolor.hpp>
#include <iostream>
#include <fstream>
#include <chrono>
#include <future>

using namespace indicators;
using namespace std::chrono_literals;

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

    project.evaluate_dependencies();

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

    project.evaluate_blueprint_dependencies();

    t2 = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
    boblog->info("{}ms to process blueprints", duration);

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

    console->flush();
    show_console_cursor(true);
    return 0;
}

namespace bob {

std::pair<std::string, int> exec( const std::string& command_text, const std::string& arg_text)
{
    auto boblog = spdlog::get("boblog");
    boblog->info("{} {}", command_text, arg_text);
    try {
        std::string command = command_text;
        if (!arg_text.empty())
            command += " " + arg_text;
        #if defined(__USING_WINDOWS__)
        auto p = subprocess::Popen(command, subprocess::output{subprocess::PIPE}, subprocess::error{subprocess::STDOUT} );
        #else
        auto p = subprocess::Popen(command, subprocess::shell{true}, subprocess::output{subprocess::PIPE}, subprocess::error{subprocess::STDOUT} );
        #endif
        auto output = p.output();
        std::string result;
        if (output != nullptr)
        {
            while(1) {
                int c = fgetc(output);
                if (c == EOF)
                    break;
                result += (char)c;
            };
        }

        p.wait();
        p.poll();
        return {result,p.retcode()};
    } catch (std::exception e)
    {
        boblog->error("Exception while executing: {}\n{}", command_text, e.what());
        return {"", -1};
    }
}

void exec( const std::string& command_text, const std::string& arg_text, std::function<void(std::string&)> function)
{
    auto boblog = spdlog::get("boblog");
    boblog->info("{} {}", command_text, arg_text);
    try {
        std::string command = command_text;
        if (!arg_text.empty())
            command += " " + arg_text;
        #if defined(__USING_WINDOWS__)
        auto p = subprocess::Popen(command, subprocess::output{subprocess::PIPE}, subprocess::error{subprocess::STDOUT} );
        #else
        auto p = subprocess::Popen(command, subprocess::shell{true}, subprocess::output{subprocess::PIPE}, subprocess::error{subprocess::STDOUT} );
        #endif
        auto output = p.output();
        std::array<char, 512> buffer;
        size_t count = 0;
        if (output != nullptr)
        {
            while(1) {
                buffer[count] = fgetc(output);
                if ( feof(output) )
                    break;

                if (count == buffer.size()-1 || buffer[count] == '\n' )
                {
                    std::string temp(buffer.data());
                    function(temp);
                    count = 0;
                }
                else
                    ++count;
            };
        }
        p.wait();
        p.poll();
    } catch (std::exception e)
    {
        boblog->error("Exception while executing: {}\n{}", command_text, e.what());
    }
}

bool yaml_diff(const YAML::Node& node1, const YAML::Node& node2)
{
    std::vector<std::pair<YAML::Node, YAML::Node>> compare_list;
    compare_list.push_back({node1, node2});
    for (int i = 0; i < compare_list.size(); ++i)
    {
        YAML::Node& left  = compare_list[i].first;
        YAML::Node& right = compare_list[i].second;

        if (left.Type() != right.Type())
            return true;

        switch(left.Type())
        {
            case YAML::NodeType::Scalar:
                if (left.Scalar() != right.Scalar())
                  return true;
                break;

            case YAML::NodeType::Sequence:
                // Verify the sequences have the same length
                if (left.size() != right.size())
                    return true;
                for (int a=0; a < left.size(); ++a)
                    compare_list.push_back({left[a], right[a]});
                break;

            case YAML::NodeType::Map:
                // Verify the maps have the same length
                if (left.size() != right.size())
                    return true;
                for (const auto& a: left)
                {
                    auto& key = a.first.Scalar();
                    auto& node = a.second;
                    if (!right[key])
                      return true;
                    compare_list.push_back({node, right[key]});
                }
                break;
            default:
                break;
        }
    }

    return false;
}

YAML::Node yaml_path(const YAML::Node& node, std::string path)
{
    YAML::Node temp = node;
    std::stringstream ss(path);
    std::string s;
    while (std::getline(ss, s, '.'))
        temp.reset(temp[s]);
    return temp;
}

std::tuple<component_list_t, feature_list_t, command_list_t> parse_arguments( const std::vector<std::string>& argument_string )
{
    component_list_t components;
    feature_list_t features;
    command_list_t commands;

//    for (auto s = argument_string.begin(); s != argument_string.end(); ++s)
    for (auto s: argument_string)
    {
        // Identify features, commands, and components
        if (s.front() == '+')
            features.insert(s.substr(1));
        else if (s.back() == '!')
            commands.insert(s.substr(0, s.size() - 1));
        else
            components.insert(s);
    }

    return {std::move(components), std::move(features), std::move(commands)};
}

std::string generate_project_name(const component_list_t& components, const feature_list_t& features)
{
    std::string project_name = "";

    // Generate the project name from the project string
    for (const auto &i : components)
        project_name += i + "-";

    if (!components.empty())
        project_name.pop_back();

    for (const auto &i : features)
        project_name += "-" + i;

    if (project_name.empty())
        project_name = "none";

    return project_name;
}
}
