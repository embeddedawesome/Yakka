#pragma once

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

    std::string exec( const std::string& command_text, const std::string& arg_text);

    template<typename Functor>
    void exec( const std::string_view command_text, const std::string_view& arg_text, Functor function);
}
