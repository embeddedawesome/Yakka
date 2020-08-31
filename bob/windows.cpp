#include <windows.h>
#include <shlwapi.h>
#include <cstring>
#include <string>
#include <iostream>
#include <sys/stat.h>
#include <array>

namespace bob {

std::string exec( const std::string_view command_text, const std::string_view& arg_text )
{
    std::array<char, 128> buffer;
    std::string result;
    std::string full_command { command_text };
    std::replace(full_command.begin(), full_command.end(), '/', '\\');
    full_command.append(" ");
    full_command.append(arg_text);
    std::clog << "exec: " << full_command << "\n";
    std::shared_ptr<FILE> pipe( _popen( full_command.c_str(), "rt" ), _pclose );
    if ( !pipe )
        throw std::runtime_error( "_popen() failed!" );
    while ( !feof( pipe.get( ) ) )
    {
        if ( fgets( buffer.data( ), 128, pipe.get( ) ) != nullptr )
            result += buffer.data( );
    }
    return result;
}

};
