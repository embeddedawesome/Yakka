#include "bob.h"
#include "bob_project.h"
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

    // Setup logging
    std::ofstream log_file( "bob.log" );
    auto clog_backup = std::clog.rdbuf( log_file.rdbuf( ) );

     ProgressBar bar1{option::BarWidth{50},
                   option::ShowElapsedTime{true},
                   option::PrefixText{"Building "}};

    DynamicProgress<ProgressBar> bars(bar1);
    bars.set_option(option::HideBarWhenComplete{false});
    bars.print_progress();

    // Convert the command line args into a vector
    std::vector< std::string > args;
    for ( int a = 1; a < argc; a++ )
        args.push_back( argv[a] );

    auto t1 = std::chrono::high_resolution_clock::now();

    bob::project project(args);

    project.load_component_registries();

    project.evaluate_dependencies();

    auto t2 = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
    std::clog << duration << " milliseconds to process components\n";

    std::clog << "Required features:" << std::endl;
    for (auto f: project.required_features)
        std::clog << "- " << f << std::endl;

    // project.process_aggregates();

    project.save_summary();

    t1 = std::chrono::high_resolution_clock::now();
    project.parse_blueprints();

    project.evalutate_blueprint_dependencies();

    t2 = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
    std::clog << duration << " milliseconds to process blueprints\n";

    project.load_common_commands();

//    std::clog << project.project_summary_json << std::endl;

    //auto building_future = std::async([&project, &bar1]() {
    //    project.process_construction(bar1);
    //});
    auto building_future = std::async(std::launch::async,[&project, &bar1](){
        project.process_construction(bar1);
    });

  // Update bar state
  do {
      bars.print_progress();
  } while ( building_future.wait_for( 100ms ) != std::future_status::ready );
  
  bars.print_progress();

//    std::cout << "Need to build: " << std::endl;
//    for (const auto& c: project.construction_list)
//        std::cout << c.first << std::endl;
    
    std::clog.rdbuf( clog_backup );
    return 0;
}
