#include "component_database.h"
#include <iostream>
#include <fstream>

namespace bob
{
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
        if ( !fs::exists( bob_component_database_filename ) )
        {
            scan_for_components( project_home );
            save();
        }
        else
        {
            try
            {
                YAML::Node::operator =( YAML::LoadFile( bob_component_database_filename ) );
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
        database_file << static_cast<YAML::Node&>(*this);
        database_file.close();
        database_is_dirty = false;
    }

    void component_database::scan_for_components( fs::path path )
    {
        this->reset();
        for ( const auto& p : fs::recursive_directory_iterator( path ) )
            if (p.is_directory() )
            {
                const auto component_id   = p.path().filename().generic_string();
                const auto component_path = p.path().generic_string() + "/" + component_id + ".yaml";
                if (fs::exists(component_path))
                {
                    (*this)[component_id].push_back( component_path );
                }
            }
    }

} /* namespace bob */
