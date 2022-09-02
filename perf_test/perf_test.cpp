#include "nlohmann/json.hpp"
#include "yaml-cpp/yaml.h"
//#include "scnlib.h"
#include <random>
#include <string>
#include <chrono>
#include <iostream>

void insert_as_map(const std::vector<std::string>& data);
void insert_as_sequence(const std::vector<std::string>& data);
std::string random_string(int string_length);

int main(int argc, const char** argv)
{
    #if 0
    // Generate random strings
    std::vector<std::string> data;
    for (int i=0; i < 30000; ++i)
        data.push_back(random_string(8));

    insert_as_sequence(data);
    insert_as_map(data);
    #endif

    nlohmann::json test = R"(
    {
    "happy": {"bob_file": "good"},
    "pi": ["1","2","3","4","5"]
    })"_json;

    #ifdef TEST1
    for (auto& i: test.items())
        if (i.value().contains("bob_file"))
            std::cout << i.value()["bob_file"].get<std::string>();
    #endif

    for (auto& i: test["pi"]) {
        std::string temp = i;//.get<std::string>();
        std::cout << temp << "\n";
    }
    
    return 0;
}


std::string random_string(int string_length)
{
     std::string str("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");

     std::random_device rd;
     std::mt19937 generator(rd());

     std::shuffle(str.begin(), str.end(), generator);

     return str.substr(0, string_length);
}


void insert_as_map(const std::vector< std::string>& data)
{
    // Insert into JSON as map
    nlohmann::json json_node;
    auto t1 = std::chrono::high_resolution_clock::now();
    for (int i=0; i < data.size(); ++i)
        json_node[data[i]] = 1;
    auto t2 = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
    std::cout << duration << "ms to insert JSON map\n";

    // Insert into YAML as map
    YAML::Node yaml_node;
    t1 = std::chrono::high_resolution_clock::now();
    for (int i=0; i < data.size(); ++i)
        yaml_node[data[i]] = 1;
    t2 = std::chrono::high_resolution_clock::now();

    duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
    std::cout << duration << "ms to insert YAML map\n";
}

void insert_as_sequence(const std::vector<std::string>& data)
{
    // Insert into JSON as sequence
    nlohmann::json json_node;
    auto t1 = std::chrono::high_resolution_clock::now();
    for (int i=0; i < data.size(); ++i)
        json_node.push_back(data[i]);
    auto t2 = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
    std::cout << duration << "ms to insert JSON sequence\n";

    // Insert into YAML as sequence
    YAML::Node yaml_node;
    t1 = std::chrono::high_resolution_clock::now();
    for (int i=0; i < data.size(); ++i)
        yaml_node.push_back(data[i]);
    t2 = std::chrono::high_resolution_clock::now();

    duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
    std::cout << duration << "ms to insert YAML sequence\n";
}