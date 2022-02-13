#include "bob_blueprint.hpp"
#include <iostream>

namespace bob
{
    blueprint::blueprint(const std::string& target, const YAML::Node& blueprint, const std::string& parent_path)
    {
        this->target = target;
        this->parent_path = parent_path;
        
        if (blueprint["regex"])
            this->regex = blueprint["regex"].Scalar();

        for (auto& d: blueprint["depends"])
        {
            if (d.IsScalar())
                this->dependencies.push_back({dependency::DEFAULT_DEPENDENCY, d.Scalar()});
            else if (d.IsMap())
            {
                if (d.begin()->first.Scalar() == "data")
                {
                    if (d.begin()->second.IsSequence())
                        for (auto& i: d.begin()->second)
                            this->dependencies.push_back({dependency::DATA_DEPENDENCY, i.Scalar()});
                    else
                        this->dependencies.push_back({dependency::DATA_DEPENDENCY, d.begin()->second.Scalar()});
                }
                else if (d.begin()->first.Scalar() == "dependency_file")
                {
                    this->dependencies.push_back({dependency::DEPENDENCY_FILE_DEPENDENCY, d.begin()->second.Scalar()});
                }
            }
        }

        process = blueprint["process"];
    }
}