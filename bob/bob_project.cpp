#include "bob_project.h"
#include "ctpl.h"
#include <fstream>

namespace bob
{
    using namespace std::chrono_literals;

    #define THREAD_POOL_SIZE   std::thread::hardware_concurrency()

    project::project( ) : project_directory("."), bob_home_directory("/.bob"), thread_pool(THREAD_POOL_SIZE)
    {
        load_config_file("config.yaml");
        configuration_json["host_os"] = host_os_string;
    }

    project::~project( )
    {
    }


    project::project(  std::vector<std::string>& project_string ) : project_directory("."), bob_home_directory("/.bob"), thread_pool(THREAD_POOL_SIZE)
    {
        load_config_file("config.yaml");
        configuration_json["host_os"] = host_os_string;
        parse_project_string(project_string);
    }

    void project::set_project_directory(const std::string path)
    {
        project_directory = path;
    }


    YAML::Node project::get_project_summary()
    {
        return project_summary;
    }

    void project::parse_project_string( std::vector<std::string>& project_string )
    {
        project_name = "";
        for ( auto& s : project_string )
        {
            // Identify features, commands, and components
            if ( s.front( ) == '+' )
            {
                s.erase( 0, 1 );
                required_features.insert( s );
            }
            else if ( s.back( ) == '!' )
            {
                s.pop_back( );
                if (s == refresh_database_command)
                {
                    component_database.scan_for_components(project_directory);
                    component_database.save();
                }
                else
                    commands.insert( s );
            }
            else
            {
                required_components.insert( s );
            }
        }

        // Generate the project name from the project string
        for ( const auto& i : required_components )
            project_name += i + "-";

        if (!required_components.empty())
            project_name.pop_back( );

        for ( const auto& i : required_features )
            project_name += "+" + i;

        if (project_name.empty())
            project_name = "none";

        // Add standard information into the project summary
        project_summary["project_name"]   = project_name;
        project_summary["project_output"] = default_output_directory + project_name;
    }

