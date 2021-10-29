#include "bob_project.h"
#include "spdlog/spdlog.h"
#include <fstream>
#include <chrono>
#include <thread>

namespace bob
{
    using namespace std::chrono_literals;


    project::project( std::shared_ptr<spdlog::logger> log ) : project_directory("."), bob_home_directory("/.bob")
    {
        this->log = log;
        load_config_file("config.yaml");
    }

    project::~project( )
    {
    }

    void project::set_project_directory(const std::string path)
    {
        project_directory = path;
    }

    void project::init_project()
    {
        project_name = bob::generate_project_name(this->unprocessed_components, this->unprocessed_features);
        output_path  = bob::default_output_directory;
        project_summary_file = "output/" + project_name + "/bob_summary.yaml";
        previous_summary["components"] = YAML::Node();

        if (fs::exists(project_summary_file))
        {
            project_summary_last_modified = fs::last_write_time(project_summary_file);
            project_summary = YAML::LoadFile(project_summary_file);

            // Remove the known components from unprocessed_components
            component_list_t temp_components;
            for (const auto& c: unprocessed_components)
                if (!project_summary["components"][c])
                    temp_components.insert(c);
            feature_list_t temp_features;
            // for (const auto& c: unprocessed_features)
            //     if (!project_summary["features"][c])
            //         temp_features.insert(c);
            unprocessed_components = std::move(temp_components);
            // unprocessed_features = std::move(temp_features);

            update_summary();
        }
        else
            fs::create_directories(output_path);
    }

    YAML::Node project::get_project_summary()
    {
        return project_summary;
    }

    void project::process_requirements(const YAML::Node& node)
    {
        if (node.IsScalar () || node.IsSequence ())
        {
            log->error("Node 'requires' entry is malformed: '{}'", node.Scalar());
            return;
        }

        try
        {
            // Process required components
            if (node["components"])
            {
                // Add the item/s to the new_component list
                if (node["components"].IsScalar())
                    unprocessed_components.insert(node["components"].as<std::string>());
                else if (node["components"].IsSequence())
                    for (auto &i : node["components"])
                        unprocessed_components.insert(i.as<std::string>());
                else
                    log->error("Node '{}' has invalid 'requires'", node.Scalar());
            }

            // Process required features
            if (node["features"])
            {
                std::vector<std::string> new_features;

                // Add the item/s to the new_features list
                if (node["features"].IsScalar())
                    unprocessed_features.insert(node["features"].as<std::string>());
                else if (node["features"].IsSequence())
                    for (auto &i : node["features"])
                        unprocessed_features.insert(i.as<std::string>());
                else
                    log->error("Node '{}' has invalid 'requires'", node.Scalar());
            }
        }
        catch (YAML::Exception &e)
        {
            log->error("Failed to process requirements for '{}'\n{}", node.Scalar(), e.msg);
        }
    }


    void project::update_summary()
    {
        // Check if any component files have been modified
        for (const auto& c: project_summary["components"])
        {
            auto bob_file = c.second["bob_file"].as<std::string>();
            const auto name = c.first.as<std::string>();
            if ( fs::last_write_time(bob_file) > project_summary_last_modified)
            {
                // If so, move existing data to previous summary
                previous_summary["components"][name] = std::move(c.second); // TODO: Verify this is correct way to do this efficiently
                project_summary["components"][name] = {};
                unprocessed_components.insert(name);
            }
            else
            {
                // Previous summary should point to the same object
                previous_summary["components"][name] = c.second;

                auto a = previous_summary["components"][name];
                auto b = project_summary["components"][name];

            }
        }
    }

