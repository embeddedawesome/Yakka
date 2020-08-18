#include <windows.h>
#include <shlwapi.h>
#include <cstring>
#include <string>
#include <iostream>
#include <sys/stat.h>
#include <array>

namespace bob {

std::string exec( std::string command_text, const std::string& arg_text )
{
    std::array<char, 128> buffer;
    std::string result;
    std::replace(command_text.begin(), command_text.end(), '/', '\\');
    command_text.append(" " + arg_text);
    std::clog << "exec: " << command_text << "\n";
    std::shared_ptr<FILE> pipe( _popen( command_text.c_str(), "rt" ), _pclose );
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
