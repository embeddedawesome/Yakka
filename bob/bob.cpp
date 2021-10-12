#include "bob.h"
#include "bob_project.h"
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

using namespace indicators;
using namespace std::chrono_literals;

int main(int argc, char **argv)
{
    auto bob_start_time = std::time(nullptr);

    // Setup logging
    fs::remove("bob.log");
    auto console = spdlog::stderr_color_mt("bobconsole");
    console->flush_on(spdlog::level::level_enum::off);
    console->set_pattern("[%^%l%$]: %v");
    //spdlog::set_async_mode(4096);
    auto boblog = spdlog::basic_logger_mt("boblog", "bob.log");

    cxxopts::Options options("bob", "BOB the universal builder");
    options.add_options()
        ("h,help", "Print usage")
        ("l,list", "List known components", cxxopts::value<bool>()->default_value("false"))
        ("r,refresh", "Refresh component database", cxxopts::value<bool>()->default_value("false"))
        ("f,fetch", "Fetch missing components", cxxopts::value<bool>()->default_value("false"));

    auto result = options.parse(argc, argv);
    if (result.count("help") || argc == 1)
    {
        std::cout << options.help() << std::endl;
        exit(0);
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

    if (result.unmatched().empty())
        exit(0);

    auto t1 = std::chrono::high_resolution_clock::now();


    show_console_cursor(false);
    DynamicProgress<ProgressBar> fetch_progress_ui;
    std::vector<std::shared_ptr<ProgressBar>> fetch_progress_bars;

    bob::project project(result.unmatched(), boblog);

    project.load_component_registries();


    // The following code needs to be cleaned up
    std::map<std::string, std::future<void>> git_fetching_list;
    do {
        for (const auto& c: project.unknown_components)
            project.unprocessed_components.push_back(c);
        project.unknown_components.clear();

        project.evaluate_dependencies();
        int found_components = 0;

        if ( project.unknown_components.size( ) != 0 )
        {
            // Check whether any of the unknown components are in registries.
            for ( auto r : project.registries )
            {
                for (const auto& c: project.unknown_components)
                {
                    // Check if it's already being fetched
                    // boblog->info( "Checking for {}", c);
                    if (git_fetching_list.find(c) != git_fetching_list.end()) {
                    //   boblog->info("Already being fetched");
                      continue;
                    }

                    if ( r.second["provides"]["components"][c].IsDefined( ) )
                    {
                        if (result["fetch"].as<bool>())
                        {
                            ++found_components;
                            boblog->info( "Fetching new component: {}", c);
                            fetch_progress_bars.push_back(std::make_shared<ProgressBar>(option::BarWidth{50}, option::ShowPercentage{true}, option::PrefixText{"Fetching " + c + " "}, option::SavedStartTime{true}));
                            auto id = fetch_progress_ui.push_back(*fetch_progress_bars.back());
                            // auto id = bars.push_back(ProgressBar{option::BarWidth{50}, option::ShowPercentage{true}, option::PrefixText{"Fetching " + c + " "}, option::SavedStartTime{true}});
                            fetch_progress_ui.print_progress();
                            git_fetching_list[c] = std::async(std::launch::async, [&](size_t bar_id){
                                std::string name = c; // Make a copy of the name string
                                bob::fetch_component(name, r.second["provides"]["components"][name], [&](size_t number) {fetch_progress_ui[bar_id].set_progress(number);});
                                fetch_progress_ui[bar_id].mark_as_completed();
                                // Wait for the operating system to make files appear.
                                // Occasionally Git checkout is complete but the files don't "exist"
                                // std::this_thread::sleep_for(1000ms);
                            }, id);
                        }
                        else
                            console->error( "Component '{}' can be fetched from the '{}' registry", c, r.second["name"].Scalar());
                    }
                }
            }
        }

        // Wait for one of the fetches to complete
        if (git_fetching_list.size() != 0)
        {
            decltype(git_fetching_list)::iterator completed_fetch;
            auto t1 = std::chrono::high_resolution_clock::now();
            do {
                completed_fetch = std::find_if(git_fetching_list.begin(), git_fetching_list.end(), [](decltype(git_fetching_list)::value_type& f){ return f.second.wait_for(100ms) == std::future_status::ready; });
            } while (completed_fetch == git_fetching_list.end());
            auto t2 = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
            boblog->info("{}: fetched in {}ms", completed_fetch->first, duration);

            project.unknown_components.erase(completed_fetch->first);
            project.unprocessed_components.push_back(completed_fetch->first);
            // std::this_thread::sleep_for(1000ms);
            project.component_database.scan_for_components("./components/" + completed_fetch->first);
            git_fetching_list.erase(completed_fetch);
            continue;
        }

        if (git_fetching_list.size() == 0 && project.unknown_components.size( ) != 0)
        {
            console->error("Failed to find the following components:");
            for (const auto& c: project.unknown_components)
                console->error(" - {}", c);

            fs::remove(bob::component_database::database_filename);
            std::cout << "Scanning '.' for components" << std::endl;
            bob::component_database db;
            db.save();
            
            return 0;
        }

    } while(project.unprocessed_components.size() != 0 || project.unknown_components.size( ) != 0 || git_fetching_list.size() != 0);

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

        auto construction_start_time = fs::file_time_type::clock::now();

        auto building_future = std::async(std::launch::async, [&project, &building_bar] () {
            project.process_construction(building_bar);
        });

        // Update bar state
        size_t previous = 0;
        do {
            // if (building_bar.current() != previous) {
            //     building_bar.print_progress();
            //     previous = building_bar.current();
            // }
        } while (building_future.wait_for(200ms) != std::future_status::ready);

        auto construction_end_time = fs::file_time_type::clock::now();

        // building_bar.print_progress();
        std::cout << "Building complete in " << std::chrono::duration_cast<std::chrono::milliseconds>(construction_end_time - construction_start_time).count() << " milliseconds" << std::endl;
    }

    console->flush();
	show_console_cursor(true);
    return 0;
}

