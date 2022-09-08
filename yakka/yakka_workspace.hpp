#pragma once

#include "yaml-cpp/yaml.h"
#include "spdlog/spdlog.h"
#include "inja.hpp"
#include "component_database.hpp"
#include <string>
#include <future>
#include <optional>

namespace yakka
{
    class workspace
    {
    public:
        workspace( std::string path = "." );
        void init();
        std::future<fs::path> fetch_component(const std::string& name, YAML::Node node, std::function<void(size_t)> progress_handler);
        void load_component_registries();
        yakka_status add_component_registry(const std::string& url);
        std::optional<YAML::Node> find_registry_component(const std::string& name);
        std::optional<fs::path> find_component(const std::string component_dotname);
        void load_config_file(const std::string config_filename);
        std::string template_render(const std::string input);
        yakka_status fetch_registry(const std::string& url );
        yakka_status update_component(const std::string& name );

        static fs::path do_fetch_component(const std::string& name, const std::string url, const std::string branch, const std::string git_location, const std::string checkout_location, std::function<void(size_t)> progress_handler);

    public:
        std::shared_ptr<spdlog::logger> log;
        YAML::Node registries;
        YAML::Node configuration;
        nlohmann::json configuration_json;
        std::map<std::string, std::future<void>> fetching_list;
        std::string workspace_directory;
        inja::Environment inja_environment;
        component_database local_database;
        component_database shared_database;
    };
}