    /**
     * @brief Processes all the @ref unprocessed_components and @ref unprocessed_features, adding items to @ref unknown_components if they are not in the component database
     *        It is assumed the caller will process the @ref unknown_components before adding them back to @ref unprocessed_component and calling this again.
     * @return project::state
     */
    project::state project::evaluate_dependencies()
    {
        // Start processing all the required components and features
        while ( !unprocessed_components.empty( ) || !unprocessed_features.empty( ))
        {
            // Loop through the list of unprocessed components.
            // Note: Items will be added to unprocessed_components during processing
            std::unordered_set<std::string> temp_component_list = std::move(unprocessed_components);
            for (const auto& i: temp_component_list)
            {
                // Convert string to id
                const auto c = bob::component_dotname_to_id(i);

                // Find the component in the project component database
                auto component_path = find_component(c);
                if ( !component_path )
                {
                    // log->info("{}: Couldn't find it", c);
                    unknown_components.insert(c);
                    continue;
                }

                // Add component to the required list and continue if this is not a new component
                if ( project_summary["components"][c] && !project_summary["components"][c].IsNull())
                {
                    log->info("{}: Component already processed", c);
                    continue;
                }

                std::shared_ptr<bob::component> new_component;
                try
                {
                    new_component = std::make_shared<bob::component>( component_path.value() );
                    components.push_back( new_component );
                }
                catch ( std::exception e )
                {
                    log->error("Failed to parse: {}\n{}", c, e.what());
                    project_summary["components"].remove(c);
                    //unknown_components.insert(c);
                }

                // Add all the required components into the unprocessed list
                for (const auto& r : new_component->yaml["requires"]["components"])
                    unprocessed_components.insert(r.Scalar());

                // Add all the required features into the unprocessed list
                for (const auto& f : new_component->yaml["requires"]["features"])
                    unprocessed_features.insert(f.Scalar());

                // Process all the currently required features. Note new feature will be processed in the features pass
                for ( auto& f : required_features )
                    if ( new_component->yaml["supports"][f] && new_component->yaml["supports"][f]["requires"] )
                        process_requirements(new_component->yaml["supports"][f]["requires"]);
            }


            // Process all the new features
            // Note: Items will be added to unprocessed_features during processing
            std::unordered_set<std::string> temp_feature_list = std::move(unprocessed_features);
            for (const auto& f: temp_feature_list)
            {
                // Insert feature and continue if this is not a new feature
                if ( required_features.insert( f ).second == false )
                    continue;

                // Update each component with the new feature
                for ( auto& c : components )
                    if ( c->yaml["supports"][f] && c->yaml["supports"][f]["requires"])
                        process_requirements(c->yaml["supports"][f]["requires"]);
            }
        };

        if (unknown_components.size() != 0) return project::state::PROJECT_HAS_UNKNOWN_COMPONENTS;

        return project::state::PROJECT_VALID;
    }

    void project::generate_project_summary()
    {
        // Add standard information into the project summary
        project_summary["project_name"]   = project_name;
        project_summary["project_output"] = default_output_directory + project_name;
        project_summary["configuration"]["host_os"] = host_os_string;
        project_summary["configuration"]["executable_extension"] = executable_extension;

        if (!project_summary["tools"])
          project_summary["tools"] = YAML::Node();

        // Put all YAML nodes into the summary
        for (const auto& c: components)
        {
            project_summary["components"][c->id] = c->yaml;
            for (auto tool: c->yaml["tools"])
            {
                inja::Environment inja_env = inja::Environment();
                inja_env.add_callback("curdir", 0, [&c](const inja::Arguments& args) { return c->yaml["directory"].Scalar();});

                project_summary["tools"][tool.first.Scalar()] = try_render(inja_env, tool.second.Scalar(), configuration_json, log);
            }
        }

        // Process all the supported features by merging their content with the parent component
        for ( auto c : project_summary["components"] )
        	if (c.second["supports"].IsDefined())
				for ( auto f : this->required_features )
					if ( c.second["supports"][f].IsDefined() )
						yaml_node_merge( c.second, c.second["supports"][f] );


        project_summary["features"] = {};
        for (const auto& i: this->required_features)
        	project_summary["features"].push_back(i);

        project_summary_json = project_summary.as<nlohmann::json>();
        project_summary_json["data"] = nlohmann::json::object();
        project_summary_json["host"] = nlohmann::json::object();
        project_summary_json["host"]["name"] = host_os_string;
    }

    std::optional<fs::path> project::find_component(const std::string component_dotname)
    {
        const std::string component_id = bob::component_dotname_to_id(component_dotname);

        // Get component from database
        const auto& c = component_database[component_id];

        // Check if that component is in the database
        if (!c)
            return {};

        if (c.IsScalar() && fs::exists(c.Scalar()))
            return c.Scalar();
        if (c.IsSequence())
        {
            if ( c.size( ) == 1 )
            {
                if ( fs::exists( c[0].Scalar( ) ) )
                    return c[0].Scalar( );
            }
            else
                log->error("TODO: Parse multiple matches to the same component ID: '{}'", component_id);
        }
        return {};
    }

    void project::parse_blueprints()
    {
        for ( auto c: project_summary["components"])
            for ( auto b: c.second["blueprints"] )
            {
                std::string blueprint_string = try_render(inja_environment,  b.second["regex"] ? b.second["regex"].Scalar() : b.first.Scalar( ), project_summary_json, log);
                log->info("Blueprint: {}", blueprint_string);
                b.second["bob_parent_path"] = c.second["directory"];
                blueprint_list.insert( blueprint_list.end( ), { blueprint_string, b.second } );
            }
    }

