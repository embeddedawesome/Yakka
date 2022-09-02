#include "yaml-cpp/yaml.h"
#include "inja.hpp"
#include "nlohmann/json.hpp"
#include "cxxopts.hpp"
#include "semver.hpp"
#include <iostream>
#include <filesystem>

static const semver::version yinja_version {
    #include "yinja_version.h"
};

static void yaml_node_merge(YAML::Node merge_target, const YAML::Node& node);

int main( int argc, char **argv )
{
    std::vector<std::string> yaml_files;
    std::vector<std::string> json_files;
    std::string template_file;
    std::string yaml_data;

    cxxopts::Options options("yinja", "Version " + yinja_version.to_string() + "\nApplies an Inja template file to YAML data");
    options.add_options()
        ("y,yaml",     "YAML data file", cxxopts::value<std::vector<std::string>>(yaml_files))
        ("j,json",     "JSON data file", cxxopts::value<std::vector<std::string>>(json_files))
        ("t,template", "Template file",  cxxopts::value<std::string>(template_file))
        ("d,data", "YAML data",  cxxopts::value<std::string>(yaml_data))
        ("h,help", "Print usage");

    auto result = options.parse(argc, argv);

    if (result.count("help") || template_file.empty())
    {
        std::cout << options.help() << "\n";
        std::cout << "If no YAML files or YAML data is provided, Yinja will read from stdin\n";
        exit(0);
    }

    // Check we need to read from stdin, if so read everything from stdin
    if (yaml_files.empty() && yaml_data.empty() && json_files.empty())
      yaml_data = std::string{std::istreambuf_iterator<char>(std::cin), {}};

    try
    {
        nlohmann::json json;
        YAML::Node yaml;
        if (!yaml_data.empty())
           yaml = YAML::Load(yaml_data);

        // Load the YAML
        for (const auto& f: yaml_files)
        {
           if (yaml.IsNull())
              yaml = YAML::LoadFile(f);
           else
              yaml_node_merge(yaml, YAML::LoadFile(f));
        }
        if (!yaml.IsNull())
          json = yaml.as<nlohmann::json>();

        // Load the JSON
        for (const auto& f: json_files)
        {
          if (!std::filesystem::exists(f))
          {
            std::cerr << "File does not exist\n";
            exit(-1);
          }
          std::ifstream json_file(f);
          json_file >> json;
        }

        // Load the template
        std::stringstream buffer;
        buffer << std::ifstream( template_file ).rdbuf();
        const std::string inja_template = buffer.str( );
        inja::Environment env;
        env.add_callback("notdir", 1, [](inja::Arguments& args){
           std::filesystem::path temp{args.at(0)->get<std::string>()};
           return temp.filename();
        });
         env.add_callback("extension", 1, [](inja::Arguments& args){
           std::filesystem::path temp{args.at(0)->get<std::string>()};
           return temp.extension();
        });
        env.add_callback("notdir", 1, [](inja::Arguments& args) { return std::filesystem::path{args.at(0)->get<std::string>()}.filename();});
        env.add_callback("absolute_dir", 1, [](inja::Arguments& args) { return std::filesystem::absolute(args.at(0)->get<std::string>());});
        env.add_callback("extension", 1, [](inja::Arguments& args) { return std::filesystem::path{args.at(0)->get<std::string>()}.extension().string().substr(1);});
        env.add_callback("filesize", 1, [&](const inja::Arguments& args) { return std::filesystem::file_size(args[0]->get<std::string>());});
        env.add_callback("render", 1, [&](const inja::Arguments& args) { return env.render(args[0]->get<std::string>(), json);});
        env.add_callback("read_file", 1, [&](const inja::Arguments& args) { 
            auto file = std::ifstream(args[0]->get<std::string>()); 
            return std::string{std::istreambuf_iterator<char>{file}, {}};
        });
        env.add_callback("aggregate", 1, [&](const inja::Arguments& args) {
            nlohmann::json aggregate;
            auto path = nlohmann::json::json_pointer(args[0]->get<std::string>());
            // Loop through components, check if object path exists, if so add it to the aggregate
            for (const auto& [c_key, c_value]: json["components"].items())
            {
                // auto v = json_path(c.value(), path);
                if (!c_value.contains(path) || c_value[path].is_null())
                    continue;
                
                auto v = c_value[path];
                if (v.is_object())
                    for (const auto& [i_key, i_value] : v.items())
                    {
                        aggregate[i_key] = i_value; //env.render(i.second.as<std::string>(), project->project_summary);
                    }
                else if (v.is_array())
                    for (const auto& [i_key, i_value]: v.items())
                        aggregate.push_back(env.render(i_value.get<std::string>(), json));
                else if (!v.is_null())
                    aggregate.push_back(env.render(v.get<std::string>(), json));
            }
            return aggregate;
        });

        std::cout << env.render(inja_template, json);
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }
    return 0;
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
