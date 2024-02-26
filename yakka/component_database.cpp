#include "component_database.hpp"
#include "yakka_component.hpp"
#include "spdlog/spdlog.h"
#include "glob/glob.h"
#include <iostream>
#include <fstream>
#include <filesystem>

namespace yakka {
component_database::component_database() : has_scanned(false), database_is_dirty(false), workspace_path("")
{
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
  database_filename    = this->workspace_path / "yakka-components.yaml";
  try {
    if (!fs::exists(database_filename)) {
      scan_for_components(this->workspace_path);
      save();
    } else {
      database = YAML::LoadFile(database_filename.string());
      if (!database["components"].IsDefined()) {
        clear();
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
#ifdef SLCC_SUPPORT
  if (slcc::slcc_database["features"])
    (*this)["provides"]["features"] = slcc::slcc_database["features"];
#endif
  database_file << static_cast<YAML::Node &>(database);
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
  while (database.begin() != database.end())
    database.remove(database.begin()->first);

  database_is_dirty = true;
}

void component_database::add_component(std::string component_id, fs::path path)
{
  if (!fs::exists(path))
    return;

  if (!database["components"][component_id] && database["components"][component_id].size() == 0) {
    database["components"][component_id].push_back(path.generic_string());
    database_is_dirty = true;
  }
}

void component_database::scan_for_components(fs::path search_start_path)
{
  std::vector<std::future<yakka::component>> parsed_slcc_files;

  if (search_start_path.empty())
    search_start_path = this->workspace_path;

  try {
    if (!fs::exists(search_start_path)) {
      spdlog::error("Cannot scan for components. Path does not exist: '{}'", search_start_path.generic_string());
      return;
    }

    auto rdi = fs::recursive_directory_iterator(search_start_path);
    for (auto p = fs::begin(rdi); p != fs::end(rdi); ++p) {
      // Skip any directories that start with '.'
      if (p->is_directory() && p->path().filename().string().front() == '.') {
        p.disable_recursion_pending();
        continue;
      }

      if (p->path().filename().extension() == yakka_component_extension || p->path().filename().extension() == yakka_component_old_extension || p->path().filename().extension() == slcp_component_extension) {
        spdlog::info("Found {}", p->path().string());
        const auto component_id = p->path().filename().replace_extension().generic_string();
        add_component(component_id, p->path());
      } else if (p->path().filename().extension() == slcc_component_extension) {
        spdlog::info("Found {}", p->path().string());
        parsed_slcc_files.push_back(std::async(
          std::launch::async,
          [](fs::path path, fs::path workspace_path) -> yakka::component {
            try {
              yakka::component temp;
              temp.parse_file(path, workspace_path);
              return temp;
            } catch (...) {
              return {};
            }
          },
          p->path(),
          workspace_path));
      }
    }

    for (auto &file: parsed_slcc_files) {
      file.wait();
      const yakka::component slcc = file.get();
      add_component(slcc.json["id"], slcc.file_path);
      if (slcc.json.contains("/provides/features"_json_pointer))
        for (const auto &p: slcc.json["provides"]["features"]) {
          database["features"][p.get<std::string>()].push_back(slcc.json["id"].get<std::string>());
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

fs::path component_database::get_component(const std::string id) const
{
  auto node = this->database["components"][id];
  if (!node)
    return {};

  if (node.IsScalar()) {
    if (fs::exists(node.Scalar())) {
      return node.Scalar();
    }
  }
  if (node.IsSequence() && node.size() == 1) {
    if (fs::exists(node[0].Scalar())) {
      return node[0].Scalar();
    }
  }

  return {};
}

fs::path component_database::get_feature_provider(const std::string feature) const
{
  return this->database["features"][feature].as<std::string>();
}
} /* namespace yakka */
