#pragma once

#include "yaml-cpp/yaml.h"
// #include "indicators/progress_bar.hpp"
#include <set>
#include <string>
#include <functional>
#include <unordered_set>

namespace bob {
    using component_list_t = std::unordered_set<std::string>;
    using feature_list_t = std::unordered_set<std::string>;
    using command_list_t = std::unordered_set<std::string>;

    const std::string bob_component_extension   = ".bob";
    const char data_dependency_identifier = ':';
    const char data_wildcard_identifier = '*';

    static std::string component_dotname_to_id(const std::string dotname)
    {
        return dotname.find_last_of(".") != std::string::npos ? dotname.substr(dotname.find_last_of(".")+1) : dotname;
    }

    void fetch_component(const std::string& name, YAML::Node node, std::function<void(size_t)> progress_handler);
    std::pair<std::string, int> exec( const std::string& command_text, const std::string& arg_text);

    void exec( const std::string& command_text, const std::string& arg_text, std::function<void(std::string&)> function);

    bool yaml_diff(const YAML::Node& node1, const YAML::Node& node2);
    YAML::Node yaml_path(const YAML::Node& node, std::string path);
    std::tuple<component_list_t, feature_list_t, command_list_t> parse_arguments( const std::vector<std::string>& argument_string );
    std::string generate_project_name(const component_list_t& components, const feature_list_t& features);
}
