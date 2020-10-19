#pragma once

#include "yaml-cpp/yaml.h"
#include "bob.h"
#include "bob_component.h"
#include "component_database.h"
#include "nlohmann/json.hpp"
#include "inja.hpp"
#include "ctpl.h"
#include <filesystem>
#include <regex>
#include <map>
#include <optional>

namespace fs = std::filesystem;

namespace bob
{
    const std::string bob_component_extension   = ".yaml";
    const std::string refresh_database_command    = "refresh-db";
    const std::string default_output_directory    = "output/";
    const std::string host_os_string              = "windows";

    typedef std::function<std::string(std::string, const YAML::Node&, std::string, const nlohmann::json&, inja::Environment&)> blueprint_command;

    class project
    {
    public:
        project( );
        project(  std::vector<std::string>& project_string );

        virtual ~project( );

        void set_project_directory(const std::string path);
        YAML::Node get_project_summary();
        void parse_project_string( std::vector<std::string>& project_string );
        void evaluate_dependencies();
        std::optional<fs::path> find_component(const std::string component_dotname);
        void parse_blueprints();
        void process_aggregates();
        std::vector<std::unique_ptr<blueprint_match>> find_blueprint_match( const std::string target );
        void evalutate_blueprint_dependencies();
        void load_common_commands();
        void process_construction();
        void load_config_file(const std::string config_filename);
        void save_summary();
        void load_component_registries();
        std::optional<YAML::Node> find_registry_component(const std::string& name);
        bool fetch_component(YAML::Node& node);
        std::future<void> fetch_component(const std::string& name);

        // Basic project data
        std::string project_name;
        std::string bob_home_directory;

        // Component processing
        std::set<std::string> required_components;
        std::set<std::string> required_features;
        std::set<std::string> commands;

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

        YAML::Node registries;

        std::vector< std::pair<std::string, YAML::Node> > blueprint_list;
        std::map< std::string, blueprint_command> blueprint_commands;

        // Local thread execution pool
        ctpl::thread_pool thread_pool;

    private:
        void process_aggregate( YAML::Node& aggregate );
    };

    static void run_command( int id, std::shared_ptr< construction_task> task, const project* project );
    static void yaml_node_merge(YAML::Node merge_target, const YAML::Node& node);
    static std::vector<std::string> parse_gcc_dependency_file(const std::string filename);

} /* namespace bob */

