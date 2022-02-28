#pragma once

#include "bob_blueprint.hpp"
#include <string>
#include <vector>
#include <memory>
#include <map>

namespace bob
{
    class blueprint_database
    {
    public:
        void load(const std::string path);
        void save(const std::string path);

        // void generate_task_database(std::vector<std::string> command_list);
        // void process_blueprint_target( const std::string target );

        std::multimap<std::string, std::shared_ptr<blueprint> > blueprints;
    };
}
