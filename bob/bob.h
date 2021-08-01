#pragma once

#include "yaml-cpp/yaml.h"
#include "indicators/progress_bar.hpp"
#include <set>
#include <string>
#include <functional>

namespace bob {
    typedef std::set<std::string> component_list_t;
    typedef std::set<std::string> feature_list_t;

    const std::string bob_component_extension   = ".bob";

    static std::string component_dotname_to_id(const std::string dotname)
    {
        return dotname.find_last_of(".") != std::string::npos ? dotname.substr(dotname.find_last_of(".")+1) : dotname;
    }

    template<typename Functor>
    void fetch_component(const std::string& name, YAML::Node node, Functor set_progress);
    std::pair<std::string, int> exec( const std::string& command_text, const std::string& arg_text);

    template<typename Functor>
    void exec( const std::string& command_text, const std::string& arg_text, Functor function);
}
