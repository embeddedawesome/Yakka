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

static void process_blueprint(nlohmann::json &database, const std::string &id_string, const c4::yml::ConstNodeRef &blueprint_node);

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

bool component_database::add_component(std::string component_id, fs::path path)
{
  path = fs::absolute(path);

  if (!fs::exists(path))
    return false;

  const auto path_string = path.generic_string();

  // Check this entry already exists
  if (database["components"].contains(component_id)) {
    for (const auto &i: database["components"][component_id])
      if (i.get<std::string>() == path_string)
        return false;
  }
  database["components"][component_id].push_back(path_string);
  database_is_dirty = true;
  return true;
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
    const auto extension = p.filename().extension();
    if (extension == yakka_component_extension || extension == yakka_component_old_extension) {
      spdlog::info("Found {}", p.string());
      const auto component_id = p.filename().replace_extension().generic_string();
      if (add_component(component_id, p)) {
        parse_yakka_file(p, component_id);
      }
    } else if (extension == slcp_component_extension) {
      spdlog::info("Found project '{}'", p.string());
      const auto component_id = p.filename().replace_extension().generic_string();
      add_component(component_id, p);
    } else if (extension == slcc_component_extension || extension == slce_component_extension) {
      spdlog::info("Found {}", p.string());
      try {
        parse_slcc_file(p);
      } catch (std::exception &e) {
        spdlog::error("Error parsing {}: {}", p.string(), e.what());
      }
    }
  };

  // Check if path has an slcs
  bool workspace_is_sdk = false;
  // fs::path slcs_file;
  auto di = fs::directory_iterator(search_start_path);
  for (auto p = fs::begin(di); p != fs::end(di); ++p) {
    if (p->path().filename().extension() == slcs_extension) {
      workspace_is_sdk = true;
      // slcs_file = p->path();
      break;
    }
  }

  try {
    auto rdi = fs::recursive_directory_iterator(search_start_path);
    for (auto p = fs::begin(rdi); p != fs::end(rdi); ++p) {
      // Skip any directories that start with '.'
      if (p->is_directory() && p->path().filename().generic_wstring().front() == '.') {
        p.disable_recursion_pending();
        continue;
      }
      // If scanning an SDK, skip 'extension'
      if (workspace_is_sdk && p->path().filename() == slsdk_extensions_directory) {
        p.disable_recursion_pending();
        continue;
      }

      process_path_item(p->path());
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
    const auto path      = std::filesystem::path{ node.get<std::string>() };
    const auto extension = path.extension();
    if (flags == flag::IGNORE_SLCC && ((extension == slcc_component_extension) || (extension == slce_component_extension)))
      return {};

    if (flags == flag::IGNORE_YAKKA && extension == yakka_component_extension)
      return {};

    if (fs::exists(path)) {
      return path;
    }
  } else if (node.is_array()) {
    for (const auto &n: node) {
      const auto path      = this->workspace_path / std::filesystem::path{ n.get<std::string>() };
      const auto extension = path.extension();
      if (flags == flag::IGNORE_SLCC && ((extension == slcc_component_extension) || (extension == slce_component_extension)))
        continue;
      if (flags == flag::IGNORE_YAKKA && extension == yakka_component_extension)
        continue;
      // If there is an SLCP and there is more than one entry, ignore the SLCP
      if (extension == slcp_component_extension && node.size() > 1)
        continue;
      if (fs::exists(path)) {
        return path;
      } else {
        spdlog::error("Couldn't find {}", path.string());
        return {};
      }
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

nlohmann::json component_database::get_blueprint_provider(const std::string blueprint) const
{
  if (this->database.contains("blueprints"))
    if (this->database["blueprints"].contains(blueprint))
      return this->database["blueprints"][blueprint];

  return {};
}

void component_database::parse_yakka_file(const std::filesystem::path path, const std::string &id)
{
  std::vector<char> contents = yakka::get_file_contents<std::vector<char>>(path.string());
  ryml::Tree tree            = ryml::parse_in_place(ryml::to_substr(contents));
  auto root                  = tree.crootref();

  if (root.has_child("blueprints")) {
    for (const auto &b: root["blueprints"].children()) {
      process_blueprint(database, id, b);
    }
  }
}

void component_database::parse_slcc_file(const std::filesystem::path path)
{
  std::vector<char> contents = yakka::get_file_contents<std::vector<char>>(path.string());
  ryml::Tree tree            = ryml::parse_in_place(ryml::to_substr(contents));
  auto root                  = tree.crootref();

  c4::yml::ConstNodeRef id_node;
  c4::yml::ConstNodeRef provides_node;
  c4::yml::ConstNodeRef blueprint_node;

  // Check if the slcc is an omap
  if (root.is_seq()) {
    for (const auto &c: root.children()) {
      if (c.has_child("id"))
        id_node = c["id"];
      if (c.has_child("provides"))
        provides_node = c["provides"];
      if (c.has_child("blueprints"))
        blueprint_node = c["blueprints"];
    }
  } else {
    if (root.has_child("id"))
      id_node = root["id"];
    if (root.has_child("provides"))
      provides_node = root["provides"];
    if (root.has_child("blueprints"))
      blueprint_node = root["blueprints"];
  }

  if (!id_node.valid())
    return;

  std::string id_string = std::string(id_node.val().str, id_node.val().len);

  if (add_component(id_string, path)) {
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

    if (blueprint_node.valid()) {
      for (const auto &b: blueprint_node.children()) {
        process_blueprint(database, id_string, b);
      }
    }
  }
}

static void process_blueprint(nlohmann::json &database, const std::string &id_string, const c4::yml::ConstNodeRef &blueprint_node)
{
  // Ignore regex blueprints
  if (blueprint_node.has_child("regex")) {
    return;
  }

  // Ignore templated blueprints
  if (blueprint_node.key().find("{") != ryml::npos) {
    return;
  }

  // Store blueprint entry
  std::string blueprint_name = std::string(blueprint_node.key().str, blueprint_node.key().len);
  spdlog::info("Found blueprint: {}", blueprint_name);
  database["blueprints"][blueprint_name].push_back(id_string);
}

} /* namespace yakka */
