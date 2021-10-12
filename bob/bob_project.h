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
    #elif defined(__APPLE__)
    const std::string host_os_string       = "macos";
    const std::string executable_extension = "";
    #elif defined (__linux__)
    const std::string host_os_string       = "linux";
    const std::string executable_extension = "";
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
        project( const std::vector<std::string>& project_string, std::shared_ptr<spdlog::logger> log );

        virtual ~project( );

        void set_project_directory(const std::string path);
        YAML::Node get_project_summary();
        void parse_project_string( const std::vector<std::string>& project_string );
        void process_requirements(const YAML::Node& node);
        state evaluate_dependencies();
        std::optional<fs::path> find_component(const std::string component_dotname);
        void parse_blueprints();
        void generate_project_summary();

        std::vector<std::unique_ptr<blueprint_match>> find_blueprint_match( const std::string target );
        void evaluate_blueprint_dependencies();
        void load_common_commands();
        void process_construction(indicators::ProgressBar& bar);
        void load_config_file(const std::string config_filename);
        void save_summary();
        void load_component_registries();
        std::optional<YAML::Node> find_registry_component(const std::string& name);
        std::future<void> fetch_component(const std::string& name, indicators::ProgressBar& bar);
        void process_data_dependency(const std::string& path);
        void process_supported_feature(YAML::Node& component, const YAML::Node& node);

        // Logging
        std::shared_ptr<spdlog::logger> log;

        // Basic project data
        std::string project_name;
        std::string bob_home_directory;

        // Component processing
        std::vector<std::string> unprocessed_components;
        std::vector<std::string> unprocessed_features;
        std::unordered_set<std::string> required_components;
        std::unordered_set<std::string> required_features;
        std::unordered_set<std::string> commands;
        std::unordered_set<std::string> unknown_components;
        // std::map<std::string, YAML::Node> remote_components;

        YAML::Node  project_summary;
        std::string project_directory;
        std::vector<std::shared_ptr<bob::component>> components;
        bob::component_database component_database;

        nlohmann::json project_summary_json;
        nlohmann::json configuration_json;

        // Blueprint evaluation
        inja::Environment inja_environment;
        std::multimap<std::string, std::shared_ptr< construction_task > > construction_list;
        std::vector<std::string> todo_list;
        std::unordered_set<std::string> required_data;

        YAML::Node registries;

        std::vector< std::pair<std::string, YAML::Node> > blueprint_list;
        std::map< std::string, blueprint_command > blueprint_commands;


    private:
        void process_aggregate( YAML::Node& aggregate );
    };

    static std::pair<std::string, int> run_command( std::shared_ptr< construction_task> task, const project* project );
    static void yaml_node_merge(YAML::Node& merge_target, const YAML::Node& node);
    static void json_node_merge(nlohmann::json& merge_target, const nlohmann::json& node);
    static std::vector<std::string> parse_gcc_dependency_file(const std::string filename);

} /* namespace bob */

