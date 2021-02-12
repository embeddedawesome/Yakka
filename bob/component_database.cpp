#include "component_database.h"
#include "bob_component.h"
#include <iostream>
#include <fstream>

namespace bob
{
    const std::string component_database::database_filename = "bob-components.yaml";

    component_database::component_database( fs::path project_home )
    {
        load( project_home );
        database_is_dirty = false;
    }

    component_database::~component_database( )
    {
        if (database_is_dirty)
            save();
    }

    void component_database::insert(const std::string id, fs::path config_file)
    {
        (*this)[id].push_back( config_file.generic_string() );
        database_is_dirty = true;
    }

    void component_database::load( fs::path project_home )
    {
        if ( !fs::exists( database_filename ) )
        {
            scan_for_components( project_home );
            save();
        }
        else
        {
            try
            {
                YAML::Node::operator =( YAML::LoadFile( database_filename ) );
            }
            catch(...)
            {
                std::cerr << "Could not load component database" << std::endl;
            }
        }
    }
    void component_database::save()
    {
        std::ofstream database_file( bob_component_database_filename );
        (*this)["provides"]["features"] = slcc::slcc_database["features"];
        database_file << static_cast<YAML::Node&>(*this);
        database_file.close();
        database_is_dirty = false;
    }

    void component_database::add_component( fs::path path )
    {
        const auto component_id   = path.filename().generic_string();
        const auto component_path = path.generic_string() + "/" + component_id + ".yaml";
        if ( fs::exists( component_path ) )
        {
            ( *this )[component_id].push_back( component_path );
             database_is_dirty = true;
        }
    }

    void component_database::scan_for_components( fs::path path )
    {
        add_component(path);

        if (!fs::exists(path)) return;

        for ( const auto& p : fs::recursive_directory_iterator( path ) )
        {
            if (p.is_directory() )
            {
                add_component(p.path());
            }
            else if (p.path().filename().extension() == ".slcc")
            {
                slcc uc(p.path());
                uc.convert_to_bob();
                (*this)[uc.yaml["id"].as<std::string>()].push_back( p.path().generic_string() );
            }
        }
    }

} /* namespace bob */
