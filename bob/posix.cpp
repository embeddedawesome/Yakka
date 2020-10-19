//#include <sys/stat.h>
//#include <unistd.h>
//#include <string>
//#include <iostream>
#include "subprocess.hpp"
//#include "boost/filesystem.hpp"

namespace bob {

std::string exec( const std::string_view command_text, const std::string_view& arg_text )
{
//    std::initializer_list<const char *> args { arg_text.c_str()};
    try
    {
        auto output = subprocess::check_output(command_text, subprocess::shell{true} );
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
