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

#if 0

    ProgressBar temp_git_bar{option::BarWidth{50}, option::ShowElapsedTime{false}, option::PrefixText{"Fetching "}, option::SavedStartTime{true}};
    DynamicProgress<ProgressBar> temp_bars(temp_git_bar);
    temp_bars.set_option(option::HideBarWhenComplete{false});
    temp_bars.print_progress();

    enum {
        GIT_COUNTING    = 0,
        GIT_COMPRESSING = 1,
        GIT_RECEIVING   = 2,
    } phase = GIT_COUNTING;
    static const int phase_rates[] = {0, 20, 40, 100};
#if defined(_WIN64) || defined(_WIN32) || defined(__CYGWIN__)
    bob::exec("c:\\silabs\\apps\\git\\bin\\git", "clone --progress ssh://git@stash.silabs.com/gos/sl_wifi.git", [&temp_git_bar, &phase, &temp_bars](std::string& data) -> void {
        std::smatch s;
        //std::clog << "[[ " << data << " ]]\n";

        // Determine phase
        // if ( data.find("Coun") != data.npos ) phase = GIT_COUNTING;//std::clog << "Counting...\n";
        if ( data.find("Comp" ) != data.npos ) phase = GIT_COMPRESSING;//std::clog << "Compressing...\n";
        if ( data.find("Rece") != data.npos ) phase = GIT_RECEIVING;//std::clog << "Receiving...\n";
        
        if (std::regex_search(data, s, std::regex { R"(\((.*)/(.*)\))" }))
        {
            int phase_progress = std::stoi( s[1] );
            int end_value = std::stoi( s[2] );
            float progress = phase_rates[phase] + (phase_rates[phase+1]-phase_rates[phase])*phase_progress/end_value;
            temp_git_bar.set_progress(progress);
            temp_bars.print_progress();
            std::clog << "Got " << s[1] << " of " << s[2] << ".. Calculated " << progress << "%" << std::endl;
        }
        // Determine progress
        // auto left  = data.find("(");
        // auto right = data.find(")", left);
        // if (left != data.npos && right != data.npos)
        // {
            
        // }
    });
#else
    std::clog << bob::exec("git", "clone --progress ssh://git@stash.silabs.com/gos/sl_wifi.git");
#endif
    std::clog.flush();
    std::clog.rdbuf( clog_backup );
    exit(0);
#endif

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

