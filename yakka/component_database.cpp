#include "component_database.hpp"
#include "yakka_component.hpp"
#include "spdlog/spdlog.h"
#include "glob/glob.h"
#include <iostream>
#include <fstream>

namespace yakka
{
    const std::string component_database::database_filename = "yakka-components.yaml";

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
        auto yakkalog = spdlog::get("yakkalog");
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
                yakkalog->error("Could not load component database");
            }
        }
    }
    void component_database::save()
    {
        std::ofstream database_file( database_filename );
#ifdef SLCC_SUPPORT
        if (slcc::slcc_database["features"])
            (*this)["provides"]["features"] = slcc::slcc_database["features"];
#endif
        database_file << static_cast<YAML::Node&>(*this);
        database_file.close();
        database_is_dirty = false;
    }

    void component_database::add_component( fs::path path )
    {
        //const auto component_path = path.generic_string() + "/" + component_id + yakka_component_extension;
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
        auto yakkalog = spdlog::get("yakkalog");
        std::vector<std::future<slcc>> parsed_slcc_files;

        // add_component(path);

        if (!fs::exists(path))
        {
          yakkalog->error( "Cannot scan for components. Path does not exist: '{}'", path.generic_string());
          return;
        }

        for (auto &p : glob::rglob({"**/*.yakka", "**/*.yakka"}))
        {
            add_component(p);
        }
#if 0
        auto rdi = fs::recursive_directory_iterator( path );
        for ( auto p = fs::begin(rdi); p != fs::end(rdi); ++p )
        {
            if (p->path().filename().extension() == yakka_component_extension || p->path().filename().extension() == yakka_component_old_extension)
            {
                add_component(p->path());
            }
#ifdef SLCC_SUPPORT
            else if (p.path().filename().extension() == ".slcc")
            {
                parsed_slcc_files.push_back( std::async(std::launch::async, [](fs::path path) {
                        try
                        {
                            slcc uc(path);
                            uc.convert_to_yakka();
                            return uc;
                        }
                        catch(...)
                        {
                            return slcc();
                        }
                    }, p.path()));
            }
#endif
        }
#ifdef SLCC_SUPPORT
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
#endif
#endif
    }

} /* namespace yakka */
