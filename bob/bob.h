#pragma once

#include <set>
#include <string>

namespace bob {
    typedef std::set<std::string> component_list_t;
    typedef std::set<std::string> feature_list_t;

    static std::string component_dotname_to_id(const std::string dotname)
    {
        return dotname.find_last_of(".") != std::string::npos ? dotname.substr(dotname.find_last_of(".")+1) : dotname;
    }

    std::string exec( std::string command_text, const std::string& arg_text );
}
