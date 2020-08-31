#include "bob.h"
#include "bob_project.h"
#include <iostream>
#include <fstream>
#include <chrono>

int main(int argc, char **argv)
{
    auto bob_start_time = std::time(nullptr);

    // Setup logging
    std::ofstream log_file( "bob.log" );
    std::clog.rdbuf( log_file.rdbuf( ) );

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

    project.process_aggregates();

    t1 = std::chrono::high_resolution_clock::now();
    project.parse_blueprints();

    project.evalutate_blueprint_dependencies();

    t2 = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
    std::clog << duration << " milliseconds to process blueprints\n";

    project.save_summary();

    project.load_common_commands();

//    std::clog << project.project_summary_json << std::endl;

    project.process_construction();

//    std::cout << "Need to build: " << std::endl;
//    for (const auto& c: project.construction_list)
//        std::cout << c.first << std::endl;

    return 0;
}
