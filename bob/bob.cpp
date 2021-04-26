#include "bob.h"
#include "bob_project.h"
#include "cxxopts.hpp"
#include "subprocess.hpp"
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
    std::ofstream log_file( "bob.log" );
    auto clog_backup = std::clog.rdbuf( log_file.rdbuf( ) );

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

    if (result.unmatched().empty())
        exit(0);

    auto t1 = std::chrono::high_resolution_clock::now();

    DynamicProgress<ProgressBar> bars;
    std::vector<std::shared_ptr<ProgressBar>> progress_bars;

    bob::project project(result.unmatched());

    project.load_component_registries();

    
    std::vector<std::pair<std::string, std::future<void>>> git_fetching_list;
    do {
        project.unknown_components.clear();
        project.evaluate_dependencies();
        int found_components = 0;

        std::clog << "Unknown components: ";
        for (const auto u: project.unknown_components)
            std::clog << u << ", ";
        std::clog << "\n";

        if ( project.unknown_components.size( ) != 0 )
        {
            // Check whether any of the unknown components are in registries.
            for ( auto r : project.registries )
            {
                for (const auto& c: project.unknown_components)
                {
                    if ( r.second["provides"]["components"][c].IsDefined( ) )
                    {
                        if (result["fetch"].as<bool>())
                        {
                            ++found_components;
                            progress_bars.push_back(std::make_shared<ProgressBar>(option::BarWidth{50}, option::ShowPercentage{true}, option::PrefixText{"Fetching " + c + " "}, option::SavedStartTime{true}));
                            auto id = bars.push_back(*progress_bars.back());
                            // auto id = bars.push_back(ProgressBar{option::BarWidth{50}, option::ShowPercentage{true}, option::PrefixText{"Fetching " + c + " "}, option::SavedStartTime{true}});
                            bars.print_progress();
                            git_fetching_list.push_back( {c, std::async(std::launch::async, [&](size_t bar_id){
                                bob::fetch_component(c, r.second["provides"]["components"][c], [&](size_t number) {bars[bar_id].set_progress(number);});
                                bars[bar_id].mark_as_completed();
                            }, id)});
                        }
                        else
                            std::cerr << "Component '" << c << "' can be fetched from the '" << r.second["name"] << "' registry" << std::endl;
                    }
                }
            }
        }  
        
        // Wait for all the fetching to complete
        if (git_fetching_list.size() != 0)
        {
            decltype(git_fetching_list)::iterator completed_fetch;
            do {
                completed_fetch = std::find_if(git_fetching_list.begin(), git_fetching_list.end(), [](decltype(git_fetching_list)::value_type& f){ return f.second.wait_for(100ms) == std::future_status::ready; });
            } while (completed_fetch == git_fetching_list.end());
            std::clog << completed_fetch->first << " finished\n";
            project.unprocessed_components.push_back(completed_fetch->first);
            project.component_database.scan_for_components("./components/" + completed_fetch->first);
            git_fetching_list.erase(completed_fetch);
            continue;
        }

        if (git_fetching_list.size() == 0 && project.unknown_components.size( ) != 0)
        {
            std::cerr << "Failed to find the following components:" << std::endl;
            for (const auto& c: project.unknown_components)
                std::cerr << " - " << c << std::endl;
            return 0;
        }

        // for( const auto& f: git_fetching_list)
        // {
        //     f.wait();
        // }

        // Scan to find the new components. Ideally we just insert the components as they are downloaded. Note that a component download may contain multiple components.
        // project.component_database.save();
        
        // return 0;
    } while(project.unknown_components.size( ) != 0 || git_fetching_list.size() != 0);

    auto t2 = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
    std::clog << duration << " milliseconds to process components\n";

    std::clog << "Required features:" << std::endl;
    for (auto f: project.required_features)
        std::clog << "- " << f << std::endl;

    project.generate_project_summary();
    project.save_summary();


    t1 = std::chrono::high_resolution_clock::now();
    project.parse_blueprints();

    project.evaluate_blueprint_dependencies();

    t2 = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
    std::clog << duration << " milliseconds to process blueprints\n";

    project.load_common_commands();

    ProgressBar bar1{option::BarWidth{50}, option::ShowPercentage{true}, option::PrefixText{"Building "}};

    bars.push_back(bar1);
    bars.set_option(option::HideBarWhenComplete{false});
    bars.print_progress();
//    std::clog << project.project_summary_json << std::endl;

    auto construction_start_time = fs::file_time_type::clock::now();

    auto building_future = std::async(std::launch::async,[&project, &bar1](){
        project.process_construction(bar1);
    });

  // Update bar state
  size_t previous = 0;
  do {
    if (bar1.current() != previous) {
        bars.print_progress();
        previous = bar1.current();
    }
  } while ( building_future.wait_for( 200ms ) != std::future_status::ready );
  
  auto construction_end_time = fs::file_time_type::clock::now();

  bars.print_progress();
  std::cout << "Completed in " << std::chrono::duration_cast<std::chrono::milliseconds>(construction_end_time - construction_start_time).count() << " milliseconds" << std::endl;

