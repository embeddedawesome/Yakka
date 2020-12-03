#include "subprocess.hpp"
#include <iostream>
namespace bob {

using namespace std::chrono_literals;

std::string exec( const std::string_view command_text, const std::string_view& arg_text )
{
    try
    {
        std::string full_command { command_text };
        if (!arg_text.empty())
        {
            full_command.append(" ");
            full_command.append(arg_text);
        }
        auto p = subprocess::Popen( full_command, subprocess::output{subprocess::IOTYPE::PIPE}, subprocess::error{subprocess::IOTYPE::PIPE} );
        auto result = p.communicate();
        // auto output = p.communicate().first;
        // auto error_data = p.communicate().second;
        // char* a;
        // do {
            
        //     char buffer[1024];
        //     memset(buffer,0,sizeof(buffer));
        //     a = fgets(buffer, sizeof(buffer), p.output());
        //     //auto output = p.communicate().first;
        //     if (a != NULL)
        //         std::cout << a << "\n";
            
        //     if (feof(p.output()) || ferror(p.output()))
        //         break;
            
        //     // std::cout << std::string(output.buf.begin(), output.buf.end());
        //     std::this_thread::sleep_for(50ms);
        // } while (true);
        std::cerr << result.second.buf.data();
        return std::string(result.first.buf.data());
    }
    catch( ...)
    {
        std::cerr << "subprocess failed\n";
        return "";
    }
}

}
