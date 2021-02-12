#pragma once

#include "yaml-cpp/yaml.h"
#include <filesystem>

namespace fs = std::filesystem;

namespace bob
{
    class component_database : public YAML::Node
    {
    public:
        static const std::string database_filename;

        component_database( fs::path project_home = "." );
        virtual ~component_database( );

        void insert(const std::string id, fs::path config_file);
        void load( fs::path project_home );
        void save();
        void add_component( fs::path path );
        void scan_for_components(fs::path project_home = ".");

    private:
        bool database_is_dirty;
    };

} /* namespace bob */