    void project::evaluate_dependencies()
    {
        component_list_t unprocessed_components;
        component_list_t new_components;
        feature_list_t unprocessed_features;
        feature_list_t new_features;
        component_list_t missing_components;
        std::vector<std::pair<std::string, std::future<void>>> component_git_list;

        // Start processing all the required components and features
        unprocessed_components.swap(required_components);
        unprocessed_features.swap(required_features);
        do
        {
            for ( auto& component : unprocessed_components )
            {
                auto c = bob::component_dotname_to_id(component);
                
                // Ensure this is a new component
                if ( required_components.find( c ) != required_components.end() )
                    continue;

                try
                {
                    // Find the component, if it doesn't exist check the known registries
                    auto component_path = find_component(c);
                    if ( !component_path )
                    {
                        component_git_list.push_back( {c, fetch_component(c)});
                        continue;
                    }

                auto new_component = std::make_shared<bob::component>( component_path.value() );
                    components.push_back( new_component );
                    required_components.insert( c );

                    for (const auto& r : new_component->yaml["requires"]["components"])
                        new_components.insert(r.Scalar());

                    for (const auto& f : new_component->yaml["requires"]["features"])
                        new_features.insert(f.Scalar());

                    for ( auto& f : this->required_features )
                        new_component->apply_feature( f, new_components, new_features );
                }
                catch ( ... )
                {
                    missing_components.insert(c);
                }
            }

            if (missing_components.size() != 0)
                component_database.scan_for_components(project_directory);

            for ( auto& f : unprocessed_features )
            {
                // Ensure this is a new feature
                if ( required_features.insert( f ).second == false )
                    continue;

                for ( auto& c : components )
                    c->apply_feature( f, new_components, new_features );
            }

            // Check if we should abort because nothing new has happened
            if (new_components.empty() && new_features.empty() && component_git_list.empty())
                break;

            unprocessed_components.clear();
            unprocessed_features.clear();
            std::set_difference(new_components.begin(),     new_components.end(),     required_components.begin(), required_components.end(), std::inserter(unprocessed_components, unprocessed_components.begin()));
            std::set_difference(missing_components.begin(), missing_components.end(), required_components.begin(), required_components.end(), std::inserter(unprocessed_components, unprocessed_components.begin()));
            std::set_difference(new_features.begin(),       new_features.end(),       required_features.begin(),   required_features.end(),   std::inserter(unprocessed_features,   unprocessed_features.begin()));
            new_components.clear();
            new_features.clear();
            missing_components.clear();

            // Check if component fetching is complete
            for ( auto a = component_git_list.begin( ); !component_git_list.empty(); )
            {
                if (!a->second.valid())
                {
                    // std::cerr << "Failed to start thread for " << a->first << std::endl;
                    a = component_git_list.erase( a );
                }
                else if ( a->second.wait_for( 100ms ) == std::future_status::ready )
                {
                    unprocessed_components.insert( a->first );
                    component_database.scan_for_components(fs::path("components/" + a->first));
                    a = component_git_list.erase( a );
                }
                else
                    ++a;

                if (a == component_git_list.end( ))
                {
                    a = component_git_list.begin( );
                }
            }

        } while ( !unprocessed_components.empty( ) || !unprocessed_features.empty( ) || !component_git_list.empty());

        if ( missing_components.size( ) != 0 )
        {
            std::cerr << "Failed to find the following components:" << std::endl;
            for (const auto& c: missing_components)
                std::cerr << " - " << c << std::endl;
            return;
        }

        // Put all YAML nodes into the summary
        for (const auto& c: components)
        {
            project_summary["components"][c->id] = c->yaml;
            for (auto tool: c->yaml["tools"])
            {
                auto new_tool = project_summary["tools"][tool.first.Scalar()];
                if (tool.second.IsMap())
                {
                    new_tool["exe"]    = tool.second["exe"];
                    new_tool["prefix"] = tool.second["prefix"];
                    if ( new_tool["prefix"].Scalar() == "directory")
                    {
                        new_tool["prefix"] = c->yaml["directory"].Scalar( ) + "/";
                    }
                    std::clog << "Tool '" << new_tool["exe"].Scalar() << "' at '" << new_tool["prefix"] << "'" << std::endl;
                }
                else
                {
                    new_tool["exe"] = tool.second;
                }
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
        project_summary_json["aggregate"] = nlohmann::json::object();
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
                std::cerr << "TODO: Parse multiple matches to the same component ID: " << component_id << std::endl;
        }
        return {};
    }

    void project::parse_blueprints()
    {
        for ( auto c: project_summary["components"])
            for ( auto b : c.second["blueprints"] )
            {
                std::string blueprint_string = inja_environment.render( b.second["regex"] ? b.second["regex"].Scalar() : b.first.Scalar( ), project_summary_json );
                std::clog << "Blueprint: " << blueprint_string << std::endl;
                b.second["bob_parent_path"] = c.second["directory"];
                blueprint_list.insert( blueprint_list.end( ), { blueprint_string, b.second } );
            }
    }

    void project::process_aggregates()
    {
        // Find all aggregates
        for ( auto component : project_summary["components"] )
            for ( auto aggregate : component.second["aggregates"] )
                process_aggregate( aggregate );
    }

    void project::process_aggregate( YAML::Node& aggregate )
    {
        std::string prefix;
        std::vector<std::string> sections;
        std::string path;
        std::string item;
        bool prefix_directory = false;

        // Extract the YAML path information
        if ( aggregate.IsScalar( ) )
            path = aggregate.Scalar();
        else if ( aggregate.IsMap( ) && aggregate["name"] )
            path = aggregate["name"].Scalar( );
        else
        {
            std::cerr << "Invalid aggregate path\n";
            return;
        }

        path = inja_environment.render( path, project_summary_json );

        // Check if there is a custom prefix
        if ( aggregate.IsMap( ) && aggregate["prefix"] )
        {
            prefix = aggregate["prefix"].Scalar( );
            if ( prefix.compare( "directory" ) == 0 )
                prefix_directory = true;
        }

        std::stringstream ss { path };

        // Convert YAML path into vector of YAML node names
        while ( std::getline( ss, item, '/' ) )
            sections.push_back( std::move( item ) );

        // Create JSON path
        path = "/aggregate/" + path;
        nlohmann::json &j = project_summary_json[nlohmann::json::json_pointer( path )];

        // Iterate through each component looking for matching entries
        for ( auto component : project_summary["components"] )
        {
            YAML::Node node = component.second;
            int section_matches = 0;

            // Check if we need to add the component directory as a prefix
            if ( prefix_directory )
                prefix = component.second["directory"].Scalar( ) + "/";

            // Loop through each section of the YAML path
            for ( auto& s : sections )
            {
                // Make sure path section exists in this node
                if ( !node[s] )
                    break;

                // Set node to follow down the path
                ++section_matches;
                node.reset( node[s] );
            }

            // Verify that we matched the full YAML path, if not try the next component
            if ( section_matches != sections.size( ) )
                continue;

            // The node is pointing the data we need to aggregate
            if ( node.IsScalar( ) )
            {
                if ( !prefix.empty( ) )
                {
                    std::string temp = prefix + node.Scalar( );
                    j.push_back( temp );
                }
                else
                {
                    j.push_back( node.as<nlohmann::json>( ) );
                }
            }
            else
            {
                for ( auto a : node )
                {
                    if ( node.IsMap( ) )
                        j[a.first.Scalar()] = a.second.as<nlohmann::json>();
                    else if ( a.IsScalar( )  )
                    {
                        if ( prefix.empty( ) )
                            j.push_back( a.as<nlohmann::json>( ) );
                        else
                        {
                            std::string temp = prefix + a.Scalar( );
                            j.push_back( temp );
                        }
                    }
                    else
                        j.push_back( a.as<nlohmann::json>( ) );
                }
            }
        }
    }

    void project::evalutate_blueprint_dependencies()
    {
        std::set<std::string> new_targets;
        std::set<std::string> processed_targets;
        std::set<std::string> unprocessed_targets = commands;
        
        while (!unprocessed_targets.empty())
        {
            for (auto& t: unprocessed_targets)
            {
                auto matches = find_blueprint_match(t);
                for (auto& match: matches)
                {
                    new_targets.insert(match->dependencies.begin(), match->dependencies.end());
                    auto new_task = std::make_shared<construction_task>();//( std::move(match), bob_task_to_be_done, 0);
                    new_task->blueprint = std::move(match);
                    new_task->state = bob_task_to_be_done;
                    construction_list.insert( std::make_pair(t, new_task ));
                }
                processed_targets.insert(t);
                todo_list.push_back(t);
            }

            unprocessed_targets.clear();
            std::set_difference(new_targets.begin(), new_targets.end(), processed_targets.begin(), processed_targets.end(), std::inserter(unprocessed_targets, unprocessed_targets.begin()));
        }
    }

    std::vector<std::unique_ptr<blueprint_match>> project::find_blueprint_match( const std::string target )
    {
        std::vector<std::unique_ptr<blueprint_match>> blueprint_matches;

        for ( auto& b : blueprint_list )
        {
            std::smatch s;

            // Check if rule is a regex, otherwise do a string comparison
            if ( b.second["regex"] )
            {
                if ( !std::regex_match(target, s, std::regex { b.first } ) )
                    continue;
            }
            else
            {
                if (target != b.first )
                    continue;
            }

            auto match = std::make_unique<blueprint_match>();
            match->target = target;
            match->blueprint = b.second;

            // Process match
            if ( b.second["regex"] )
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
            // Set the regex matches
//            for (nlohmann::json::iterator i = item->blueprint->regex_matches.begin(); i != item->blueprint->regex_matches.end(); ++i)
//                project_summary_json[i.key()] = i.value();

            // Run template engine on dependencies
            for ( auto d : b.second["depends"] )
            {
                // Check for special dependency_file condition
                if ( d.IsMap( ) && ( d.begin()->first.Scalar().compare( "dependency_file" ) == 0 ) )
                {
                    const std::string generated_dependency_file = local_inja_env.render( d.begin()->second.Scalar(), project_summary_json );
                    auto dependencies = parse_gcc_dependency_file(generated_dependency_file);
                    match->dependencies.insert( std::end( match->dependencies ), std::begin( dependencies ), std::end( dependencies ) );
                    continue;
                }
                // Verify validity of dependency
                else if ( !d.IsScalar( ) )
                {
                    std::cerr << "Dependencies only support Scalar entries\n";
                    return {};
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
                    std::cerr << "Couldn't apply template: '" << depend_string << "'" << std::endl;
                    return {};
                }

                // Check if the input was a YAML array construct
                if ( depend_string.front( ) == '[' && depend_string.back( ) == ']' )
                {
                    // Load the generated dependency string as YAML and push each item individually
                    YAML::Node generated_node = YAML::Load( generated_depend );
                    for ( auto i : generated_node )
                    {
                        match->dependencies.push_back( i.Scalar( ) );
                    }
                }
                else
                {
                    match->dependencies.push_back( generated_depend );
                }
            }

            // If the file exists, get the last modified timestamp
            if ( fs::exists( target ) )
                match->last_modified = fs::last_write_time( target );

            blueprint_matches.push_back( std::move( match ) );
        }

        if (blueprint_matches.empty())
        {
            if (fs::exists( target ))
            {
                auto match = std::make_unique<blueprint_match>();
                match->target        = target;
                match->blueprint = YAML::Node();
                match->last_modified = fs::last_write_time( target );
                blueprint_matches.push_back( std::move( match ) );
                std::clog << "Found non-blueprint dependency '" << target << "'" << std::endl;
            }
            else
                std::clog << "No blueprint for '" << target << "'" << std::endl;
        }
            
        return blueprint_matches;
    }

    void project::load_common_commands()
    {
        blueprint_commands["echo"] = []( std::string target, const YAML::Node& command, std::string captured_output, const nlohmann::json& generated_json, inja::Environment& inja_env) -> std::string {
            if (!command.begin()->second.IsNull())
                captured_output = inja_env.render(command.begin()->second.as<std::string>(), generated_json);

            std::cout << captured_output << "\n";
            return captured_output;
        };


        blueprint_commands["execute"] = []( std::string target, const YAML::Node& command, std::string captured_output, const nlohmann::json& generated_json, inja::Environment& inja_env) -> std::string {
            if (command.begin()->second.IsNull())
                return "";

            std::string temp = command.begin( )->second.as<std::string>( );
            captured_output = inja_env.render( temp, generated_json );
            std::replace( captured_output.begin( ), captured_output.end( ), '/', '\\' );
            std::clog << "Executing '" << captured_output << "'\n";
            captured_output = exec( captured_output, std::string( "" ) );

            if ( captured_output.length( ) != 0 )
                std::cout << captured_output << "\n";

            return captured_output;
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
                    std::string r = std::regex_replace(line, regex_search, command["replace"].as<std::string>(), std::regex_constants::format_no_copy);
                    captured_output.append(r);
                }
            }
            else
            {
                captured_output = std::regex_replace(captured_output, regex_search, command["replace"].as<std::string>());
            }
            return captured_output;
        };


