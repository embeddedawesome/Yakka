#pragma once

#include "json.hpp"
#include <filesystem>
#include <expected>
#include <string_view>

namespace yakka {

// Using declarations for cleaner code
using json = nlohmann::json;
using std::filesystem::path;

class component_database {
public:
  component_database();
  virtual ~component_database();

  // Delete copy operations since we're managing resources
  component_database(const component_database &)            = delete;
  component_database &operator=(const component_database &) = delete;

  // Allow moving since we can safely transfer ownership
  component_database(component_database &&) noexcept            = default;
  component_database &operator=(component_database &&) noexcept = default;

  // Modern enum class with strongly typed scope
  enum class flag { ALL_COMPONENTS, IGNORE_ALL_SLC, ONLY_SLCC, IGNORE_YAKKA };

  // Use string_view for non-owning string references
  void insert(std::string_view id, const path &config_file);

  // Return expected for operations that might fail
  [[nodiscard]] std::expected<void, std::error_code> load(const path &workspace_path);
  [[nodiscard]] std::expected<void, std::error_code> save() const;

  void erase() noexcept;
  void clear() noexcept;

  // Return expected for operations that might fail
  [[nodiscard]] std::expected<bool, std::error_code> add_component(std::string_view component_id, const path &path);

  // Use optional path for operations that might not find a result
  void scan_for_components(std::optional<path> search_start_path = std::nullopt);

  // Make getter methods const and nodiscard
  [[nodiscard]] const path &get_path() const noexcept;
  [[nodiscard]] std::expected<path, std::error_code> get_component(std::string_view id, flag flags = flag::ALL_COMPONENTS) const;

  [[nodiscard]] std::expected<std::string, std::error_code> get_component_id(const path &path) const;

  // Return optional for queries that might not find a result
  [[nodiscard]] std::optional<json> get_feature_provider(std::string_view feature) const;
  [[nodiscard]] std::optional<json> get_blueprint_provider(std::string_view blueprint) const;

  // Process external data
  std::expected<void, std::error_code> process_slc_sdk(const path &slcs_path);
  std::expected<void, std::error_code> parse_slcc_file(const path &path);
  std::expected<void, std::error_code> parse_yakka_file(const path &path, std::string_view id);

  // Make this a const getter
  [[nodiscard]] bool has_done_scan() const noexcept
  {
    return this->has_scanned;
  }

private:
  json database;
  path workspace_path;
  path database_filename;
  bool database_is_dirty{ false }; // Initialize member in-class
  bool has_scanned{ false };       // Convert from public member to private with getter
};

} // namespace yakka