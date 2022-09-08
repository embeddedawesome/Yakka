#pragma once

#include "yaml-cpp/yaml.h"
#include <filesystem>

namespace fs = std::filesystem;

namespace yakka
{
    class component_database : public YAML::Node
    {
    public:
        component_database( fs::path workspace_path );
        virtual ~component_database( );

        void insert(const std::string id, fs::path config_file);
        void load();
        void save();
        void erase();
        void add_component( fs::path path );
        void scan_for_components(fs::path search_start_path = "");

    private:
        fs::path workspace_path;
        fs::path database_filename;
        bool database_is_dirty;
    };

} /* namespace yakka */

