#pragma once

#include "yaml-cpp/yaml.h"
#include "bob.h"
#include "bob_component.h"
#include "component_database.h"
#include "nlohmann/json.hpp"
#include "inja.hpp"
#include "spdlog/spdlog.h"
#include <indicators/progress_bar.hpp>
#include <filesystem>
#include <regex>
#include <map>
#include <unordered_set>
#include <optional>
#include <functional>

namespace fs = std::filesystem;

namespace bob
{
    const std::string default_output_directory  = "output/";

    #if defined(_WIN64) || defined(_WIN32) || defined(__CYGWIN__)
    const std::string host_os_string       = "windows";
    const std::string executable_extension = ".exe";
    const std::string host_os_path_seperator = ";";
    const auto async_launch_option = std::launch::async|std::launch::deferred;
    #elif defined(__APPLE__)
    const std::string host_os_string       = "macos";
    const std::string executable_extension = "";
    const std::string host_os_path_seperator = ":";
    const auto async_launch_option = std::launch::async|std::launch::deferred; // Unsure
    #elif defined (__linux__)
    const std::string host_os_string       = "linux";
    const std::string executable_extension = "";
    const std::string host_os_path_seperator = ":";
    const auto async_launch_option = std::launch::deferred;
    #endif

    typedef std::function<std::string(std::string, const YAML::Node&, std::string, const nlohmann::json&, inja::Environment&)> blueprint_command;

    class project
    {
    public:
        enum class state {
            PROJECT_HAS_UNKNOWN_COMPONENTS,
            PROJECT_HAS_REMOTE_COMPONENTS,
            PROJECT_VALID
        };

    public:
        project( std::shared_ptr<spdlog::logger> log);

        virtual ~project( );

        void set_project_directory(const std::string path);
        void init_project();
        YAML::Node get_project_summary();
        void parse_project_string( const std::vector<std::string>& project_string );
        void process_requirements(const YAML::Node& node);
        state evaluate_dependencies();
        std::optional<fs::path> find_component(const std::string component_dotname);

        void parse_blueprints();
        void update_summary();
        void generate_project_summary();

        void process_blueprint_target( const std::string target );
        void evaluate_blueprint_dependencies();
        void load_common_commands();
        void set_project_file(const std::string filepath);
        void process_construction(indicators::ProgressBar& bar);
        void load_config_file(const std::string config_filename);
        void save_summary();
        std::optional<YAML::Node> find_registry_component(const std::string& name);
        std::future<void> fetch_component(const std::string& name, indicators::ProgressBar& bar);
        bool has_data_dependency_changed(std::string data_path);

        // Logging
        std::shared_ptr<spdlog::logger> log;

        // Basic project data
        std::string project_name;
        std::string output_path;
        std::string bob_home_directory;

        // Component processing
        std::set<std::string> unprocessed_components;
        std::set<std::string> unprocessed_features;
        std::unordered_set<std::string> required_features;
        std::unordered_set<std::string> commands;
        std::unordered_set<std::string> unknown_components;
        // std::map<std::string, YAML::Node> remote_components;

        YAML::Node  project_summary;
        YAML::Node  previous_summary;
        std::string project_directory;
        std::string project_summary_file;
        fs::file_time_type project_summary_last_modified;
        std::vector<std::shared_ptr<bob::component>> components;
        bob::component_database component_database;

        nlohmann::json project_summary_json;
        nlohmann::json configuration_json;

        // Blueprint evaluation
        inja::Environment inja_environment;
        std::multimap<std::string, std::shared_ptr< blueprint_node > > blueprint_database;
        std::multimap<std::string, std::shared_ptr< construction_task > > construction_list;
        std::vector<std::string> todo_list;

        std::vector< std::pair<std::string, YAML::Node> > blueprint_list;
        std::map< std::string, blueprint_command > blueprint_commands;
    };

    std::string try_render(inja::Environment& env, const std::string& input, const nlohmann::json& data, std::shared_ptr<spdlog::logger> log);
    static std::pair<std::string, int> run_command( std::shared_ptr< construction_task> task, const project* project );
    static void yaml_node_merge(YAML::Node& merge_target, const YAML::Node& node);
    static void json_node_merge(nlohmann::json& merge_target, const nlohmann::json& node);
    static std::vector<std::string> parse_gcc_dependency_file(const std::string filename);
} /* namespace bob */

