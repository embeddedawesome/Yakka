#include "subprocess.hpp"
#include <iostream>
namespace bob {

using namespace std::chrono_literals;

std::string exec( const std::string_view command_text, const std::string_view& arg_text )
{
    std::string full_command { command_text };
    if (!arg_text.empty())
    {
        full_command.append(" ");
        full_command.append(arg_text);
    }
    full_command.append(" 2>&1");
    std::clog << "exec: '" << full_command << "'\n";
     
    try
    {
        auto output = subprocess::check_output(full_command, subprocess::shell{true} );
//        auto output = subprocess::check_output(arg_text, subprocess::executable{command_text}, subprocess::shell{true} );
        return std::string(output.buf.begin(), output.buf.end());
    }
    catch( ...)
    {
        std::cerr << "subprocess failed\n";
        return "";
    }
}

}