//    std::cout << "Need to build: " << std::endl;
//    for (const auto& c: project.construction_list)
//        std::cout << c.first << std::endl;
    
    std::clog.rdbuf( clog_backup );
    return 0;
}

namespace bob {
using namespace std::string_literals;
template<typename Functor>
void fetch_component(const std::string& name, YAML::Node node, Functor set_progress)
{
    enum {
        GIT_COUNTING    = 0,
        GIT_COMPRESSING = 1,
        GIT_RECEIVING   = 2,
    } phase = GIT_COUNTING;

    // Of the total time to fetch a Git repo, 10% is allocated to counting, 10% to compressing, and 80% to receiving.
    static const int phase_rates[] = {0, 10, 20, 90};

    std::string url    = node["packages"]["default"]["url"].as<std::string>();
    std::string branch = node["packages"]["default"]["branch"].as<std::string>();
    const std::string fetch_string = "-C "s + ".bob"s + "/repos/ clone " + url + " " + name + " -b " + branch + " --progress --single-branch --no-checkout";

#if defined(_WIN64) || defined(_WIN32) || defined(__CYGWIN__)
# define GIT_STRING  "c:/silabs/apps/git/bin/git"
#else
# define GIT_STRING  "git"
#endif
    auto t1 = std::chrono::high_resolution_clock::now();
    bob::exec(GIT_STRING, fetch_string, [&](std::string& data) -> void {
        std::smatch s;
        // std::clog << "[[ " << data << " ]]\n";

        // Determine phase
        // if ( data.find("Coun") != data.npos ) phase = GIT_COUNTING;//std::clog << "Counting...\n";
        if ( phase < GIT_COMPRESSING && data.find("Comp" ) != data.npos ) {phase = GIT_COMPRESSING; /*progress_bar.set_option(indicators::option::PostfixText{"Compressing"});*/ }
        if ( phase < GIT_RECEIVING && data.find("Rece") != data.npos ) { phase = GIT_RECEIVING; /*progress_bar.set_option(indicators::option::PostfixText{"Receiving"});*/ }
        
        if (std::regex_search(data, s, std::regex { R"(\((.*)/(.*)\))" }))
        {
            int phase_progress = std::stoi( s[1] );
            int end_value = std::stoi( s[2] );
            float progress = phase_rates[phase] + (phase_rates[phase+1]-phase_rates[phase])*phase_progress/end_value;
            set_progress(progress);
        }
    });
    // auto t2 = std::chrono::high_resolution_clock::now();
    // auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
    // std::clog << duration << " milliseconds to download: " << name << "\n";
    // t1 = t2;

    if (!fs::exists("components/" + name))
        fs::create_directories("components/" + name);
    const std::string checkout_string     = "--git-dir "s + ".bob"s + "/repos/" + name + "/.git --work-tree components/" + name + " checkout " + branch + " --force";
    const std::string lfs_checkout_string = "--git-dir "s + ".bob"s + "/repos/" + name + "/.git --work-tree components/" + name + " lfs checkout";
    
    // Checkout isntance
    auto result = bob::exec(GIT_STRING, checkout_string);
    // std::clog << result << std::endl;
    // t2 = std::chrono::high_resolution_clock::now();
    // duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
    // std::clog << duration << " milliseconds to checkout: " << name << "\n";
    // t1 = t2;
    set_progress(90);

    result = bob::exec(GIT_STRING, lfs_checkout_string);
    // std::clog << result << std::endl;
    // t2 = std::chrono::high_resolution_clock::now();
    // duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
    // std::clog << duration << " milliseconds to LFS: " << name << "\n";
    set_progress(100);
}

std::string exec( const std::string& command_text, const std::string& arg_text)
{
    std::clog << command_text << " " << arg_text << "\n";
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
        return result;
    } catch (std::exception e)
    {
        std::clog << e.what();
        return {};
    }
}

template<typename Functor>
void exec( const std::string& command_text, const std::string& arg_text, Functor function)
{
    std::clog << command_text << " " << arg_text << "\n";
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
        std::array<char, 32> buffer;
        size_t count = 0;
        do {
            if (output != nullptr)
            {
                if ((count = fread(buffer.data(), 1, buffer.size(), output)) > 0) {
                    std::string temp(buffer.data());
                    function( temp );
                }
            }
        } while(count > 0);
        p.wait();
    } catch (std::exception e)
    {
        std::clog << e.what();;
    }
}
}