    void project::evaluate_blueprint_dependencies()
    {
        std::vector<std::string> new_targets;
        std::unordered_set<std::string> processed_targets;
        std::vector<std::string> unprocessed_targets;

        for (const auto& c: commands)
            unprocessed_targets.push_back(c);

        while (!unprocessed_targets.empty())
        {
            for (const auto& t: unprocessed_targets)
            {
                // Add to processed targets and check if it's already been processed
                if (processed_targets.insert(t).second == false)
                    continue;

                // Check if target is not in the database. Note blueprint_database is a multimap
                auto blueprints = blueprint_database.equal_range(t);
                if (blueprints.first == blueprints.second)
                {
                    // If the target is data, add it to the data dependency list
                    if (t.front() == data_dependency_identifier)
                        blueprint_database.insert(std::make_pair(t, std::make_shared<blueprint_node>(t)));
                    else
                        process_blueprint_target(t);

                    blueprints = blueprint_database.equal_range(t);
                }

                for (auto& i = blueprints.first; i != blueprints.second; ++i)
                {
                    // Add dependencies of the target to the list of unprocessed targets
                    new_targets.insert(new_targets.end(), i->second->dependencies.begin(), i->second->dependencies.end());

                    // Create a task it to the list of things to build
                    auto task = construction_list.insert(std::make_pair(t, std::make_shared<construction_task>()));
                    task->second->blueprint = i->second;

                    // If a file with the target name exists, get the last modified timestamp
                    if ( fs::exists( t ) )
                        task->second->last_modified = fs::last_write_time( t );
                }
                todo_list.push_back(t);
            }

            // unprocessed_targets = std::move(new_targets);
            unprocessed_targets.clear();
            unprocessed_targets.swap(new_targets);
        }
    }

    bool project::has_data_dependency_changed(std::string data_path)
    {
        std::vector<std::pair<YAML::Node,YAML::Node>> changed_nodes;

        if (data_path.front() != data_dependency_identifier)
            return false;

        try
        {
            // Check for wildcard or component name
            if (data_path[1] == data_wildcard_identifier)
            {
                if (data_path[2] != '.')
                {
                    log->error("Data dependency malformed: {}", data_path);
                    return false;
                }

                for (const auto& i: project_summary["components"])
                {
                    std::string component_name = i.first.as<std::string>();
                    if (!(previous_summary["components"][component_name] == project_summary["components"][component_name] ))
                        changed_nodes.push_back({project_summary["components"][component_name], previous_summary["components"][component_name]});
                }
                data_path = data_path.substr(3);
            }
            else
            {
                std::string component_name = data_path.substr(1, data_path.find_first_of('.')-1);
                data_path = data_path.substr(data_path.find_first_of('.')+1);
                if (!(previous_summary["components"][component_name] == project_summary["components"][component_name] ))
                    changed_nodes.push_back({project_summary["components"][component_name], previous_summary["components"][component_name]});
            }

            // Check if we have any nodes that have changed
            if (changed_nodes.size() == 0)
                return false;

            // Check every data path for every changed node
            for (const auto& n: changed_nodes)
            {
                auto first = yaml_path(n.first, data_path);
                auto second = yaml_path(n.second, data_path);
                if (yaml_diff(first, second))
                    return true;
            }
            return false;
        }
        catch(const std::exception& e)
        {
            log->error("Failed to determine data dependency: {}", e.what());
            return false;
        }
    }

