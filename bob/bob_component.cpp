#include "bob_component.hpp"
#include "blueprint_database.hpp"
#include "spdlog/spdlog.h"

namespace bob
{
    YAML::Node& component::parse_file( fs::path file_path, blueprint_database& database )
    {
        auto boblog = spdlog::get("boblog");
        this->file_path = file_path;
        std::string path_string = file_path.generic_string();
        boblog->info( "Parsing '{}'", path_string);

        try
        {
            yaml = YAML::LoadFile( path_string );
        }
        catch ( std::exception& e )
        {
            boblog->error( "Failed to load file: '{}'\n{}\n", path_string, e.what());
            std::cerr << "Failed to parse: " << path_string << "\n" << e.what() << "\n";
            return yaml;
        }

        // Add known information
        this->id = file_path.stem().string();
        yaml["bob_file"] = path_string;

        if (file_path.has_parent_path())
            path_string = file_path.parent_path().generic_string();
        else
            path_string = ".";
        yaml["directory"] = path_string;

        // Ensure certain nodes are sequences
        if (yaml["requires"]["components"].IsScalar())
        {
            std::string value = yaml["requires"]["components"].Scalar();
            yaml["requires"]["components"] = YAML::Node();
            yaml["requires"]["components"].push_back(value);
        }

        if (yaml["requires"]["features"].IsScalar())
        {
            std::string value = yaml["requires"]["features"].Scalar();
            yaml["requires"]["features"] = YAML::Node();
            yaml["requires"]["features"].push_back(value);
        }

        // Fix relative component addressing
        for( auto n: yaml["requires"]["components"])
            if (n.Scalar().front() == '.')
                n.first = n.as<std::string>().insert(0, path_string);

        for( auto f: yaml["supports"]["features"])
            for( auto n: f["requires"]["components"])
                if (n.Scalar().front() == '.')
                    n.first = n.as<std::string>().insert(0, path_string);

        for( auto c: yaml["supports"]["components"])
            for( auto n: c["requires"]["components"])
                if (n.Scalar().front() == '.')
                    n.first = n.as<std::string>().insert(0, path_string);

        return yaml;
    }

    // std::tuple<component_list_t&, feature_list_t&> component::apply_feature( std::string feature_name)
    // {
    //     if ( yaml["supports"][feature_name] )
    //     {
    //         return process_requirements( yaml["supports"][feature_name] );
    //     }
    //     return {component_list_t(), feature_list_t()};
    //     // if ( yaml["provides"][feature_name] )
    //     // {
    //     //     if ( yaml["provides"][feature_name].IsScalar( ) )
    //     //         new_features.insert( yaml["provides"][feature_name].as<std::string>( ) );
    //     //     else
    //     //         process_requirements( yaml["provides"][feature_name], new_components, new_features );
    //     // }
    // }

    // std::tuple<component_list_t&, feature_list_t&> component::process_requirements(const YAML::Node& node)
    // {
    //     component_list_t new_components;
    //     feature_list_t new_features;
    //     auto boblog = spdlog::get("boblog");
    //     if (!node["requires"])
    //         return {std::move(new_components), std::move(new_features)};

    //     if (node["requires"].IsScalar() || node["requires"].IsSequence())
    //     {
    //         boblog->error( "{}: 'requires' entry is malformed: \n'{}'", yaml["name"].as<std::string>(), node["requires"].as<std::string>());
    //         return {std::move(new_components), std::move(new_features)};
    //     }

    //     try
    //     {
    //         // Process required components
    //         if (node["requires"]["components"])
    //         {
    //             // Add the item/s to the new_component list
    //             if (node["requires"]["components"].IsScalar())
    //                 new_components.insert(node["requires"]["components"].as<std::string>());
    //             else if (node["requires"]["components"].IsSequence())
    //                 for (auto& i: node["requires"]["components"])
    //                     new_components.insert(i.as<std::string>());
    //             else
    //                 boblog->error( "Node '{}' has invalid 'requires'", yaml["name"].as<std::string>());
    //         }


    //         // Process required features
    //         if (node["requires"]["features"])
    //         {
    //             // Add the item/s to the new_features list
    //             if ( node["requires"]["features"].IsScalar( ) )
    //                 new_features.insert( node["requires"]["features"].as<std::string>( ) );
    //             else if ( node["requires"]["features"].IsSequence( ) )
    //                 for ( auto& i : node["requires"]["features"] )
    //                     new_features.insert( i.as<std::string>( ) );
    //             else
    //                 boblog->error("Node '{}' has invalid 'requires'", yaml["name"].as<std::string>());
    //         }
    //     }
    //     catch (YAML::Exception &e)
    //     {
    //         boblog->error( "Failed to process requirements for '{}'\n{}", yaml["name"].as<std::string>(), e.msg);
    //     }

    //     return {std::move(new_components), std::move(new_features)};
    // }

    // component_list_t component::get_required_components( )
    // {
    //     component_list_t list;
    //     for (const auto& c : yaml["requires"]["components"])
    //         list.insert(c.Scalar());

    //     return list;
    // }

    // feature_list_t component::get_required_features( )
    // {
    //     feature_list_t list;
    //     for (const auto& f : yaml["requires"]["features"])
    //         list.insert(f.Scalar());

    //     return list;
    // }
} /* namespace bob */
