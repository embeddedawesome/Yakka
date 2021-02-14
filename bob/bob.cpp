#include "bob.h"
#include "bob_project.h"
#include "cxxopts.hpp"
#include <indicators/dynamic_progress.hpp>
#include <indicators/progress_bar.hpp>
#include <indicators/cursor_control.hpp>
#include <indicators/termcolor.hpp>
#include <iostream>
#include <fstream>
#include <chrono>

using namespace indicators;
using namespace std::chrono_literals;

#include "bob.h"

int main(int argc, char **argv)
{
    auto bob_start_time = std::time(nullptr);

    // std::cout << bob::exec("git", "clone ssh://git@stash.silabs.com/gos/sl_wifi.git");
    // exit(0);

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

    // Setup logging
    std::ofstream log_file( "bob.log" );
    auto clog_backup = std::clog.rdbuf( log_file.rdbuf( ) );

    auto t1 = std::chrono::high_resolution_clock::now();

    bob::project project(result.unmatched());

    project.load_component_registries();

    do {
        project.evaluate_dependencies();

        if ( project.unknown_components.size( ) != 0 )
        {
            std::cerr << "Failed to find the following components:" << std::endl;
            for (const auto& c: project.unknown_components)
                std::cerr << " - " << c << std::endl;
            
            // Check whether any of the unknown components are in registries.
            for ( auto r : project.registries )
                for (const auto& c: project.unknown_components)
                    if ( r.second["provides"]["components"][c].IsDefined( ) )
                    {
                        std::cerr << "Component '" << c << "' can be fetched from the '" << r.second["name"] << "' registry" << std::endl;
                    }
            
            exit(0);
        }
    } while(0);

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

    ProgressBar bar1{option::BarWidth{50}, option::ShowElapsedTime{false}, option::PrefixText{"Building "}, option::SavedStartTime{true}};

    DynamicProgress<ProgressBar> bars(bar1);
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