    void project::process_blueprint_target( const std::string target )
    {
        bool blueprint_match_found = false;

        for ( auto& blueprint : blueprint_list )
        {
            std::smatch s;

            // Check if rule is a regex, otherwise do a string comparison
            if ( blueprint.second["regex"] )
            {
                if (!std::regex_match(target, s, std::regex { blueprint.first } ) )
                    continue;
            }
            else
            {
                if (target != blueprint.first )
                    continue;
            }

            // Found a match. Create a blueprint match object
            blueprint_match_found = true;
            auto match = std::make_shared<blueprint_node>(target);
            match->blueprint = blueprint.second;

            // Process match
            if ( blueprint.second["regex"] )
            {
                // arg_count starts at 0 as the first match is the entire string
                int arg_count = 0;
                for ( auto& regex_match : s )
                {
                    match->regex_matches[arg_count] = regex_match.str( );
                    ++arg_count;
                }
            }
            else
            {
                match->regex_matches[0] = target;
            }

            inja::Environment local_inja_env;
            local_inja_env.add_callback("$", 1, [&match](const inja::Arguments& args) {
                        return match->regex_matches[ args[0]->get<int>() ];
                    });

            local_inja_env.add_callback("curdir", 0, [&match](const inja::Arguments& args) { return match->blueprint["bob_parent_path"].Scalar();});

            // Run template engine on dependencies
            for ( auto d : blueprint.second["depends"] )
            {
                // Check for special dependency_file condition
                if ( d.IsMap( ) )
                {
                    if ( d["dependency_file"] )
                    {
                        const std::string generated_dependency_file = try_render(local_inja_env,  d.begin()->second.Scalar(), project_summary_json, log );
                        auto dependencies = parse_gcc_dependency_file(generated_dependency_file);
                        match->dependencies.insert( std::end( match->dependencies ), std::begin( dependencies ), std::end( dependencies ) );
                        continue;
                    }
                    else if (d["data"] )
                    {
                        for (const auto& data_item: d["data"])
                        {
                            std::string data_name = try_render(local_inja_env, data_item.as<std::string>(), project_summary_json, log);
                            if (data_name.front() != data_dependency_identifier)
                                data_name.insert(0,1, data_dependency_identifier);
                            match->dependencies.push_back(data_name);
                        }
                        continue;
                    }
                }
                // Verify validity of dependency
                else if ( !d.IsScalar( ) )
                {
                    log->error("Dependencies should be Scalar");
                    return;
                }

                std::string depend_string = d.Scalar( );

                // Generate full dependency string by applying template engine
                std::string generated_depend;
                try
                {
                    generated_depend = local_inja_env.render( depend_string, project_summary_json );
                }
                catch ( std::exception& e )
                {
                    log->error("Couldn't apply template: '{}'", depend_string);
                    return;
                }

                // Check if the input was a YAML array construct
                if ( depend_string.front( ) == '[' && depend_string.back( ) == ']' )
                {
                    // Load the generated dependency string as YAML and push each item individually
                    YAML::Node generated_node = YAML::Load( generated_depend );
                    for ( auto i : generated_node )
                        match->dependencies.push_back( i.Scalar( ) );
                }
                else
                {
                    match->dependencies.push_back( generated_depend );
                }
            }

            blueprint_database.insert(std::make_pair(target, match));
        }

        if (!blueprint_match_found)
        {
            if (fs::exists( target ))
                blueprint_database.insert(std::make_pair(target, std::make_shared<blueprint_node>(target)));
            else
                log->info("No blueprint for '{}'", target);
        }
    }