namespace bob {

// static std::string make_command(const std::string_view command_text, const std::string_view& arg_text)
// {
//     std::string full_command { command_text };

//     // #if defined(_WIN64) || defined(_WIN32) || defined(__CYGWIN__)

//     // #endif

//     #if defined(_WIN64) || defined(_WIN32) || defined(__CYGWIN__)
//         // std::replace(full_command.begin(), full_command.end(), '/', '\\');
//         // if (!arg_text.empty())
//         // {
//             full_command.insert(0, "\"");
//             full_command.append("\"");
//         // }
//     #endif

//        if (!arg_text.empty())
//         {
//             full_command.append(" ");
//             full_command.append(arg_text);
//         }

//         // full_command.append(" 2>&1");
//         std::clog << "exec: " << full_command << "\n";
//     return full_command;
// }

#if 0//defined(__USING_WINDOWS__)
std::string exec( const std::string& command_text, const std::string& arg_text)
{
    std::clog << command_text << " " << arg_text << "\n";
    try {
        auto output = subprocess::check_output({command_text, arg_text}, subprocess::error{subprocess::STDOUT});
        return output.buf.data();
    } catch (std::exception e)
    {
        std::clog << e.what();
        return {};
    }
}
#else
std::string exec( const std::string& command_text, const std::string& arg_text)
{
    std::clog << command_text << " " << arg_text << "\n";
    try {
        std::string command = command_text + " " + arg_text;
        auto p = subprocess::Popen(command, subprocess::output{subprocess::PIPE}, subprocess::error{subprocess::STDOUT} );
        auto output = p.output();
        std::array<char, 512> buffer;
        std::string result;
        size_t count = 0;
        do {
            if (output != nullptr)
            {
                if ((count = fread(buffer.data(), 1, buffer.size(), output)) > 0) {
                    result += buffer.data();
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
#endif

//     std::array<char, 512> buffer;
//     std::string result = "";
//     std::string full_command { command_text };

//     #if defined(_WIN64) || defined(_WIN32) || defined(__CYGWIN__)
//     // std::replace(full_command.begin(), full_command.end(), '/', '\\');
//     // if (!arg_text.empty())
//     // {
//         full_command.insert(0, "\"");
//         full_command.append("\"");
//     // }
//     #endif

//     if (!arg_text.empty())
//     {
//         full_command.append(" ");
//         full_command.append(arg_text);
//     }

//     // full_command.append(" 2>&1");
//     std::clog << "exec: " << full_command << "\n";

//     #if defined(_WIN64) || defined(_WIN32) || defined(__CYGWIN__)
//     // auto out = subprocess::check_output(full_command, subprocess::error{subprocess::STDOUT});
//     // return out.buf.data();
//     auto p = subprocess::Popen(full_command, subprocess::error{subprocess::STDOUT} );
//     auto output = p.output();
//     size_t count = 0;
//     do {
//         if (output != nullptr)
//         {
//             if ((count = fread(buffer.data(), 1, buffer.size(), output)) > 0) {
//                 result += buffer.data();
//             }
//         }
//     } while(count > 0);

//     p.wait();
//     return result;
//     #else
//         // auto p = (command_text.find("git") != command_text.npos) ?
//                 //   subprocess::Popen(full_command, subprocess::output{subprocess::PIPE}, subprocess::error{subprocess::STDOUT} ) :
//     auto p = subprocess::Popen(command, subprocess::error{subprocess::STDOUT} );
    

//     auto output = p.output();
//     size_t count = 0;
//     do {
//         if (output != nullptr)
//         {
//             if ((count = fread(buffer.data(), 1, buffer.size(), output)) > 0) {
//                 result += buffer.data();
//             }
//         }
//     } while(count > 0);

//     p.wait();
//     return result;
//     #endif
// return {};
// }

// template<typename Functor>
// void exec( const std::string_view command_text, const std::string_view& arg_text, Functor function)
// {
//     std::array<char, 64> buffer;
//     std::string full_command { command_text };

//     #if defined(_WIN64) || defined(_WIN32) || defined(__CYGWIN__)
//     // std::replace(full_command.begin(), full_command.end(), '/', '\\');
//     // if (!arg_text.empty())
//     // {
//         full_command.insert(0, "\"");
//         full_command.append("\"");
//     // }
//     #endif

//     if (!arg_text.empty())
//     {
//         full_command.append(" ");
//         full_command.append(arg_text);
//     }

//     // full_command.append(" 2>&1");
//     std::clog << "exec: " << full_command << "\n";

//     #if defined(_WIN64) || defined(_WIN32) || defined(__CYGWIN__)
//     auto p = subprocess::Popen(full_command, subprocess::error{subprocess::STDOUT});
//     #else
//         // auto p = (command_text.find("git") != command_text.npos) ?
//                 //   subprocess::Popen(full_command, subprocess::output{subprocess::PIPE}, subprocess::error{subprocess::STDOUT} ) :
//     auto p =subprocess::Popen(command, subprocess::error{subprocess::STDOUT} );
//     #endif

//     auto output = p.output();

//     size_t count = 0;
//     do {
//         if (output != nullptr)
//         {
//             if ((count = fread(buffer.data(), 1, buffer.size(), output)) > 0) {
//                 function( std::string(buffer.data()) );
//             }
//         }
//     } while(count > 0);

//     p.wait();
// }
}