namespace bob {
using namespace std::string_literals;
template<typename Functor>
void fetch_component(const std::string& name, YAML::Node node, Functor set_progress)
{
    auto boblog = spdlog::get("boblog");
    enum {
        GIT_COUNTING    = 0,
        GIT_COMPRESSING = 1,
        GIT_RECEIVING   = 2,
        GIT_RESOLVING   = 3,
        GIT_LFS_CHECKOUT= 4,
    } phase = GIT_COUNTING;
    int old_progress = 0;

    // Of the total time to fetch a Git repo, 10% is allocated to counting, 10% to compressing, and 80% to receiving.
    static const int phase_rates[] = {0, 10, 20, 75, 80};

    std::string url    = node["packages"]["default"]["url"].as<std::string>();
    std::string branch = node["packages"]["default"]["branch"].as<std::string>();
    const std::string fetch_string = "-C "s + ".bob"s + "/repos/ clone " + url + " " + name + " -b " + branch + " --progress --single-branch --no-checkout";

#if defined(_WIN64) || defined(_WIN32) || defined(__CYGWIN__)
# define GIT_STRING  "c:/silabs/apps/git/bin/git"
#else
# define GIT_STRING  "git"
#endif
    auto t1 = std::chrono::high_resolution_clock::now();
    bob::exec(GIT_STRING, fetch_string, [&](const std::string& data) -> void {
        std::smatch s;
        if ( phase < GIT_COMPRESSING && data.find("Comp" ) != data.npos ) {phase = GIT_COMPRESSING; }
        if ( phase < GIT_RECEIVING && data.find("Rece") != data.npos ) { phase = GIT_RECEIVING; }
        if ( phase < GIT_RESOLVING && data.find("Reso") != data.npos ) { phase = GIT_RESOLVING; }

        if (std::regex_search(data, s, std::regex { R"(\((.*)/(.*)\))" }))
        {
            // boblog->info(data);
            int phase_progress = std::stoi( s[1] );
            int end_value = std::stoi( s[2] );
            int progress = phase_rates[phase] + ((phase_rates[phase+1]-phase_rates[phase])*phase_progress)/end_value;
            // if (progress < old_progress)
            //   boblog->info << name << ": " << "Progress regressed\n" << data << "\n";
            if (progress != old_progress)
                set_progress(progress);
            old_progress = progress;
        }
    });
    auto t2 = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
    boblog->info("{}: cloned in {}ms", name, duration);

    if (!fs::exists("components/" + name))
        fs::create_directories("components/" + name);
    const std::string checkout_string     = "--git-dir "s + ".bob"s + "/repos/" + name + "/.git --work-tree components/" + name + " checkout " + branch + " --force";
    const std::string lfs_checkout_string = "--git-dir "s + ".bob"s + "/repos/" + name + "/.git --work-tree components/" + name + " lfs checkout";

    // Checkout instance
    t1 = std::chrono::high_resolution_clock::now();
    bob::exec(GIT_STRING, checkout_string, [&](const std::string& data) -> void {
        std::smatch s;
        if (std::regex_search(data, s, std::regex { R"(\((.*)/(.*)\))" }))
        {
            // boblog->info(data);
            int phase_progress = std::stoi( s[1] );
            int end_value = std::stoi( s[2] );
            int progress = phase_rates[GIT_LFS_CHECKOUT] + ((100-phase_rates[GIT_LFS_CHECKOUT])*phase_progress)/end_value;
            set_progress(progress);
        }
    });
    t2 = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
    boblog->info("{}: checkout in {}ms", name, duration);

    // set_progress(90);

    // t1 = std::chrono::high_resolution_clock::now();
    // bob::exec(GIT_STRING, lfs_checkout_string, [&](const std::string& data) -> void {
    //     std::smatch s;
    //     if (std::regex_search(data, s, std::regex { R"(\((.*)/(.*)\))" }))
    //     {
    //         // boblog->info(data);
    //         int phase_progress = std::stoi( s[1] );
    //         int end_value = std::stoi( s[2] );
    //         int progress = phase_rates[GIT_LFS_CHECKOUT] + ((100-phase_rates[GIT_LFS_CHECKOUT])*phase_progress)/end_value;
    //         set_progress(progress);
    //     }
    // });
    // t2 = std::chrono::high_resolution_clock::now();
    // duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
    // boblog->info("{}: LFS checkout in {}ms", name, duration);
    set_progress(100);
}

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
        std::array<char, 512> buffer;
        std::string result;
        size_t count = 0;
        do {
            if (output != nullptr)
            {
                if ((count = fread(buffer.data(), 1, buffer.size(), output)) > 0)
                    result += buffer.data();

                if (count != buffer.size())
                {
                    if (ferror(output))
                        count = 0;
                }
            }
        } while(count > 0);

