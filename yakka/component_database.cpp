#include "component_database.hpp"
#include "yakka_component.hpp"
#include "spdlog/spdlog.h"
#include "glob/glob.h"
#include <iostream>
#include <fstream>
#include <filesystem>

namespace yakka {
component_database::component_database() : workspace_path("")
{
  has_scanned       = false;
  database_is_dirty = false;
}

component_database::~component_database()
{
  if (database_is_dirty)
    save();
}

void component_database::insert(const std::string id, fs::path config_file)
{
  database["components"][id].push_back(config_file.generic_string());
  database_is_dirty = true;
}

void component_database::load(const fs::path workspace_path)
{
  this->workspace_path = workspace_path;
  database_filename    = this->workspace_path / "yakka-components.json";
  try {
    if (!fs::exists(database_filename)) {
      database = { { "components", nullptr }, { "features", nullptr } };
      scan_for_components(this->workspace_path);
      save();
    } else {
      std::ifstream ifs(database_filename.string());
      database = nlohmann::json::parse(ifs);
      if (!database.contains("components")) {
        database.clear();
        database = { { "components", nullptr }, { "features", nullptr } };
        scan_for_components(this->workspace_path);
        save();
      }
    }
  } catch (...) {
    spdlog::error("Could not load component database at {}", this->workspace_path.string());
  }
}

void component_database::save()
{
  std::ofstream database_file(database_filename);
  database_file << database.dump(2);
  database_file.close();
  database_is_dirty = false;
}

void component_database::erase()
{
  if (!database_filename.empty())
    fs::remove(database_filename);
}

void component_database::clear()
{
  database.clear();

  database_is_dirty = true;
}

void component_database::add_component(std::string component_id, fs::path path)
{
  if (!fs::exists(path))
    return;

  // Check if it doesn't exist. In theory we could support multiple locations which is why it is stored in an arrary
  if (!database["components"].contains(component_id)) {
    database["components"][component_id].push_back(path.generic_string());
    database_is_dirty = true;
  }
}

void component_database::scan_for_components(fs::path search_start_path)
{
  std::vector<std::future<yakka::component>> parsed_slcc_files;

  if (search_start_path.empty())
    search_start_path = this->workspace_path;

  // Ensure search path exists
  if (!fs::exists(search_start_path)) {
    spdlog::error("Cannot scan for components. Path does not exist: '{}'", search_start_path.generic_string());
    return;
  }

  // Define lambda that will process a path item
  auto process_path_item = [&](const fs::path &p) {
    if (p.filename().extension() == yakka_component_extension || p.filename().extension() == yakka_component_old_extension || p.filename().extension() == slcp_component_extension) {
      spdlog::info("Found {}", p.string());
      const auto component_id = p.filename().replace_extension().generic_string();
      add_component(component_id, p);
    } else if (p.filename().extension() == slcc_component_extension) {
      spdlog::info("Found {}", p.string());
      parsed_slcc_files.push_back(std::async(
        std::launch::async,
        [](fs::path path, fs::path workspace_path) -> yakka::component {
          try {
            yakka::component temp;
            temp.parse_file(path, workspace_path);
            return temp;
          } catch (std::exception &e) {
            spdlog::error("Failed to parse {}: {}", path.string(), e.what());
            return {};
          }
        },
        p,
        workspace_path));
    }
  };

  // Check if path has an slcs
  fs::path slcs_file;
  auto di = fs::directory_iterator(search_start_path);
  for (auto p = fs::begin(di); p != fs::end(di); ++p) {
    if (p->path().filename().extension() == slcs_extension) {
      slcs_file = p->path();
      break;
    }
  }

  try {
    // Check if slcs file was not found
    if (slcs_file.empty()) {
      auto rdi = fs::recursive_directory_iterator(search_start_path);
      for (auto p = fs::begin(rdi); p != fs::end(rdi); ++p) {
        // Skip any directories that start with '.'
        if (p->is_directory() && p->path().filename().string().front() == '.') {
          p.disable_recursion_pending();
          continue;
        }

        process_path_item(p->path());
      }
    } else {
      // Load .slcs file
      auto slcs = YAML::LoadFile(slcs_file.string());

      // Iterate through directories
      for (const auto &c: slcs["component_path"]) {
        auto component_path = c["path"].as<std::string>();
        auto search_path    = search_start_path / component_path;
        if (!fs::exists(search_path))
          continue;

        auto di = fs::directory_iterator(search_path);
        for (auto p = fs::begin(di); p != fs::end(di); ++p)
          process_path_item(p->path());
      }
    }
  } catch (std::exception &e) {
    spdlog::error("Error scanning for components: {}", e.what());
    return;
  }

  // Process slcc files
  for (auto &file: parsed_slcc_files) {
    file.wait();
    const yakka::component slcc = file.get();
    add_component(slcc.json["id"], slcc.file_path);
    if (slcc.json.contains("/provides/features"_json_pointer))
      for (const auto &p: slcc.json["provides"]["features"]) {
        database["features"][p.get<std::string>()].push_back(slcc.json["id"].get<std::string>());
      }
    else
      spdlog::info("{} provides no features", slcc.json["id"].get<std::string>());
  }

  this->has_scanned = true;
}

fs::path component_database::get_path() const
{
  return this->workspace_path;
}

fs::path component_database::get_component(const std::string id) const
{
  if (!this->database["components"].contains(id))
    return {};

  auto &node = this->database["components"][id];

  if (node.is_string()) {
    if (fs::exists(node.get<std::string>())) {
      return node.get<std::string>();
    }
  }
  if (node.is_array() && node.size() == 1) {
    if (fs::exists(node[0].get<std::string>())) {
      return node[0].get<std::string>();
    }
  }

  return {};
}

nlohmann::json component_database::get_feature_provider(const std::string feature) const
{
  if (this->database["features"].contains(feature))
    return this->database["features"][feature];

  return {};
}

} /* namespace yakka */
