#include "component_database.hpp"
#include "yakka_component.hpp"
#include "spdlog/spdlog.h"
#include "glob/glob.h"
#include "ryml.hpp"
#include <ryml_std.hpp>
//#include <c4/format.hpp>
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

  const auto path_string = path.generic_string();

  // Check this entry already exists
  if (database["components"].contains(component_id)) {
    for (const auto &i: database["components"][component_id])
      if (i.get<std::string>() == path_string)
        return;
  }
  database["components"][component_id].push_back(path_string);
  database_is_dirty = true;
}

void component_database::scan_for_components(fs::path search_start_path)
{
  // std::vector<std::future<ryml::Tree>> parsed_slcc_files;

  if (search_start_path.empty())
    search_start_path = this->workspace_path;

  // Ensure search path exists
  if (!fs::exists(search_start_path)) {
    spdlog::error("Cannot scan for components. Path does not exist: '{}'", search_start_path.generic_string());
    return;
  }

  // Define lambda that will process a path item
  auto process_path_item = [&](const fs::path &p) {
    if (p.filename().extension() == yakka_component_extension || p.filename().extension() == yakka_component_old_extension || p.filename().extension() == slcp_component_extension || p.filename().extension() == slce_component_extension) {
      spdlog::info("Found {}", p.string());
      const auto component_id = p.filename().replace_extension().generic_string();
      add_component(component_id, p);
    } else if (p.filename().extension() == slcc_component_extension) {
      spdlog::info("Found {}", p.string());
      parse_slcc_file(p);
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

  this->has_scanned = true;
}

fs::path component_database::get_path() const
{
  return this->workspace_path;
}

fs::path component_database::get_component(const std::string id, flag flags) const
{
  if (!this->database["components"].contains(id))
    return {};

  auto &node = this->database["components"][id];

  if (node.is_string()) {
    const auto path = std::filesystem::path{ node.get<std::string>() };

    if (flags == flag::IGNORE_SLCC && path.extension() == slcc_component_extension)
      return {};

    if (flags == flag::IGNORE_YAKKA && path.extension() == yakka_component_extension)
      return {};

    if (fs::exists(path)) {
      return path;
    }
  } else if (node.is_array()) {
    for (const auto &n: node) {
      const auto path = std::filesystem::path{ n.get<std::string>() };
      if (flags == flag::IGNORE_SLCC && path.extension() == slcc_component_extension)
        continue;
      if (flags == flag::IGNORE_YAKKA && path.extension() == yakka_component_extension)
        continue;
      if (fs::exists(path))
        return path;
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

void component_database::parse_slcc_file(std::filesystem::path path)
{
  std::vector<char> contents = yakka::get_file_contents<std::vector<char>>(path.string());
  ryml::Tree tree            = ryml::parse_in_place(ryml::to_substr(contents));
  auto root                  = tree.crootref();

  c4::yml::ConstNodeRef id_node;
  c4::yml::ConstNodeRef provides_node;

  // Check if the slcc is an omap
  if (root.is_seq()) {
    for (const auto &c: root.children()) {
      if (c.has_child("id"))
        id_node = c["id"];
      if (c.has_child("provides"))
        provides_node = c["provides"];
    }
  } else {
    if (root.has_child("id"))
      id_node = root["id"];
    if (root.has_child("provides"))
      provides_node = root["provides"];
  }

  if (!id_node.valid())
    return;

  std::string id_string = std::string(id_node.val().str, id_node.val().len);

  add_component(id_string, path);
  if (provides_node.valid()) {
    for (const auto &f: provides_node.children()) {
      if (!f.has_child("name"))
        continue;
      auto feature_node        = f["name"].val();
      std::string feature_name = std::string(feature_node.str, feature_node.len);
      if (f.has_child("condition")) {
        nlohmann::json node({ { "name", id_string }, { "condition", {} } });
        for (const auto &c: f["condition"].children()) {
          std::string condition_string = std::string(c.val().str, c.val().len);
          node["condition"].push_back(condition_string);
        }
        database["features"][feature_name].push_back(node);
      } else
        database["features"][feature_name].push_back(id_string);
    }
  }
}

} /* namespace yakka */
