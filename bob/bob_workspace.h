#include "yaml-cpp/yaml.h"
#include "spdlog/spdlog.h"
#include <string>
#include <future>
#include <optional>

namespace bob
{
    class workspace
    {
    public:
        workspace();
        void init();
        std::optional<std::future<void>> fetch_component(std::string component_name);
        void load_component_registries();
        std::optional<YAML::Node> find_registry_component(const std::string& name);

    public:
        std::shared_ptr<spdlog::logger> log;
        YAML::Node registries;
        std::map<std::string, std::future<void>> fetching_list;
        std::string workspace_directory;
    };
}