    void project::load_common_commands()
    {
        blueprint_commands["echo"] = []( std::string target, const YAML::Node& command, std::string captured_output, const nlohmann::json& generated_json, inja::Environment& inja_env) -> std::string {
            auto console = spdlog::get("bobconsole");
            auto boblog = spdlog::get("boblog");
            if (!command.begin()->second.IsNull())
                captured_output = try_render(inja_env, command.begin()->second.as<std::string>(), generated_json, boblog);

            console->info("{}", captured_output);
            return captured_output;
        };


        blueprint_commands["execute"] = []( std::string target, const YAML::Node& command, std::string captured_output, const nlohmann::json& generated_json, inja::Environment& inja_env) -> std::string {
            if (command.begin()->second.IsNull())
                return "";
            auto boblog = spdlog::get("boblog");
            auto console = spdlog::get("bobconsole");
            std::string temp = command.begin( )->second.as<std::string>( );
            try {
                captured_output = inja_env.render( temp, generated_json );
                //std::replace( captured_output.begin( ), captured_output.end( ), '/', '\\' );
                boblog->info("Executing '{}'", captured_output);
                auto [temp_output, retcode] = exec( captured_output, std::string( "" ) );

                if (retcode < 0 && temp_output.length( ) != 0) {
                    console->error( temp_output );
                    boblog->error( "\n{} returned {}\n{}", captured_output, retcode, temp_output);
                }
                else if ( temp_output.length( ) != 0 )
                    boblog->info("{}", temp_output);
                return temp_output;
            }
            catch (std::exception& e)
            {
                boblog->error( "Failed to execute: {}", temp);
                captured_output = "";
                return "";
            }
        };

        blueprint_commands["fix_slashes"] = []( std::string target, const YAML::Node& command, std::string captured_output, const nlohmann::json& generated_json, inja::Environment& inja_env) -> std::string {
            std::replace(captured_output.begin(), captured_output.end(), '\\', '/');
            return captured_output;
        };


        blueprint_commands["regex"] = []( std::string target, const YAML::Node& command, std::string captured_output, const nlohmann::json& generated_json, inja::Environment& inja_env) -> std::string {
            std::regex regex_search(command["search"].as<std::string>());
            if (command["split"])
            {
                std::istringstream ss(captured_output);
                std::string line;
                captured_output = "";
                int count = 0;

                while (std::getline(ss, line))
                {
                  if (command["replace"])
                  {
                      std::string r = std::regex_replace(line, regex_search, command["replace"].as<std::string>(), std::regex_constants::format_no_copy);
                      captured_output.append(r);
                  }
                  else if (command["to_yaml"])
                  {
                    std::smatch s;
                    if (!std::regex_match(line, s, regex_search))
                      continue;
                    YAML::Node node;
                    node[0] = YAML::Node();
                    int i=1;
                    for ( auto& v : command["to_yaml"] )
                      node[0][v.Scalar()] = s[i++].str();

                    captured_output.append(YAML::Dump(node));
                    captured_output.append("\n");
                  }
                }
            }
            else
            {
                captured_output = std::regex_replace(captured_output, regex_search, command["replace"].as<std::string>());
            }
            return captured_output;
        };


        blueprint_commands["inja"] = []( std::string target, const YAML::Node& command, std::string captured_output, const nlohmann::json& generated_json, inja::Environment& inja_env) -> std::string {
            auto boblog = spdlog::get("boblog");
            try
            {
                if (command["file"])
                {
                    std::string filename = try_render(inja_env, command["file"].as<std::string>(), generated_json, boblog);

                    if (!fs::exists(filename))
                        boblog->error( "{} not found when trying to apply template engine", filename);
                    else
                        try
                        {
                            captured_output = inja_env.render_file(filename, generated_json);
                        }
                        catch(std::exception&e )
                        {
                            boblog->error("Template error in {}: {}", filename, e.what());
                        }
                }
                else
                {
                    const auto& node = (command["template"]) ? command["template"] : command["inja"];
                    captured_output = try_render(inja_env, node.Scalar(), generated_json, boblog);
                }
            }
            catch (std::exception &e)
            {
                boblog->error("Failed to apply template: {}\n{}", command.Scalar(), e.what());
            }
            return captured_output;
        };

        blueprint_commands["save"] = []( std::string target, const YAML::Node& command, std::string captured_output, const nlohmann::json& generated_json, inja::Environment& inja_env) -> std::string {
            std::string save_filename;
            auto boblog = spdlog::get("boblog");

            if (command.begin()->second.IsNull())
                save_filename = target;
            else
                save_filename = try_render(inja_env, command.begin()->second.as<std::string>(), generated_json, boblog);

            try
            {
                std::ofstream save_file;
                fs::path p(save_filename);
                if (!p.parent_path().empty())
                  fs::create_directories(p.parent_path());
                save_file.open(save_filename);
                save_file << captured_output;
                save_file.close();
            }
            catch (std::exception& e)
            {
                boblog->error("Failed to save file: '{}'", save_filename);
                captured_output = "";
            }
            return captured_output;
        };

        blueprint_commands["create_directory"] = [ ]( std::string target, const YAML::Node& command, std::string captured_output, const nlohmann::json& generated_json, inja::Environment& inja_env ) -> std::string {
            auto boblog = spdlog::get("boblog");
            if (!command.begin()->second.IsNull())
            {
                std::string filename = "";
                try
                {
                    filename = command.begin()->second.as<std::string>( );
                    filename = try_render(inja_env, filename, generated_json, boblog);
                    if ( !filename.empty( ) )
                    {
                        fs::path p( filename );
                        fs::create_directories( p.parent_path( ) );
                    }
                }
                catch ( std::exception e )
                {
                    boblog->error( "Couldn't create directory for '{}'", filename);
                }
            }
            return "";
        };

        blueprint_commands["verify"] = [ ]( std::string target, const YAML::Node& command, std::string captured_output, const nlohmann::json& generated_json, inja::Environment& inja_env ) -> std::string {
            auto boblog = spdlog::get("boblog");
            std::string filename = command.begin()->second.as<std::string>( );
            filename = try_render(inja_env, filename, generated_json, boblog);
            if (fs::exists(filename))
                boblog->info("{} exists", filename);
            else
                boblog->info("BAD!! {} doesn't exist", filename);
            return captured_output;
        };

        blueprint_commands["rm"] = [ ]( std::string target, const YAML::Node& command, std::string captured_output, const nlohmann::json& generated_json, inja::Environment& inja_env ) -> std::string {
            auto boblog = spdlog::get("boblog");
            std::string filename = command.begin()->second.as<std::string>( );
            filename = try_render(inja_env, filename, generated_json, boblog);
            fs::remove(filename);
            return captured_output;
        };

        blueprint_commands["pack"] = [ ]( std::string target, const YAML::Node& command, std::string captured_output, const nlohmann::json& generated_json, inja::Environment& inja_env ) -> std::string {
            auto boblog = spdlog::get("boblog");
            std::string format = command["format"].as<std::string>();
            format = try_render(inja_env, format, generated_json, boblog);

            std::string pack_arguments = "-ie pack(\"" + format + "\"";

            for (const auto& v: command["values"])
              pack_arguments += ", " + try_render(inja_env, v.as<std::string>(), generated_json, boblog);

            // auto output, ret = bob::exec("perl", pack_arguments);

            return captured_output;
        };
    }

