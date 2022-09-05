/* A BOB workspace is identified by a .yakka folder
 * The .yakka folder holds information about locally available components, and remote registries
 */
#include "yakka.hpp"
#include "yakka_workspace.hpp"
// #include "example_registry.h"
#include <filesystem>
#include <fstream>
#include <regex>

namespace fs = std::filesystem;

std::string example_registry = "";

namespace yakka
{
    workspace::workspace() : workspace_directory(".")
    {
        load_config_file("config.yaml");

        configuration["host_os"] = host_os_string;
        configuration["executable_extension"] = executable_extension;
        configuration_json = configuration.as<nlohmann::json>();
    }

    void workspace::init()
    {
        log = spdlog::get("yakkalog");

        if (!fs::exists(".yakka/registries"))
            fs::create_directories(".yakka/registries");

        if (!fs::exists(".yakka/repos"))
            fs::create_directories(".yakka/repos");
    }

    void workspace::load_component_registries()
    {
        // Verify the .yakka/registries path exists
        if (!fs::exists(workspace_directory + "/.yakka/registries"))
            return;

        for ( const auto& p : fs::recursive_directory_iterator( workspace_directory + "/.yakka/registries") )
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

    yakka_status workspace::add_component_registry(const std::string& url)
    {
        return fetch_registry(url);
    }

    std::optional<YAML::Node> workspace::find_registry_component(const std::string& name)
    {
        // Look for component in registries
        for ( const auto& r : registries )
            if ( r.second["provides"]["components"][name].IsDefined( ) )
                return r.second["provides"]["components"][name];
        return {};
    }

    std::string workspace::template_render(const std::string input)
    {
        return inja_environment.render(input, configuration_json);
    }

    void workspace::load_config_file(const std::string config_filename)
    {
        if (!fs::exists(config_filename))
            return;

        try
        {
            auto configuration = YAML::LoadFile( config_filename );

            // project_summary["configuration"] = configuration;
            // project_summary["tools"] = configuration["tools"];

            // if (configuration["yakka_home"].IsDefined())
            // {
            //     yakka_home_directory =  configuration["yakka_home"].Scalar();
            //     if (!fs::exists(yakka_home_directory + "/repos"))
            //         fs::create_directories(yakka_home_directory + "/repos");
            // }

            if (configuration["path"].IsDefined())
            {
                std::string path = std::getenv("PATH");
                for (const auto& p: configuration["path"])
                {
                    path += host_os_path_seperator + p.as<std::string>();
                }
                #if defined(_WIN64) || defined(_WIN32) || defined(__CYGWIN__)
                _putenv_s("PATH", path.c_str());
                #else
                setenv("PATH", path.c_str(), 1);
                #endif
            }
        }
        catch ( std::exception &e )
        {
            log->error("Couldn't read '{}'\n{}", config_filename, e.what( ));
            // project_summary["configuration"];
            // project_summary["tools"] = "";
        }

    }

    std::future<void> workspace::fetch_component(const std::string& name, YAML::Node node, std::function<void(size_t)> progress_handler)
    {
        std::string url    = template_render(node["packages"]["default"]["url"].as<std::string>());
        std::string branch = template_render(node["packages"]["default"]["branch"].as<std::string>());
        
        return std::async(std::launch::async, [=]() {
                do_fetch_component(name, url, branch, progress_handler);
        });
    }

    #define GIT_STRING  "git"
    yakka_status workspace::fetch_registry(const std::string& url )
    {
        auto yakkalog = spdlog::get("yakkalog");
        const std::string fetch_string = "-C .yakka/registries/ clone " + url + " --progress --single-branch";
        auto [output, result] = yakka::exec(GIT_STRING, fetch_string);

        yakkalog->info("{}", output);

        if (result != 0)
            return FAIL;

        return SUCCESS;
    }

    yakka_status workspace::update_component(const std::string& name )
    {
        // This function could be async like fetch_component
        auto yakkalog = spdlog::get("yakkalog");
        auto console = spdlog::get("yakkaconsole");
        const std::string git_directory_string = "--git-dir .yakka/repos/" + name + "/.git --work-tree components/" + name + " ";

        auto [stash_output, stash_result] = yakka::exec(GIT_STRING, git_directory_string + "stash");
        if (stash_result != 0) 
        {
            console->error(stash_output);
            return FAIL;
        }
        yakkalog->info(stash_output);

        auto [pull_output, pull_result] = yakka::exec(GIT_STRING, git_directory_string + "pull --progress");
        yakkalog->info(pull_output);

        auto [pop_output, pop_result] = yakka::exec(GIT_STRING, git_directory_string + "stash pop");
        yakkalog->info(pop_output);
        return SUCCESS;
    }

    using namespace std::string_literals;
    void workspace::do_fetch_component(const std::string& name, const std::string url, const std::string branch, std::function<void(size_t)> progress_handler)
    {
        auto yakkalog = spdlog::get("yakkalog");
        enum {
            GIT_COUNTING    = 0,
            GIT_COMPRESSING = 1,
            GIT_RECEIVING   = 2,
            GIT_RESOLVING   = 3,
            GIT_LFS_CHECKOUT= 4,
        } phase = GIT_COUNTING;
        int old_progress = 0;

        // Of the total time to fetch a Git repo, 10% is allocated to counting, 10% to compressing, and 80% to receiving.
        static const int phase_rates[] = {0, 10, 20, 75, 90};
        const std::string fetch_string = "-C "s + ".yakka"s + "/repos/ clone " + url + " " + name + " -b " + branch + " --progress --single-branch --no-checkout";

        auto t1 = std::chrono::high_resolution_clock::now();
        yakka::exec(GIT_STRING, fetch_string, [&](std::string& data) -> void {
            std::smatch s;
            if ( phase < GIT_COMPRESSING && data.find("Comp" ) != data.npos ) {phase = GIT_COMPRESSING; }
            if ( phase < GIT_RECEIVING && data.find("Rece") != data.npos ) { phase = GIT_RECEIVING; }
            if ( phase < GIT_RESOLVING && data.find("Reso") != data.npos ) { phase = GIT_RESOLVING; }

            if (std::regex_search(data, s, std::regex { R"(\((.*)/(.*)\))" }))
            {
                // yakkalog->info(data);
                int phase_progress = std::stoi( s[1] );
                int end_value = std::stoi( s[2] );
                int progress = phase_rates[phase] + ((phase_rates[phase+1]-phase_rates[phase])*phase_progress)/end_value;
                // if (progress < old_progress)
                //   yakkalog->info << name << ": " << "Progress regressed\n" << data << "\n";
                if (progress != old_progress)
                    progress_handler(progress);
                old_progress = progress;
            }
        });
        auto t2 = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
        yakkalog->info("{}: cloned in {}ms", name, duration);

        if (!fs::exists("components/" + name))
            fs::create_directories("components/" + name);
        const std::string checkout_string     = "--git-dir "s + ".yakka"s + "/repos/" + name + "/.git --work-tree components/" + name + " checkout " + branch + " --force";

        // Checkout instance
        t1 = std::chrono::high_resolution_clock::now();
        yakka::exec(GIT_STRING, checkout_string, [&](const std::string& data) -> void {
            std::smatch s;
            if (std::regex_search(data, s, std::regex { R"(\((.*)/(.*)\))" }))
            {
                // yakkalog->info(data);
                int phase_progress = std::stoi( s[1] );
                int end_value = std::stoi( s[2] );
                int progress = phase_rates[GIT_LFS_CHECKOUT] + ((100-phase_rates[GIT_LFS_CHECKOUT])*phase_progress)/end_value;
                progress_handler(progress);
            }
        });
        t2 = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
        yakkalog->info("{}: checkout in {}ms", name, duration);

        progress_handler(100);
    }


}
