/* A BOB workspace is identified by a .bob folder
 * The .bob folder holds information about locally available components, and remote registries
 */
#include "bob.hpp"
#include "bob_workspace.hpp"
// #include "example_registry.h"
#include <filesystem>
#include <fstream>
#include <regex>

namespace fs = std::filesystem;

std::string example_registry = "";

namespace bob
{
    workspace::workspace() : workspace_directory(".")
    {
        log = spdlog::get("boblog");
        init();
    }

    void workspace::init()
    {
        if (!fs::exists(".bob"))
        {
            fs::create_directories(".bob/registries");
            std::ofstream example(".bob/registries/silabs.yaml");
            example << example_registry;
            example.close();
        }
        else
        {
            // Load registries
        }

        if (!fs::exists(".bob/repos"))
            fs::create_directories(".bob/repos");
    }

    void workspace::load_component_registries()
    {
        // Verify the .bob/registries path exists
        if (!fs::exists(workspace_directory + "/.bob/registries"))
            return;

        for ( const auto& p : fs::recursive_directory_iterator( workspace_directory + "/.bob/registries") )
            if ( p.path().extension().generic_string() == ".yaml" )
                try
                {
                    registries[p.path().filename().replace_extension().generic_string()] = YAML::LoadFile(p.path().generic_string());
                }
                catch (...)
                {
                    log->error("Could not parse component registry: '{}'", p.path().generic_string());
                }
    }

    std::optional<YAML::Node> workspace::find_registry_component(const std::string& name)
    {
        // Look for component in registries
        for ( const auto& r : registries )
            if ( r.second["provides"]["components"][name].IsDefined( ) )
                return r.second["provides"]["components"][name];
        return {};
    }

    std::future<void> workspace::fetch_component(const std::string& name, YAML::Node node, std::function<void(size_t)> progress_handler)
    {
        return std::async(std::launch::async, [=]() {
                bob::fetch_component(name, node, progress_handler);
        });
    }

    using namespace std::string_literals;
    void fetch_component(const std::string& name, YAML::Node node, std::function<void(size_t)> progress_handler)
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
        bob::exec(GIT_STRING, fetch_string, [&](std::string& data) -> void {
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
                    progress_handler(progress);
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
                progress_handler(progress);
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
        progress_handler(100);
    }


}
