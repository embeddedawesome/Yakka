#pragma once

#include "json.hpp"
#include <filesystem>

namespace fs = std::filesystem;

namespace yakka {
class component_database {
public:
  component_database();
  virtual ~component_database();

  void insert(const std::string id, fs::path config_file);
  void load(const fs::path workspace_path);
  void save();
  void erase();
  void clear();
  void add_component(std::string component_id, fs::path path);
  void scan_for_components(fs::path search_start_path = "");
  fs::path get_path() const;
  fs::path get_component(const std::string id) const;
  nlohmann::json get_feature_provider(const std::string feature) const;

  void process_slc_sdk(fs::path slcs_path);

  bool has_scanned;

private:
  // YAML::Node database;
  nlohmann::json database;
  // YAML::Node feature_database;
  fs::path workspace_path;
  fs::path database_filename;
  bool database_is_dirty;
};

} /* namespace yakka */