    static std::pair<std::string, int> run_command( std::shared_ptr< construction_task> task, const project* project )
    {
        auto boblog = spdlog::get("boblog");
        std::string captured_output;
        inja::Environment inja_env = inja::Environment();
        auto& blueprint = task->blueprint;

        inja_env.add_callback("$", 1, [&blueprint](const inja::Arguments& args) {
            return blueprint->regex_matches[ args[0]->get<int>() ];
        });

        inja_env.add_callback("curdir", 0, [&blueprint](const inja::Arguments& args) { return blueprint->blueprint["bob_parent_path"].Scalar();});
        inja_env.add_callback("filesize", 1, [&blueprint](const inja::Arguments& args) { return fs::file_size(args[0]->get<std::string>());});


        if ( !blueprint->blueprint["process"].IsSequence())
        {
            boblog->error("Error: process nodes must be sequences: {}", blueprint->blueprint["process"].Scalar());
            return {"", -1};
        }

        std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();

        // Note: A blueprint process is a sequence of maps
        for ( const auto command_entry : blueprint->blueprint["process"] )
        {
            // Take the first entry in the map as the command
            auto        command      = command_entry.begin();
            std::string command_name = command->first.as<std::string>();

            try
            {
                // Verify tool exists
                if (project->project_summary["tools"][command_name].IsDefined())
                {
                    YAML::Node tool = project->project_summary["tools"][command_name];
                    std::string command_text = "";

                    command_text.append( tool.as<std::string>( ) );

                    std::string arg_text = command->second.as<std::string>( );

                    // Apply template engine
                    arg_text = try_render(inja_env, arg_text, project->project_summary_json, boblog);

                    auto[temp_output, retcode] = exec(command_text, arg_text);

                    if (retcode < 0)
                    {
                      boblog->error("Returned {}\n{}",retcode, temp_output);
                      return {temp_output, retcode};
                    }
                    captured_output = temp_output;
                    // Echo the output of the command
                    // TODO: Note this should be done by the main thread to ensure the outputs from multiple run_command instances don't overlap
                    boblog->info(captured_output);
                }
                else if (project->blueprint_commands.find(command_name) != project->blueprint_commands.end()) // To be replaced with .contains() once C++20 is available
                {
                    captured_output = project->blueprint_commands.at(command_name)( blueprint->target, command_entry, captured_output, project->project_summary_json, inja_env );
                }
                else
                {
                    boblog->error("{} tool doesn't exist", command_name);
                }

            }
            catch ( std::exception& e )
            {
                boblog->error("Failed to run command: '{}' as part of {}", command_name, blueprint->target);
                boblog->info( "Failed to run: {}", command_entry.Scalar());
                throw e;
            }
        }

        std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
        boblog->info( "{}: {} milliseconds", blueprint->target, duration);
        return {captured_output, 0};
    }

    static void json_node_merge(nlohmann::json& merge_target, const nlohmann::json& node)
    {
        auto boblog = spdlog::get("boblog");
        switch(node.type())
        {
            case nlohmann::detail::value_t::object:
                switch(merge_target.type())
                {
                    case nlohmann::detail::value_t::object:
                    case nlohmann::detail::value_t::array:
                    default:
                        boblog->error("Currently not supported"); break;
                }
                break;
            case nlohmann::detail::value_t::array:
                switch(merge_target.type())
                {
                    case nlohmann::detail::value_t::object:
                        boblog->error("Cannot merge array into an object"); break;
                    case nlohmann::detail::value_t::array:
                        for (auto& i: node)
                            merge_target.push_back(i);
                        break;
                    default:
                        merge_target.push_back(node); break;
                }
                break;
            default:
                switch(merge_target.type())
                {
                    case nlohmann::detail::value_t::object:
                        boblog->error("Cannot merge scalar into an object"); break;
                    case nlohmann::detail::value_t::array:
                    default:
                        merge_target.push_back(node); break;
                }
                break;
        }
    }

    static void yaml_node_merge(YAML::Node& merge_target, const YAML::Node& node)
    {
        auto boblog = spdlog::get("boblog");
        for (const auto& i : node)
        {
            const std::string item_name = i.first.as<std::string>();
            YAML::Node        item_node = i.second;

            if ( !merge_target[item_name] )
            {
                merge_target[item_name] = item_node;
            }
            else
            {
                if (item_node.IsScalar())
                {
                    if (merge_target[item_name].IsScalar())
                    {
                        YAML::Node new_node;
                        new_node.push_back(merge_target[item_name]);
                        new_node.push_back(item_node.Scalar());
                        merge_target[item_name].reset(new_node);
                    }
                    else if (merge_target[item_name].IsSequence())
                    {
                        merge_target[item_name].push_back(item_node.Scalar());
                    }
                    else
                    {
                        boblog->error("Cannot merge scalar and map\nScalar: {}\nMap: {}", i.first.as<std::string>(), merge_target[item_name].as<std::string>());
                        return;
                    }
                }
                else if (item_node.IsSequence())
                {
                    if (merge_target[item_name].IsMap())
                    {
                        boblog->error("Cannot merge sequence and map\n{}\n{}", merge_target.as<std::string>(), node.as<std::string>());
                        return;
                    }
                    if (merge_target[item_name].IsScalar())
                    {
                        // Convert merge_target from a scalar to a sequence
                        YAML::Node new_node;
                        new_node.push_back( merge_target[item_name].Scalar() );
                        merge_target[item_name] = new_node;
                    }
                    for (auto a : item_node)
                    {
                        merge_target[item_name].push_back(a);
                    }
                }
                else if (item_node.IsMap())
                {
                    if (!merge_target[item_name].IsMap())
                    {
                        boblog->error("Cannot merge map and non-map\n{}\n{}", merge_target.as<std::string>(), node.as<std::string>());
                        return;
                    }
                    auto new_merge = merge_target[item_name];
                    yaml_node_merge(new_merge, item_node);
                }
            }
        }
    }

