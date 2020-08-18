#include "bob_component.h"

namespace bob
{

    component::component( )
    {
    }

    component::~component( )
    {
    }


    component::component( fs::path path )
    {
        parse_file(path);
    }

    void component::parse_file( fs::path path )
    {
        this->path = path;
        std::string path_string = path.generic_string();
        std::clog << "Parsing '" << path_string << "'\n";

        try
        {
            yaml = YAML::LoadFile( path_string );
        }
        catch ( ... )
        {
            std::cerr << "Failed to load file: " << path_string << "\n";
            return;
        }

        // Add known information
        this->id = path.stem().string();
        yaml["bob_file"] = path_string;
        path_string = path.parent_path().generic_string();
        yaml["directory"] = path_string;
        std::replace(path_string.begin(), path_string.end(), '/', '.');
        path_string = path_string.substr(path_string.find_first_not_of('.'));
        yaml["dot_name"] = path_string;

        if (yaml["requires"].IsDefined())
        {
        	if (!yaml["requires"].IsMap())
        		for (const auto& i: yaml["requires"])
        			if (i.IsScalar())
        			{
        				// Assume it's a component
        				yaml["requires"]["components"] = YAML::Node();
        				yaml["requires"]["components"].push_back(i.Scalar());
        			}
        			else if (i.IsSequence())
        			{
        				// Assume each item is a sequence
        				yaml["requires"]["components"] = YAML::Node();
        				for (const auto& a: i)
        					yaml["requires"]["components"].push_back(a.Scalar());
        			}

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
        }


        // Fix relative component addressing
        for( auto n: yaml["requires"]["components"])
            if (n.Scalar().front() == '.')
                n.first = n.as<std::string>().insert(0, path_string);

        for( auto f: yaml["supports"])
            for( auto n: f["requires"]["components"])
                if (n.Scalar().front() == '.')
                    n.first = n.as<std::string>().insert(0, path_string);
    }

    void component::apply_feature( std::string feature_name, component_list_t& new_components, feature_list_t& new_features )
    {
        if ( yaml["supports"][feature_name] )
        {
            process_requirements( yaml["supports"][feature_name], new_components, new_features );
        }

        if ( yaml["provides"][feature_name] )
        {
            if ( yaml["provides"][feature_name].IsScalar( ) )
                new_features.insert( yaml["provides"][feature_name].as<std::string>( ) );
            else
                process_requirements( yaml["provides"][feature_name], new_components, new_features );
        }
    }

    void component::process_requirements(const YAML::Node& node, component_list_t& new_components, feature_list_t& new_features )
    {
        if (!node["requires"])
            return;

        if (node["requires"].IsScalar() || node["requires"].IsSequence())
        {
            std::cerr << yaml["dot_name"] << ": 'requires' entry is malformed: \n'" << node["requires"] << "'\n\n";
            return;
        }

        try
        {
            // Process required components
            if (node["requires"]["components"])
            {
                // Add the item/s to the new_component list
                if (node["requires"]["components"].IsScalar())
                    new_components.insert(node["requires"]["components"].as<std::string>());
                else if (node["requires"]["components"].IsSequence())
                    for (auto& i: node["requires"]["components"])
                        new_components.insert(i.as<std::string>());
                else
                    std::cerr << "Node '" << yaml["dot_name"].as<std::string>() << "' has invalid 'requires'\n";
            }


            // Process required features
            if (node["requires"]["features"])
            {
                std::vector<std::string> new_features;

                // Add the item/s to the new_features list
                if ( node["requires"]["features"].IsScalar( ) )
                    new_features.push_back( node["requires"]["features"].as<std::string>( ) );
                else if ( node["requires"]["features"].IsSequence( ) )
                    for ( auto& i : node["requires"]["features"] )
                        new_features.push_back( i.as<std::string>( ) );
                else
                    std::cerr << "Node '" << yaml["dot_name"].as<std::string>() << "' has invalid 'requires'\n";
            }
        }
        catch (YAML::Exception &e)
        {
            std::cerr << "Failed to process requirements for '" << yaml["dot_name"] << "'\n";
            std::cerr << e.msg << "\n";
        }
    }

    component_list_t component::get_required_components( )
    {
        component_list_t list;
        for (const auto& c : yaml["requires"]["components"])
            list.insert(c.Scalar());

        return list;
    }

    feature_list_t component::get_required_features( )
    {
        feature_list_t list;
        for (const auto& f : yaml["requires"]["features"])
            list.insert(f.Scalar());

        return list;
    }
} /* namespace bob */
