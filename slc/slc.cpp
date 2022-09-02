#include "slc_project.h"
#include "cxxopts.hpp"
#include "spdlog/spdlog.h"
#include <indicators/progress_bar.hpp>
#include <iostream>
#include <fstream>
#include <chrono>
#include <future>
#include <filesystem>

using namespace std::chrono_literals;

int main(int argc, char **argv)
{
    // Setup logging
    std::error_code error_code;
    std::filesystem::remove("slc.log", error_code);
    auto slclog = spdlog::basic_logger_mt("slclog", "slc.log");

    auto console = spdlog::stderr_color_mt("slcconsole");
    console->flush_on(spdlog::level::level_enum::off);
    console->set_pattern("[%^%l%$]: %v");

    cxxopts::Options options("slc", "Silicon Labs Configurator");
    options.positional_help("<action> [optional args]");
    options.add_options()
        ("h,help", "Print usage")
        ("l,list", "List known components", cxxopts::value<bool>()->default_value("false"))
        ("r,refresh", "Refresh component database", cxxopts::value<bool>()->default_value("false"))
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
        slc_project project;
        console->info( "Scanning '.' for components" );
        auto t1 = std::chrono::high_resolution_clock::now();
        project.generate_slcc_database();
        auto t2 = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
        console->info("{}ms to process components", duration);

        for (const auto& i: project.slcc_database)
            slclog->info("{}", i.first);
    }
    
    console->flush();
    return 0;
}
