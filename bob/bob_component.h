#pragma once

#include "bob_blueprint.h"
#include "yaml-cpp/yaml.h"
#include "bob.h"
#include <vector>
#include <string>
#include <iostream>
#include <filesystem>

namespace bob {

    struct base_component
    {
        base_component( ) {}
        virtual ~base_component( ) {}

        void parse_file(std::string filename);
        void apply_feature( std::string feature, component_list_t& new_components, feature_list_t& new_features );
        component_list_t get_required_components();
        feature_list_t   get_required_features();
        std::vector< blueprint_match > get_blueprints();

        // Variables
        std::string filename;
    };
}

namespace bob {

    struct component : public base_component
    {
        component( );

        component( fs::path path );

        virtual ~component( );

        void parse_file( fs::path path );

        void apply_feature( std::string feature_name, component_list_t& new_components, feature_list_t& new_features );

        void process_requirements(const YAML::Node& node, component_list_t& new_components, feature_list_t& new_features );

        component_list_t get_required_components( );
        feature_list_t   get_required_features( );
        std::vector<blueprint_match> get_blueprints( );

        std::string id;
        fs::path path;
        YAML::Node yaml;
    };

} /* namespace bob */