        p.wait();
        return {result,p.retcode()};
    } catch (std::exception e)
    {
        boblog->error("Exception while executing: {}\n{}", command_text, e.what());
        return {};
    }
}

template<typename Functor>
void exec( const std::string& command_text, const std::string& arg_text, Functor function)
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
                
                // if ((count = fread(buffer.data(), 1, buffer.size(), output)) > 0) {
                //     std::string temp(buffer.data());
                //     function( temp );
                // }
            };
        }
        p.wait();
    } catch (std::exception e)
    {
        boblog->error("Exception while executing: {}\n{}", command_text, e.what());
    }
}
#if 0
bool yaml_diff(const YAML::Node& node1, const YAML::Node& node2)
{
    std::vector<std::pair<std::shared_ptr<YAML::Node>, std::shared_ptr<YAML::Node>>> compare_list;
    compare_list.push_back({std::make_shared<YAML::Node>(node1), std::make_shared<YAML::Node>(node2)});
    for (auto i = compare_list.begin(); i != compare_list.end(); ++i)
    {
        if (i->first->Type() != i->second->Type())
            return false;
        
        switch(i->first->Type())
        {
            case YAML::NodeType::Scalar:
                break;
            case YAML::NodeType::Sequence:
                // Verify the sequences have the same length
                if (i->first->size() != i->second->size())
                    return false;
                for (int a=0; a < i->first->size(); ++a)
                    compare_list.push_back({std::make_shared<YAML::Node>(i->first[a]), std::make_shared<YAML::Node>(i->second[a])});
                break;
            case YAML::NodeType::Map:
                // Verify the maps have the same length
                if (i->first->size() != i->second->size())
                    return false;
                for (const auto& a: i->first))
                    compare_list.push_back({std::make_shared<YAML::Node>(i->first[a]), std::make_shared<YAML::Node>(i->second[a])});
                break;
        }
    }
}
#endif
}
