#include "utilities.hpp"
#include "subprocess.hpp"
#include "spdlog/spdlog.h"

namespace bob {

std::pair<std::string, int> exec( const std::string& command_text, const std::string& arg_text)
{
    auto boblog = spdlog::get("boblog");
    boblog->info("{} {}", command_text, arg_text);
    try {
        std::string command = command_text;
        if (!arg_text.empty())
            command += " " + arg_text;
        #if defined(__USING_WINDOWS__)
        auto p = subprocess::Popen(command, subprocess::output{subprocess::PIPE}, subprocess::error{subprocess::STDOUT} );
        #else
        auto p = subprocess::Popen(command, subprocess::shell{true}, subprocess::output{subprocess::PIPE}, subprocess::error{subprocess::STDOUT} );
        #endif
        auto output = p.output();
        std::string result;
        if (output != nullptr)
        {
            while(1) {
                int c = fgetc(output);
                if (c == EOF)
                    break;
                result += (char)c;
            };
        }

        auto retcode = p.wait();
#if defined(__USING_WINDOWS__)
        retcode = p.poll();
#endif
        return {result,retcode};
    } catch (std::exception e)
    {
        boblog->error("Exception while executing: {}\n{}", command_text, e.what());
        return {"", -1};
    }
}

void exec( const std::string& command_text, const std::string& arg_text, std::function<void(std::string&)> function)
{
    auto boblog = spdlog::get("boblog");
    boblog->info("{} {}", command_text, arg_text);
    try {
        std::string command = command_text;
        if (!arg_text.empty())
            command += " " + arg_text;
        #if defined(__USING_WINDOWS__)
        auto p = subprocess::Popen(command, subprocess::output{subprocess::PIPE}, subprocess::error{subprocess::STDOUT} );
        #else
        auto p = subprocess::Popen(command, subprocess::shell{true}, subprocess::output{subprocess::PIPE}, subprocess::error{subprocess::STDOUT} );
        #endif
        auto output = p.output();
        std::array<char, 512> buffer;
        size_t count = 0;
        if (output != nullptr)
        {
            while(1) {
                buffer[count] = fgetc(output);
                if ( feof(output) )
                    break;

                if (count == buffer.size()-1 || buffer[count] == '\n' )
                {
                    std::string temp(buffer.data());
                    function(temp);
                    count = 0;
                }
                else
                    ++count;
            };
        }
        p.wait();
        p.poll();
    } catch (std::exception e)
    {
        boblog->error("Exception while executing: {}\n{}", command_text, e.what());
    }
}

bool yaml_diff(const YAML::Node& node1, const YAML::Node& node2)
{
    std::vector<std::pair<const YAML::Node&, const YAML::Node&>> compare_list;
    compare_list.push_back({node1, node2});
    for (int i = 0; i < compare_list.size(); ++i)
    {
        const YAML::Node& left  = compare_list[i].first;
        const YAML::Node& right = compare_list[i].second;

        if (left.Type() != right.Type())
            return true;

        switch(left.Type())
        {
            case YAML::NodeType::Scalar:
                if (left.Scalar() != right.Scalar())
                  return true;
                break;

            case YAML::NodeType::Sequence:
                // Verify the sequences have the same length
                if (left.size() != right.size())
                    return true;
                for (int a=0; a < left.size(); ++a)
                    compare_list.push_back({left[a], right[a]});
                break;

            case YAML::NodeType::Map:
                // Verify the maps have the same length
                if (left.size() != right.size())
                    return true;
                for (const auto& a: left)
                {
                    auto& key = a.first.Scalar();
                    auto& node = a.second;
                    if (!right[key])
                      return true;
                    compare_list.push_back({node, right[key]});
                }
                break;
            default:
                break;
        }
    }

    return false;
}

YAML::Node yaml_path(const YAML::Node& node, std::string path)
{
    YAML::Node temp = node;
    std::stringstream ss(path);
    std::string s;
    while (std::getline(ss, s, '.'))
        temp.reset(temp[s]);
    return temp;
}

std::tuple<component_list_t, feature_list_t, command_list_t> parse_arguments( const std::vector<std::string>& argument_string )
{
    component_list_t components;
    feature_list_t features;
    command_list_t commands;

//    for (auto s = argument_string.begin(); s != argument_string.end(); ++s)
    for (auto s: argument_string)
    {
        // Identify features, commands, and components
        if (s.front() == '+')
            features.insert(s.substr(1));
        else if (s.back() == '!')
            commands.insert(s.substr(0, s.size() - 1));
        else
            components.insert(s);
    }

    return {std::move(components), std::move(features), std::move(commands)};
}

std::string generate_project_name(const component_list_t& components, const feature_list_t& features)
{
    std::string project_name = "";

    // Generate the project name from the project string
    for (const auto &i : components)
        project_name += i + "-";

    if (!components.empty())
        project_name.pop_back();

    for (const auto &i : features)
        project_name += "-" + i;

    if (project_name.empty())
        project_name = "none";

    return project_name;
}
}
