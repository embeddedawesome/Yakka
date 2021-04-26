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
        std::ofstream database_file( database_filename );
        if (slcc::slcc_database["features"])
            (*this)["provides"]["features"] = slcc::slcc_database["features"];
        database_file << static_cast<YAML::Node&>(*this);
        database_file.close();
        database_is_dirty = false;
    }

    void component_database::add_component( fs::path path )
    {
        //const auto component_path = path.generic_string() + "/" + component_id + bob_component_extension;
        if ( fs::exists( path ) )
        {
            const auto component_id   = path.filename().replace_extension().generic_string();
            if (!( *this )[component_id] && ( *this )[component_id].size() == 0)
            {
                ( *this )[component_id].push_back( path.generic_string() );
                database_is_dirty = true;
            }
        }
    }

    void component_database::scan_for_components( fs::path path )
    {
        std::vector<std::future<slcc>> parsed_slcc_files;

        // add_component(path);

        if (!fs::exists(path)) return;

        for ( const auto& p : fs::recursive_directory_iterator( path ) )
        {
            if (p.path().filename().extension() == bob_component_extension)
            {
                add_component(p.path());
            }
            else if (p.path().filename().extension() == ".slcc")
            {
                parsed_slcc_files.push_back( std::async(std::launch::async, [](fs::path path) {
                        try
                        {
                            slcc uc(path);
                            uc.convert_to_bob();
                            return uc;
                        }
                        catch(...)
                        {
                            return slcc();
                        }
                    }, p.path()));
            }
        }

        // TODO: SLCC database should be initialized elsewhere
        if (!slcc::slcc_database["features"])
            slcc::slcc_database["features"] = YAML::Node();

        if (!slcc::slcc_database["components"])
            slcc::slcc_database["components"] = YAML::Node();

        for (auto& file: parsed_slcc_files)
        {
            file.wait();
            const auto slcc = file.get();
            if (slcc.yaml["id"])
            {
                const auto id = slcc.yaml["id"].as<std::string>();
                (*this)[id].push_back( slcc.file_path.generic_string() );

                for (const auto& p: slcc.yaml["provides"])
                    if (p.IsDefined())
                        slcc::slcc_database["features"][p["name"]] = id;

                slcc::slcc_database["components"][id] = slcc.yaml;
            }
        }
    }

} /* namespace bob */
