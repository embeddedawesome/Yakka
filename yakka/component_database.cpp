#include "component_database.hpp"
#include "spdlog/spdlog.h"
#include "ryml.hpp"
#include "ryml_std.hpp"
#include "utilities.hpp"
#include "yakka.hpp"
#include <ranges>
#include <format>
#include <fstream>

namespace yakka {

static void process_blueprint(nlohmann::json &database, std::string_view id_string, const c4::yml::ConstNodeRef &blueprint_node);
namespace fs = std::filesystem;
using error  = std::error_code;

// Constructor initializes empty database with default values
component_database::component_database() : workspace_path(""), database_is_dirty(false), has_scanned(false)
{
}

// Destructor saves if dirty
component_database::~component_database()
{
  if (database_is_dirty) {
    auto result = save();
    if (!result) {
      spdlog::error("Failed to save database: {}", result.error().message());
    }
  }
}

void component_database::insert(std::string_view id, const path &config_file)
{
  database["components"][std::string{ id }].push_back(config_file.generic_string());
  database_is_dirty = true;
}

std::expected<void, error> component_database::load(const path &workspace_path)
{
  this->workspace_path = workspace_path;
  database_filename    = this->workspace_path / "yakka-components.json";

  try {
    if (!fs::exists(database_filename)) {
      database = { { "components", nullptr }, { "features", nullptr } };
      scan_for_components(this->workspace_path);
      return save();
    }

    std::ifstream ifs(database_filename);
    database = json::parse(ifs);

    if (!database.contains("components")) {
      database = { { "components", nullptr }, { "features", nullptr } };
      scan_for_components(this->workspace_path);
      return save();
    }
    return {};
  } catch (const std::exception &e) {
    spdlog::error("Could not load component database at {}", this->workspace_path.string());
    return std::unexpected(std::make_error_code(std::errc::io_error));
  }
}

std::expected<void, error> component_database::save() const
{
  try {
    std::ofstream ofs(database_filename);
    ofs << database.dump(2);
    return {};
  } catch (const std::exception &) {
    return std::unexpected(std::make_error_code(std::errc::io_error));
  }
}

void component_database::erase() noexcept
{
  if (!database_filename.empty()) {
    std::error_code ec;
    fs::remove(database_filename, ec);
  }
}

void component_database::clear() noexcept
{
  database.clear();
  database_is_dirty = true;
}

std::expected<bool, error> component_database::add_component(std::string_view component_id, const path &path)
{
  auto abs_path = fs::absolute(path);

  if (!fs::exists(abs_path)) {
    return std::unexpected(std::make_error_code(std::errc::no_such_file_or_directory));
  }

  const auto path_string = abs_path.generic_string();
  const auto id_str      = std::string{ component_id };

  if (database["components"].contains(id_str)) {
    const json &entries = database["components"][id_str];
    if (std::ranges::any_of(entries, [&](const auto &entry) {
          return entry.template get<std::string>() == path_string;
        })) {
      return false;
    }
  }

  database["components"][id_str].push_back(path_string);
  database_is_dirty = true;
  return true;
}

void component_database::scan_for_components(std::optional<path> search_start_path)
{
  const auto scan_path = search_start_path.value_or(workspace_path);

  if (!fs::exists(scan_path)) {
    return;
  }

  const auto process_entry = [this](const fs::directory_entry &entry) {
    const auto &path = entry.path();
    const auto ext   = path.extension();
    const auto id    = path.stem().string();

    if (auto result = add_component(id, path); result && *result) {
      if (ext == yakka_component_extension || ext == yakka_component_old_extension) {
        parse_yakka_file(path, id);
      } else if (ext == slcc_component_extension) {
        spdlog::info("Found {}", path.string());
        parse_slcc_file(path);
      } else if (ext == slcp_component_extension) {
        spdlog::info("Found project '{}'", path.string());
        // parse_slcp_file(path);
      }
    }
  };

  auto entries = fs::recursive_directory_iterator(scan_path) | std::views::filter([](const auto &e) {
                   return !e.is_directory() && e.path().filename().string().front() != '.'
                          && (e.path().extension() == yakka::yakka_component_extension || e.path().extension() == yakka::yakka_component_old_extension || e.path().extension() == yakka::slcc_component_extension
                              || e.path().extension() == yakka::slcp_component_extension);
                 });

  std::ranges::for_each(entries, process_entry);
  has_scanned = true;
}

const path &component_database::get_path() const noexcept
{
  return workspace_path;
}

std::expected<path, error> component_database::get_component(std::string_view id, flag flags) const
{
  if (!database["components"].contains(std::string{ id })) {
    return std::unexpected(std::make_error_code(std::errc::no_such_file_or_directory));
  }

  const auto &node = database["components"][std::string{ id }];

  for (const auto &n: node) {
    const auto path      = this->workspace_path / std::filesystem::path{ n.get<std::string>() };
    const auto extension = path.extension();
    if (flags == flag::IGNORE_ALL_SLC && ((extension == slcc_component_extension) || (extension == slce_component_extension) || (extension == slcp_component_extension)))
      continue;
    if (flags == flag::IGNORE_YAKKA && extension == yakka_component_extension)
      continue;
    if (flags == flag::ONLY_SLCC && ((extension == slce_component_extension) || (extension == slcp_component_extension)))
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
  return {};
}

std::expected<std::string, error> component_database::get_component_id(const path &path) const
{
  for (const auto &[name, node]: database["components"].items()) {
    if (node.is_string() && fs::path{ node.get<std::string>() } == path) {
      return name;
    }
    if (node.is_array() && std::ranges::any_of(node, [&](const auto &n) {
          return fs::path{ n.template get<std::string>() } == path;
        })) {
      return name;
    }
  }
  return std::unexpected(std::make_error_code(std::errc::no_such_file_or_directory));
}

std::optional<json> component_database::get_blueprint_provider(std::string_view blueprint) const
{
  const auto blueprint_str = std::string{ blueprint };
  if (database["blueprints"].contains(blueprint_str)) {
    return database["blueprints"][blueprint_str];
  }

  return std::nullopt;
}

std::expected<void, std::error_code> component_database::parse_yakka_file(const path &path, std::string_view id)
{
  std::vector<char> contents = yakka::get_file_contents<std::vector<char>>(path.string());
  ryml::Tree tree            = ryml::parse_in_place(ryml::to_substr(contents));
  auto root                  = tree.crootref();

  if (root.has_child("blueprints")) {
    for (const auto &b: root["blueprints"].children()) {
      process_blueprint(database, id, b);
    }
  }

  return {};
}

std::optional<json> component_database::get_feature_provider(std::string_view feature) const
{
  const auto feature_str = std::string{ feature };
  if (database["features"].contains(feature_str)) {
    return database["features"][feature_str];
  }
  return std::nullopt;
}

std::expected<void, std::error_code> component_database::parse_slcc_file(const path &path)
{
  try {
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

    if (!id_node.valid()) {
      return std::unexpected(std::make_error_code(std::errc::invalid_argument));
    }

    std::string id_string = std::string(id_node.val().str, id_node.val().len);

    auto result = add_component(id_string, path);
    if (!result) {
      return std::unexpected(result.error());
    } else if (*result) {
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
          } else {
            database["features"][feature_name].push_back(id_string);
          }
        }
      }

      if (blueprint_node.valid()) {
        for (const auto &b: blueprint_node.children()) {
          process_blueprint(database, id_string, b);
        }
      }
    }

    return {};
  } catch (const std::exception &) {
    return std::unexpected(std::make_error_code(std::errc::io_error));
  }
}

static void process_blueprint(nlohmann::json &database, std::string_view id_string, const c4::yml::ConstNodeRef &blueprint_node)
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

} // namespace yakka