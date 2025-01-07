#pragma once

#include "yaml-cpp/yaml.h"
#include "spdlog/spdlog.h"
#include "inja.hpp"
#include "component_database.hpp"
#include <string>
#include <future>
#include <expected>
#include <optional>
#include <filesystem>

namespace yakka {
class workspace {
public:
  workspace()  = default;
  ~workspace() = default;

  std::expected<void, std::error_code> init(const fs::path &workspace_path = ".");

  std::future<fs::path> fetch_component(std::string_view name, const YAML::Node &node, std::function<void(std::string_view, size_t)> progress_handler);
  void load_component_registries();
  std::expected<void, std::error_code> add_component_registry(std::string_view url);
  std::optional<YAML::Node> find_registry_component(std::string_view name) const;
  std::optional<std::pair<fs::path, fs::path>> find_component(std::string_view component_dotname, component_database::flag flags = component_database::flag::ALL_COMPONENTS);
  std::optional<nlohmann::json> find_feature(std::string_view feature) const;
  std::optional<nlohmann::json> find_blueprint(std::string_view blueprint) const;
  std::expected<void, std::error_code> load_config_file(const fs::path &config_file_path);
  std::string template_render(const std::string input);
  std::expected<void, std::error_code> fetch_registry(std::string_view url);
  std::expected<void, std::error_code> update_component(std::string_view name);
  fs::path get_yakka_shared_home();

  std::expected<void, std::error_code> execute_git_command(std::string_view command, std::string_view git_directory_string);
  static std::expected<fs::path, std::error_code> do_fetch_component(std::string_view name,
                                                                     std::string_view url,
                                                                     std::string_view branch,
                                                                     const fs::path &git_location,
                                                                     const fs::path &checkout_location,
                                                                     std::function<void(std::string_view, size_t)> progress_handler);

public:
  std::shared_ptr<spdlog::logger> log;
  YAML::Node registries;
  YAML::Node configuration;
  nlohmann::json configuration_json;
  std::map<std::string, std::future<void>> fetching_list;
  std::filesystem::path workspace_path;
  std::filesystem::path shared_components_path;
  inja::Environment inja_environment;
  component_database local_database;
  component_database shared_database;
  std::filesystem::path yakka_shared_home;
  std::vector<std::filesystem::path> packages;
  std::vector<component_database> package_databases;
};
} // namespace yakka
