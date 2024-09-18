#pragma once

#include "json.hpp"
#include <filesystem>

namespace fs = std::filesystem;

namespace yakka {
class component_database {
public:
  component_database();
  virtual ~component_database();

  enum class flag { ALL_COMPONENTS, IGNORE_SLCC, IGNORE_SLCP, IGNORE_YAKKA };

  void insert(const std::string id, fs::path config_file);
  void load(const fs::path workspace_path);
  void save();
  void erase();
  void clear();
  bool add_component(std::string component_id, fs::path path);
  void scan_for_components(fs::path search_start_path = "");
  fs::path get_path() const;
  fs::path get_component(const std::string id, flag flags = flag::ALL_COMPONENTS) const;
  nlohmann::json get_feature_provider(const std::string feature) const;
  nlohmann::json get_blueprint_provider(const std::string blueprint) const;

  void process_slc_sdk(fs::path slcs_path);
  void parse_slcc_file(const std::filesystem::path path);
  void parse_yakka_file(const std::filesystem::path path, const std::string& id);

  bool has_scanned;

private:
  nlohmann::json database;
  fs::path workspace_path;
  fs::path database_filename;
  bool database_is_dirty;
};

} /* namespace yakka */