        blueprint_commands["inja"] = []( std::string target, const YAML::Node& command, std::string captured_output, const nlohmann::json& generated_json, inja::Environment& inja_env) -> std::string {
            try
            {
                if (command["file"])
                {
                    std::string filename = command["file"].as<std::string>();
                    if (!fs::exists(filename))
                        std::cerr << filename << " not found when trying to apply template engine\n";
                    else
                        captured_output = inja_env.render_file(filename, generated_json);
                }
                else
                {
                    const auto& node = (command["template"]) ? command["template"] : command["inja"];
                    captured_output = inja_env.render(node.Scalar(), generated_json);
                }
            }
            catch (std::exception &e)
            {
                std::cerr << "Failed to apply template\n";
                std::cerr << e.what() << "\n";
            }
            return captured_output;
        };

        blueprint_commands["save"] = []( std::string target, const YAML::Node& command, std::string captured_output, const nlohmann::json& generated_json, inja::Environment& inja_env) -> std::string {
            std::string save_filename;

            if (command.begin()->second.IsNull())
                save_filename = target;
            else
                save_filename = inja_env.render(command.begin()->second.as<std::string>(), generated_json);

            try
            {
                std::ofstream save_file;
                fs::path p(save_filename);
                fs::create_directories(p.parent_path());
                save_file.open(save_filename);
                save_file << captured_output;
                save_file.close();
            }
            catch (std::exception& e)
            {
                std::cerr << "Failed to save file: " << save_filename << "\n";
                captured_output = "";
            }
            return captured_output;
        };

