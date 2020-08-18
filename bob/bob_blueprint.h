#pragma once

#include "yaml-cpp/yaml.h"
#include <future>
#include <filesystem>

#ifdef EXPERIMENTAL_FILESYSTEM
namespace fs = std::experimental::filesystem;
#else
namespace fs = std::filesystem;
#endif

namespace bob {

    typedef struct
    {
        std::string target;
        std::vector< std::string > dependencies;
        nlohmann::json regex_matches;
        YAML::Node  blueprint;
        fs::file_time_type last_modified;
//        std::string parent_component_path;
    } blueprint_match;

    typedef enum
    {
        bob_task_to_be_done = 0,
        bob_task_executing,
        bob_task_complete
    } construction_task_state;

    typedef struct
    {
        std::unique_ptr<blueprint_match> blueprint;
        construction_task_state state;
        std::future<void> thread_result;
    } construction_task;
} // bob namespace
