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

    typedef struct blueprint_node
    {
        const std::string target;
        std::vector< std::string > dependencies;
        nlohmann::json regex_matches;
        YAML::Node  blueprint;

        blueprint_node(std::string t) : target(t) {}

        // Delete the copy constructor
        // blueprint_node(const blueprint_node& n) = delete;
    } blueprint_node;

    typedef enum
    {
        bob_task_to_be_done = 0,
        bob_task_executing,
        bob_task_up_to_date,
        bob_task_failed
    } construction_task_state;

    typedef struct
    {
        std::shared_ptr<const blueprint_node> blueprint;
        fs::file_time_type last_modified;
        construction_task_state state;
        std::future<std::pair<std::string, int>> thread_result;
    } construction_task;
} // bob namespace