    void project::process_construction(indicators::ProgressBar& bar)
    {
        auto boblog = spdlog::get("boblog");
        typedef enum
        {
            nothing_to_do,
            dependency_not_ready,
            set_to_complete,
            execute_process,
        } blueprint_status;
        int loop_count = 0;
        bool something_updated = true;
        auto construction_start_time = fs::file_time_type::clock::now();
        int starting_size = todo_list.size( );
        int completed_tasks = 0;

        if ( todo_list.size( ) == 0 )
            return;

        std::vector<std::shared_ptr<construction_task>> running_tasks;

        // Process all the stuff to be built
        boblog->info("------ Building ------");
        boblog->info("{} items left to construct", todo_list.size());

    #if defined(CONSTRUCTION_LIST_DUMP)
        for (auto a: construction_list) std::cout << "- " << a.first << "\n";
    #endif

        // loop through list
        int loop = 0;
        int last_progress_update = 0;
        auto i = todo_list.begin();
        while ( todo_list.size() != 0 )
        {
            ++loop;

            while (running_tasks.size() >= std::thread::hardware_concurrency())
            {
                // Update the modified times of the construction items
                for (auto a = running_tasks.begin(); a != running_tasks.end();)
                {
                    if ( a->get()->thread_result.wait_for(0ms) == std::future_status::ready )
                    {
                        boblog->info( "{}: Done", a->get()->blueprint->target);
                        a->get()->last_modified = construction_start_time;
                        a->get()->state = bob_task_up_to_date;
                        a = running_tasks.erase( a );
                        ++completed_tasks;
                    }
                    else
                        ++a;
                }
            }

            int progress = (100 * completed_tasks)/starting_size;
            if (progress != last_progress_update)
            {
                bar.set_progress(progress);
                last_progress_update = progress;
            }

            // Check if we've done a pass through all the items in the construction list
            if ( i == todo_list.end( ) )
            {
                if (running_tasks.size() == 0 && something_updated == false )
                    break;

                // Update the modified times of the construction items
                for (auto a = running_tasks.begin(); a != running_tasks.end();)
                {
                    if ( a->get()->thread_result.wait_for(0ms) == std::future_status::ready )
                    {
                        boblog->info( "{}: Done", a->get()->blueprint->target);
                        a->get()->last_modified = construction_start_time;
                        a->get()->state = bob_task_up_to_date;
                        a = running_tasks.erase( a );
                        ++completed_tasks;
                    }
                    else
                        ++a;
                }

                // Reset iterator back to the start of the list and continue
                i = todo_list.begin( );
                something_updated = false;
            }

            auto task_list = construction_list.equal_range(*i);

            // Check validity of task_list
            if ( task_list.first == construction_list.cend( ) )
            {
                boblog->info("Couldn't find '{}' in construction list", *i);
                i = todo_list.erase(i);
                continue;
            }

            auto is_task_complete = [task_list]() -> bool {
                for(auto i = task_list.first; i != task_list.second; ++i)
                    if (i->second->state != bob_task_up_to_date)
                        return false;
                return true;
            };

            auto get_task_status = [this](construction_task& task) -> blueprint_status {
                blueprint_status status = task.last_modified.time_since_epoch().count() == 0 ? execute_process : set_to_complete;
                if (task.blueprint)
                    for ( const auto& d: task.blueprint->dependencies )
                    {
                        for (auto [start,end] = this->construction_list.equal_range(d); start != end; ++start)
                        {
                            if ( start->second->state != bob_task_up_to_date)
                                return dependency_not_ready;
                            if ( (start->second->last_modified > task.last_modified ) && ( task.blueprint->blueprint["process"]) )
                                status = execute_process;
                        }
                    }
                return status;
            };

            // Check if the target needs, and is ready for, processing
            if (is_task_complete())
            {
                something_updated = true;
//                std::cout << "Finished with: " << *i << std::endl;
                i = todo_list.erase(i);
                continue;
            }

            for (auto t=task_list.first; t != task_list.second; ++t)
            {
                if (t->second->state != bob_task_to_be_done)
                    continue;

                // Check if this task is a data dependency target
                if (t->first.front() == data_dependency_identifier)
                {
                    if (has_data_dependency_changed(t->first))
                        t->second->last_modified = construction_start_time;
                    t->second->state = bob_task_up_to_date;
                    continue;
                }

                switch ( get_task_status( *(t->second) ) )
                {
                    case set_to_complete:
                        t->second->state = bob_task_up_to_date;
                        if (starting_size > 1) --starting_size;
                        something_updated = true;
                        break;
                    case execute_process:
                        something_updated = true;
                        if ( t->second->blueprint->blueprint["process"])
                        {
                            boblog->info( "{}: Executing blueprint", t->second->blueprint->target);
                            t->second->thread_result = std::async(std::launch::async | std::launch::deferred, run_command, t->second, this);
                            running_tasks.push_back(t->second);
                            t->second->state = bob_task_executing;
                        }
                        else
                            t->second->state = bob_task_up_to_date;
                        break;
                    default:
                        break;
                }
            }

            i++;
        }

        bar.set_progress(100);
        // bar.mark_as_completed();

        for (auto& a: todo_list )
        {
            boblog->info( "Couldn't build: {}", a);
            for (auto entries = this->construction_list.equal_range(a); entries.first != entries.second; ++entries.first)
                for (const auto& b: entries.first->second->blueprint->dependencies)
                    boblog->info( "\t{}", b);
        }

        for (auto a: construction_list)
            if (a.second->state == bob_task_to_be_done )
                boblog->info("{}", a.first);
    }

