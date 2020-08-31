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

        void parse_file( fs::path file_path);
        void apply_feature( std::string feature, component_list_t& new_components, feature_list_t& new_features );
        component_list_t get_required_components();
        feature_list_t   get_required_features();
        std::vector< blueprint_match > get_blueprints();

        // Variables
        fs::path file_path;
    };

    struct component : public base_component
    {
        component( ) {};

        component( fs::path file_path );

        void parse_file( fs::path file_path );

        void apply_feature( std::string feature_name, component_list_t& new_components, feature_list_t& new_features );

        void process_requirements(const YAML::Node& node, component_list_t& new_components, feature_list_t& new_features );

        component_list_t get_required_components( );
        feature_list_t   get_required_features( );
        std::vector<blueprint_match> get_blueprints( );

        std::string id;
        YAML::Node yaml;
    };

    struct slcc : public base_component
    {
        static YAML::Node slcc_database;

        slcc( ) {};

        slcc( fs::path file_path );

        void parse_file( fs::path file_path );

        void apply_feature( std::string feature_name, component_list_t& new_components, feature_list_t& new_features );

        void process_requirements(const YAML::Node& node, component_list_t& new_components, feature_list_t& new_features );

        void convert_to_bob();

        component_list_t get_required_components( );
        feature_list_t   get_required_features( );
        std::vector<blueprint_match> get_blueprints( );

        YAML::Node yaml;
    };

} /* namespace bob */

