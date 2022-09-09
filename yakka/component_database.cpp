#include "component_database.hpp"
#include "yakka_component.hpp"
#include "spdlog/spdlog.h"
#include "glob/glob.h"
#include <iostream>
#include <fstream>
#include <filesystem>

namespace yakka
{
    component_database::component_database( fs::path workspace_path ) : workspace_path(workspace_path), database_is_dirty(false)
    {
        this->workspace_path = workspace_path;
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

    void component_database::load( )
    {
        database_filename = workspace_path / "yakka-components.yaml";
        auto yakkalog = spdlog::get("yakkalog");
        if ( !fs::exists( database_filename ) )
        {
            scan_for_components( workspace_path );
            save();
        }
        else
        {
            try
            {
                YAML::Node::operator =( YAML::LoadFile( database_filename.string() ) );
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

    void component_database::erase()
    {
        if (!database_filename.empty())
            fs::remove(database_filename);
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

    void component_database::scan_for_components(fs::path search_start_path)
    {
        auto yakkalog = spdlog::get("yakkalog");
        std::vector<std::future<slcc>> parsed_slcc_files;

        if (search_start_path.empty())
            search_start_path = this->workspace_path;
            
        // add_component(path);

        if (!fs::exists(search_start_path))
        {
          yakkalog->error( "Cannot scan for components. Path does not exist: '{}'", search_start_path.generic_string());
          return;
        }

        // for (auto &p : glob::rglob({"**/*.bob", "**/*.yakka"}))
        // {
        //     add_component(p);
        // }
#if 1
        auto rdi = fs::recursive_directory_iterator( search_start_path );
        for ( auto p = fs::begin(rdi); p != fs::end(rdi); ++p )
        {
            // Skip any directories that start with '.'
            if (p->is_directory() && p->path().filename().string().front() == '.') {
                p.disable_recursion_pending();
                continue;
            }

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
