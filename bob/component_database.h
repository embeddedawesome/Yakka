#pragma once

#include "yaml-cpp/yaml.h"
#include <filesystem>

namespace fs = std::filesystem;

namespace bob
{
    class component_database : public YAML::Node
    {
        const std::string bob_component_database_filename  = "bob-components.yaml";

    public:
        component_database( fs::path project_home = "." );
        virtual ~component_database( );

        void insert(const std::string id, fs::path config_file);
        void load( fs::path project_home );
        void save();
        void scan_for_components(fs::path project_home);

    private:
        bool database_is_dirty;
    };

} /* namespace bob */

