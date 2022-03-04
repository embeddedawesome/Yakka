#pragma once

#include "yaml-cpp/yaml.h"
#include "taskflow.hpp"
#include <future>
#include <optional>
#include <filesystem>

#ifdef EXPERIMENTAL_FILESYSTEM
namespace fs = std::experimental::filesystem;
#else
namespace fs = std::filesystem;
#endif

namespace bob {

    struct blueprint
    {
        struct dependency
        {
            enum dependency_type {
                DEFAULT_DEPENDENCY,
                DATA_DEPENDENCY,
                DEPENDENCY_FILE_DEPENDENCY
            } type;
            std::string name;
        };
        std::string target;
        std::optional<std::string> regex;
        std::vector<dependency> dependencies; // Unprocessed dependencies. Raw values as found in the YAML.
        YAML::Node process;
        std::string parent_path;

        blueprint(const std::string& target, const YAML::Node& blueprint, const std::string& parent_path);
    };

    struct blueprint_match {
        std::vector<std::string> dependencies; // Template processed dependencies
        std::shared_ptr<bob::blueprint> blueprint;
        std::vector<std::string> regex_matches; // Regex capture groups for a particular regex match
        
    };

    // struct task
    // {
    //     const std::string target;

    //     // List of dependencies after template processing
    //     // This could use the blueprint::dependency type. Not sure on benefits
    //     std::vector< std::string > dependencies;
        
    //     // Regex matches could be stored as strings or as a JSON object
    //     // Internally they are used as JSON objects so it makes sense to do all the processing upfront
    //     nlohmann::json regex_matches;
    //     // std::vector<std::string> regex_matches;

    //     std::shared_ptr<blueprint> blueprint;
    //     // Delete the copy constructor?
    //     // blueprint_node(const blueprint_node& n) = delete;
    // };

    // enum construction_task_state
    // {
    //     bob_task_to_be_done = 0,
    //     bob_task_executing,
    //     bob_task_up_to_date,
    //     bob_task_failed
    // };

    struct construction_task
    {
        std::shared_ptr<blueprint_match> match;
        fs::file_time_type last_modified;
        tf::Task task;
        // construction_task_state state;
        // std::future<std::pair<std::string, int>> thread_result;

        construction_task() : last_modified(fs::file_time_type::min()) {}
    };
} // bob namespace
