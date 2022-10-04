/* A BOB workspace is identified by a .yakka folder
 * The .yakka folder holds information about locally available components, and remote registries
 */
#include "yakka.hpp"
#include "yakka_workspace.hpp"
#include "utilities.hpp"
#include <filesystem>
#include <fstream>
#include <regex>

namespace fs = std::filesystem;

std::string example_registry = "";

namespace yakka
{
    workspace::workspace( )
    {
        configuration["host_os"] = host_os_string;
        configuration["executable_extension"] = executable_extension;
        configuration_json = configuration.as<nlohmann::json>();
    }

    void workspace::init(fs::path workspace_path, fs::path shared_components_path)
    {
        log = spdlog::get("yakkalog");

        this->workspace_path = workspace_path;
        try {
            if (!fs::exists(shared_components_path))
                fs::create_directories(shared_components_path);
            
            this->shared_components_path = shared_components_path;
        }
        catch(...)
        {
        }

        load_config_file(workspace_path / "config.yaml");

        if (!fs::exists(this->workspace_path / ".yakka/registries"))
            fs::create_directories(this->workspace_path / ".yakka/registries");

        if (!fs::exists(this->workspace_path / ".yakka/repos"))
            fs::create_directories(this->workspace_path / ".yakka/repos");

        local_database.load(this->workspace_path);

        if (!this->shared_components_path.empty())
        {
            load_config_file(this->shared_components_path / "/config.yaml");
            shared_database.load(this->shared_components_path);
        }
    }

    void workspace::load_component_registries()
    {
        // Verify the .yakka/registries path exists
        if (!fs::exists(workspace_path / ".yakka/registries"))
            return;

        for ( const auto& p : fs::recursive_directory_iterator( workspace_path / ".yakka/registries") )
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

    std::optional<fs::path> workspace::find_component(const std::string component_dotname)
    {
        const std::string component_id = yakka::component_dotname_to_id(component_dotname);

        // Get component from database
        auto local = local_database[component_id];
        auto shared = shared_database[component_id];

        // Check if that component is in the database
        if (!local && !shared)
            return {};

        auto c = (local) ? local : shared;

        if (c.IsScalar() && fs::exists(c.Scalar()))
            return c.Scalar();
        if (c.IsSequence())
        {
            if ( c.size( ) == 1 )
            {
                if ( fs::exists( c[0].Scalar( ) ) )
                    return c[0].Scalar( );
            }
            else
                log->error("TODO: Parse multiple matches to the same component ID: '{}'", component_id);
        }
        return {};
    }

    std::string workspace::template_render(const std::string input)
    {
        return inja_environment.render(input, configuration_json);
    }

    void workspace::load_config_file(const fs::path config_file_path)
    {
        try
        {
            if (!fs::exists(config_file_path))
                return;
            
            auto configuration = YAML::LoadFile( config_file_path.string() );

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
            log->error("Couldn't read '{}'\n{}", config_file_path.string(), e.what( ));
        }

    }

    std::future<fs::path> workspace::fetch_component(const std::string& name, YAML::Node node, std::function<void(size_t)> progress_handler)
    {
        std::string url    = template_render(node["packages"]["default"]["url"].as<std::string>());
        std::string branch = template_render(node["packages"]["default"]["branch"].as<std::string>());
        const bool shared_components_write_access = (fs::status(shared_components_path).permissions() & fs::perms::owner_write ) == fs::perms::none;
        fs::path git_location = (node["type"] && node["type"].as<std::string>() == "tool" && shared_components_write_access) ? shared_components_path / "repos" : workspace_path / ".yakka/repos";
        fs::path checkout_location = (node["type"] && node["type"].as<std::string>() == "tool" && shared_components_write_access) ? shared_components_path / "repos" / name : workspace_path / "components" / name;
        return std::async(std::launch::async, [=]() {
                return do_fetch_component(name, url, branch, git_location, checkout_location, progress_handler);
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
    fs::path workspace::do_fetch_component(const std::string& name, const std::string url, const std::string branch, const fs::path git_location, const fs::path checkout_location, std::function<void(size_t)> progress_handler)
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
        int retcode;

        try
        {
             if (!fs::exists(git_location)) {
            yakkalog->info("Creating {}", git_location.string());
            fs::create_directories(git_location);
            }

            if (!fs::exists(checkout_location)) {
                yakkalog->info("Creating {}", checkout_location.string());
                fs::create_directories(checkout_location);
            }
        }
        catch(const std::exception& e)
        {
            std::cerr << e.what() << '\n';
            return {};
        }       

        // Of the total time to fetch a Git repo, 10% is allocated to counting, 10% to compressing, and 80% to receiving.
        static const int phase_rates[] = {0, 10, 20, 75, 90};
        const std::string fetch_string = "-C " + git_location.string() + " clone " + url + " " + name + " -b " + branch + " --progress --single-branch --no-checkout";

        auto t1 = std::chrono::high_resolution_clock::now();
        retcode = yakka::exec(GIT_STRING, fetch_string, [&](std::string& data) -> void {
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
        if (retcode != 0) {
            return {};
        }
        auto t2 = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
        yakkalog->info("{}: cloned in {}ms", name, duration);

        const std::string checkout_string     = "--git-dir "s + git_location.string() + "/" + name + "/.git --work-tree " + checkout_location.string() + " checkout " + branch + " --force";

        // Checkout instance
        t1 = std::chrono::high_resolution_clock::now();
        retcode = yakka::exec(GIT_STRING, checkout_string, [&](const std::string& data) -> void {
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
        if (retcode != 0) {
            return {};
        }
        t2 = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
        yakkalog->info("{}: checkout in {}ms", name, duration);

        progress_handler(100);

        return checkout_location;
    }


}