        blueprint_commands["create_directory"] = [ ]( std::string target, const YAML::Node& command, std::string captured_output, const nlohmann::json& generated_json, inja::Environment& inja_env ) -> std::string {
            if (!command.begin()->second.IsNull())
            {
                std::string filename = "";
                try
                {
                    filename = command.begin()->second.as<std::string>( );
                    filename = inja_env.render(filename, generated_json);
                    if ( !filename.empty( ) )
                    {
                        fs::path p( filename );
                        fs::create_directories( p.parent_path( ) );
                    }
                }
                catch ( std::exception e )
                {
                    std::cerr << "Couldn't create directory for '" << filename << "'\n";
                }
            }
            return "";
        };

        blueprint_commands["verify"] = [ ]( std::string target, const YAML::Node& command, std::string captured_output, const nlohmann::json& generated_json, inja::Environment& inja_env ) -> std::string {
            std::string filename = command.begin()->second.as<std::string>( );
            filename = inja_env.render(filename, generated_json);
            if (fs::exists(filename))
                std::clog << filename << " exists\n";
            else
                std::clog << "BAD!! " << filename << " doesn't exist\n";
            return captured_output;
        };

        blueprint_commands["rm"] = [ ]( std::string target, const YAML::Node& command, std::string captured_output, const nlohmann::json& generated_json, inja::Environment& inja_env ) -> std::string {
            std::string filename = command.begin()->second.as<std::string>( );
            filename = inja_env.render(filename, generated_json);
            fs::remove(filename);
            return captured_output;
        };
    }

    static void run_command( int id, std::shared_ptr< construction_task> task, const project* project )
    {
        std::string captured_output;
        inja::Environment inja_env = inja::Environment();
        auto& blueprint = task->blueprint;

        inja_env.add_callback("$", 1, [&blueprint](const inja::Arguments& args) {
            return blueprint->regex_matches[ args[0]->get<int>() ];
        });

        inja_env.add_callback("curdir", 0, [&blueprint](const inja::Arguments& args) { return blueprint->blueprint["bob_parent_path"].Scalar();});


        if ( !blueprint->blueprint["process"].IsSequence())
        {
            std::cerr << "Error: process nodes must be sequences\n" << blueprint->blueprint["process"] << "\n";
            return;
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
                if (project->project_summary["tools"][command_name])
                {
                    YAML::Node tool = project->project_summary["tools"][command_name];
                    std::string command_text = "";

                    if ( tool["prefix"] )
                        command_text.append( tool["prefix"].Scalar() );

                    command_text.append( tool["exe"].as<std::string>( ) );

                    std::string arg_text = command->second.as<std::string>( );

                    // Apply template engine
                    arg_text = inja_env.render( arg_text, project->project_summary_json);

                    captured_output = exec(command_text, arg_text);

                    // Echo the output of the command
                    // TODO: Note this should be done by the main thread to ensure the outputs from multiple run_command instances don't overlap
                    std::cout << captured_output;
                }
                else if (project->blueprint_commands.at(command_name))
                {
                    captured_output = project->blueprint_commands.at(command_name)( blueprint->target, command_entry, captured_output, project->project_summary_json, inja_env );
                }
                else
                {
                    std::cerr << command_name << " tool doesn't exist\n";
                }

            }
            catch ( std::exception& e )
            {
                std::cerr << "Failed to run command: '" << command_name << "' as part of " << blueprint->target << "\n";
                std::cerr << command_entry << "\n";
                throw e;
            }
        }

        std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
        std::clog << duration << " milliseconds for " << blueprint->target << "\n";
    }

    static void yaml_node_merge(YAML::Node merge_target, const YAML::Node& node)
    {
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
                        std::cerr << "Cannot merge scalar and map\n";
                        std::cerr << "Scalar: " << i.first << "\n"
                                  << "map: " << merge_target[item_name] << "\n";
                        return;
                    }
                }
                else if (item_node.IsSequence())
                {
                    if (merge_target[item_name].IsMap())
                    {
                        std::cerr << "Cannot merge sequence and map\n";
                        std::cerr << merge_target << "\n"
                                  << node << "\n";
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
                        std::cerr << "Cannot merge map and non-map\n";
                        std::cerr << merge_target << "\n"
                                  << node << "\n";
                        return;
                    }
                    yaml_node_merge(merge_target[item_name], item_node);
                }
            }
        }
    }

    void project::process_construction()
    {
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

        if ( todo_list.size( ) == 0 )
            return;

        std::vector<std::shared_ptr<construction_task>> running_tasks;

        // Process all the stuff to be built
        std::clog << "------ Building ------\n";
        std::clog << todo_list.size() << " items left to construct\n";

    #if defined(CONSTRUCTION_LIST_DUMP)
        for (auto a: construction_list) std::cout << "- " << a.first << "\n";
    #endif

        // loop through list
        int loop = 0;
        auto i = todo_list.begin();
        while ( todo_list.size() != 0 )
        {
            ++loop;
            // Check if we've done a pass through all the items in the construction list
            if ( i == todo_list.end( ) )
            {
                bool thread_completed = false;

                // Update the modified times of the construction items
//                for (auto& a = executing_list.begin(); a != executing_list.end(); )
                for (auto a = running_tasks.begin(); a != running_tasks.end();)
                {
                    if ( a->get()->thread_result.wait_for( 1ms ) == std::future_status::ready )
                    {
                        // Make a copy of the target name so we can refer to it after we move it to the complete_list
                        std::string target_name = a->get()->blueprint->target;
                        std::clog << target_name << ": Done\n";
                        a->get()->blueprint->last_modified = construction_start_time;
                        a->get()->state = bob_task_complete;
//                        complete_list.emplace( target_name, std::move( a->get()->blueprint ) );
                        thread_completed = true;
                        a = running_tasks.erase( a );
                    }
                    else
                        ++a;
                }

                // Remove all the nodes that were completed and have been moved to the complete_list
    //            executing_list.erase(std::remove_if(executing_list.begin(), executing_list.end(), [](const auto& a) { return !(a.second); }), executing_list.end());

                if (running_tasks.size() == 0 && something_updated == false && thread_completed == false)
                    break;

                // Reset iterator back to the start of the list and continue
                i = todo_list.begin( );
                something_updated = false;

                if (thread_completed == true)
                    std::clog << todo_list.size() << " items left to construct\n";

                continue;
            }

            auto task_list = construction_list.equal_range(*i);

            // Check validity of task_list
            if ( task_list.first == construction_list.cend( ) )
            {
                std::clog << "Couldn't find '" << *i << "' in construction list" << std::endl;
                i = todo_list.erase(i);
                continue;
            }

            auto is_task_complete = [task_list]() -> bool {
                for(auto i = task_list.first; i != task_list.second; ++i)
                    if (i->second->state != bob_task_complete)
                        return false;
                return true;
            };

            auto get_task_status = [this](construction_task& task) -> blueprint_status {
                blueprint_status status = task.blueprint->last_modified.time_since_epoch().count() == 0 ? execute_process : set_to_complete;
                for ( const auto& d: task.blueprint->dependencies )
                {
                    for (auto [start,end] = this->construction_list.equal_range(d); start != end; ++start)
                    {
                        if ( start->second->state != bob_task_complete)
                            return dependency_not_ready;
                        if ( (start->second->blueprint->last_modified > task.blueprint->last_modified ) && ( task.blueprint->blueprint["process"]) )
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
                if (t->second->state == bob_task_to_be_done)
                    switch ( get_task_status( *(t->second) ) )
                    {
                        case set_to_complete:
                            std::clog << t->second->blueprint->target << ": Nothing to do" << std::endl;
                            t->second->state = bob_task_complete;
                            something_updated = true;
                            break;
                        case execute_process:
                            something_updated = true;
                            if ( t->second->blueprint->blueprint["process"])
                            {
                                std::clog << t->second->blueprint->target << ": Executing blueprint" << std::endl;
    //                            auto& blah = thread_pool.push(run_command, t->second, t->second.blueprint->target, this );
                                t->second->thread_result = thread_pool.push(run_command, t->second, this );
                                running_tasks.push_back(t->second);
                                t->second->state = bob_task_executing;
                            }
                            else
                                t->second->state = bob_task_complete;
                            break;
                    }
            }

            i++;
        }

        for (auto& a: todo_list )
        {
            std::cout << "Couldn't build: " << a << std::endl;
            for (auto entries = this->construction_list.equal_range(a); entries.first != entries.second; ++entries.first)
                for (auto b: entries.first->second->blueprint->dependencies)
                    std::cout << "\t" << b << std::endl;
        }

        for (auto a: construction_list)
            if (a.second->state == bob_task_to_be_done )
                std::clog << a.first << std::endl;
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
            std::cerr << "Couldn't read " << config_filename << std::endl << e.what( );
            project_summary["configuration"];
            project_summary["tools"] = "";
        }

    }

    void project::save_summary()
    {
        std::ofstream summary_file( project_summary["project_output"].Scalar() + "/bob_summary.yaml" );
        summary_file << project_summary;
        summary_file.close();
        std::ofstream json_file( project_summary["project_output"].Scalar() + "/bob_summary.json" );
		json_file << project_summary_json.dump(3);
		json_file.close();
    }

    void project::load_component_registries()
    {
        // Verify the .bob/registries path exists
    	if (!fs::exists(this->project_directory + "/.bob/registries"))
            return;
        
        for ( const auto& p : fs::recursive_directory_iterator( this->project_directory + "/.bob/registries") )
            if ( p.path().extension().generic_string() == ".yaml" )
                try
                {
                    registries[p.path().filename().replace_extension().generic_string()] = YAML::LoadFile(p.path().generic_string());
                }
                catch (...)
                {
                    std::cerr << "Could not parse component registry: '" << p.path().generic_string() << "'" << std::endl;
                }
    }

    std::optional<YAML::Node> project::find_registry_component(const std::string& name)
    {
        // Look for component in registries
        for ( auto r : registries )
            if ( r.second["provides"]["components"][name].IsDefined( ) )
                return r.second["provides"]["components"][name];
        return {};
    }

    std::future<void> project::fetch_component(const std::string& name)
    {
        try
        {
            const std::string git_path = project_summary["tools"]["git"].Scalar( );
            const auto result = find_registry_component(name);
            if (!result)
            {
                std::clog << "Could not find component '" << name << "'" << std::endl;
                return {};
            }
            auto node = result.value();
            // Get the URL of the git repo
            auto url    = node["packages"]["default"]["url"].Scalar();
            auto branch = node["packages"]["default"]["branch"].IsDefined() ? node["packages"]["default"]["branch"].Scalar() : "master";

            branch = inja_environment.render(branch, configuration_json);

            // Check if the repo already exists
            if (fs::exists(bob_home_directory + "/repos/" + name))
            {
                // Defer an update
//                std::clog << exec( project_summary["tools"]["git"].Scalar(), "-C " + bob_home_directory + "/repos/" + name + " pull" );
            }
            else
            {
                // Fetch it
                const std::string fetch_string = "-C " + bob_home_directory + "/repos/ clone " + url + " " + name + " -b " + branch + " --progress --single-branch --no-checkout";
                return thread_pool.push( [git_path, fetch_string](int) {
                    exec(git_path, fetch_string);
                });
            }

            if (!fs::exists("components/" + name))
                fs::create_directories("components/" + name);
            const std::string checkout_string     = "--git-dir " + bob_home_directory + "/repos/" + name + "/.git --work-tree components/" + name + " checkout " + branch + " --force";
            const std::string lfs_checkout_string = "--git-dir " + bob_home_directory + "/repos/" + name + "/.git --work-tree components/" + name + " lfs checkout";
            return thread_pool.push( [git_path, checkout_string, lfs_checkout_string]( int ) {
                // Check it out
//                std::cout << "Creating local instance of '" << name << "'" << std::endl;
                exec( git_path, checkout_string);
                exec( git_path, lfs_checkout_string);
            } );

            // Return the path to the new component
//            const std::string component_file = "components/" + name+ "/" + name + ".yaml";
//            if (!fs::exists(component_file))
//                std::cerr << "Component does not contain bob descriptor" << std::endl;

        }
        catch(...)
        {
            std::clog << "Unable to fetch component '" << name << "'" << std::endl;
        }

        return {};
    }

    std::vector<std::string> parse_gcc_dependency_file(const std::string filename)
    {
        std::vector<std::string> dependencies;
        std::ifstream infile(filename);

        if (!infile.is_open())
            return {};

        std::string line;

        // Drop the first line
        std::getline(infile, line);

        while (std::getline(infile, line, ' '))
        {
            if (line.empty() || line.compare("\\\n") == 0)
                continue;

            dependencies.push_back(line);
        }

        return dependencies;
    }

    std::string get_bob_home()
    {
        std::string home = !std::getenv("HOME") ? std::getenv("HOME") : std::getenv("USERPROFILE");
        return home + "/.bob";
    }

} /* namespace bob */
