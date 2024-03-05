#pragma once

#include "yakka_blueprint.hpp"
#include "yakka.hpp"
#include "semver.hpp"
#include <vector>
#include <string>
#include <iostream>
#include <filesystem>

namespace yakka {

struct component {
  yakka_status parse_file(fs::path file_path, fs::path package_path = {});
  //std::tuple<component_list_t &, feature_list_t &> apply_feature(std::string feature_name);
  //std::tuple<component_list_t &, feature_list_t &> process_requirements(const nlohmann::json &node);
  component_list_t get_required_components();
  feature_list_t get_required_features();
  void convert_to_yakka(fs::path package_path);
  // std::vector< blueprint_node > get_blueprints();

  // Variables
  std::string id;
  fs::path file_path;
  fs::path component_path;
  nlohmann::json json;
  semver::version version;

  // Optional path to package
  fs::path package;

  enum {
    YAKKA_FILE,
    SLCC_FILE,
    SLCP_FILE,
  } type;
};

} /* namespace yakka */