    void project::load_config_file(const std::string config_filename)
    {
        if (!fs::exists(config_filename))
            return;

        try
        {
            auto configuration = YAML::LoadFile( config_filename );

            project_summary["configuration"] = configuration;
            project_summary["tools"] = configuration["tools"];

            if (configuration["bob_home"].IsDefined())
            {
                bob_home_directory =  configuration["bob_home"].Scalar();
                if (!fs::exists(bob_home_directory + "/repos"))
                    fs::create_directories(bob_home_directory + "/repos");
            }

            configuration_json = configuration.as<nlohmann::json>();
        }
        catch ( std::exception &e )
        {
            log->error("Couldn't read '{}'\n{}", config_filename, e.what( ));
            project_summary["configuration"];
            project_summary["tools"] = "";
        }

    }

    /**
     * @brief Save to disk the content of the @ref project_summary to bob_summary.yaml and bob_summary.json
     *
     */
    void project::save_summary()
    {
        if (!fs::exists(project_summary["project_output"].Scalar()))
            fs::create_directories(project_summary["project_output"].Scalar());

        std::ofstream summary_file( project_summary["project_output"].Scalar() + "/bob_summary.yaml" );
        summary_file << project_summary;
        summary_file.close();
        std::ofstream json_file( project_summary["project_output"].Scalar() + "/bob_summary.json" );
		json_file << project_summary_json.dump(3);
		json_file.close();
    }

    std::string try_render(inja::Environment& env, const std::string& input, const nlohmann::json& data, std::shared_ptr<spdlog::logger> log)
    {
        try
        {
            return env.render(input, data);
        }
        catch(std::exception& e)
        {
            log->error("Template error: {}\n{}", input, e.what());
            return "";
        }
    }

    /**
     * @brief Parses dependency files as output by GCC or Clang generating a vector of filenames as found in the named file
     *
     * @param filename  Name of the dependency file. Typically ending in '.d'
     * @return std::vector<std::string>  Vector of files specified as dependencies
     */
    std::vector<std::string> parse_gcc_dependency_file(const std::string filename)
    {
        std::vector<std::string> dependencies;
        std::ifstream infile(filename);

        if (!infile.is_open())
            return {};

        std::string line;

        // Find and ignore the first line with the target. Typically "<target>: \"
        do
        {
            std::getline(infile, line);
        } while(line.length() > 0 && line.find(':') == std::string::npos);

        while (std::getline(infile, line, ' '))
        {
            if (line.empty() || line.compare("\\\n") == 0)
                continue;
            if (line.back() == '\n') line.pop_back();
            if (line.back() == '\r') line.pop_back();
            dependencies.push_back(line);
        }

        return std::move(dependencies);
    }

    /**
     * @brief Returns the path corresponding to the home directory of BOB
     *        Typically this would be ~/.bob or /Users/<username>/.bob or $HOME/.bob
     * @return std::string
     */
    std::string get_bob_home()
    {
        std::string home = !std::getenv("HOME") ? std::getenv("HOME") : std::getenv("USERPROFILE");
        return home + "/.bob";
    }

} /* namespace bob */
