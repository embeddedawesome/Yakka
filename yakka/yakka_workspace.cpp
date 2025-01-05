#include "yakka.hpp"
#include "yakka_workspace.hpp"
#include "component_database.hpp"
#include "utilities.hpp"
#include "spdlog/sinks/basic_file_sink.h"
#include <filesystem>
#include <fstream>
#include <regex>
#include <expected>
#include <string_view>
#include <ranges>
#include <format>

namespace fs = std::filesystem;

namespace yakka {

// Using std::expected for error handling
std::expected<void, std::error_code> workspace::init(const fs::path &workspace_path)
{
  this->workspace_path = workspace_path;

  if (auto result = load_config_file(workspace_path / "config.yaml"); !result)
    return std::unexpected(result.error());

  // Determine shared home
  if (yakka_shared_home.empty()) {
    yakka_shared_home = get_yakka_shared_home();
  }

  this->shared_components_path = yakka_shared_home;

  try {
    if (!fs::exists(shared_components_path)) {
      fs::create_directories(shared_components_path);
    }
  } catch (const fs::filesystem_error &e) {
    spdlog::error("Failed to load shared component path: {}\n", e.what());
    return std::unexpected(e.code());
  }

  // Using ranges for directory creation
  const std::array dirs = { workspace_path / ".yakka/registries", workspace_path / ".yakka/repos" };

  for (const auto &dir: dirs) {
    if (!fs::exists(dir)) {
      fs::create_directories(dir);
    }
  }

  auto result = local_database.load(this->workspace_path);
  if (!result) {
    spdlog::error("Failed to load local database: {}\n", result.error().message());
    return std::unexpected(result.error());
  }

  if (!this->shared_components_path.empty()) {
    auto result = shared_database.load(this->shared_components_path);
    if (!result) {
      spdlog::error("Failed to load shared database: {}\n", result.error().message());
      return std::unexpected(result.error());
    }
  }

  // Using ranges for package database loading
  if (!this->packages.empty()) {
    package_databases.reserve(packages.size());
    std::ranges::for_each(packages, [this](const auto &p) {
      auto result = package_databases.emplace_back().load(p);
      if (!result) {
        spdlog::error("Failed to load package database at {}: {}\n", p.string(), result.error().message());
      }
    });
  }

  configuration["host_os"]              = host_os_string;
  configuration["executable_extension"] = executable_extension;
  configuration_json                    = configuration.as<nlohmann::json>();

  return {};
}

void workspace::load_component_registries()
{
  const auto registry_path = workspace_path / ".yakka/registries";
  if (!fs::exists(registry_path))
    return;

  // Using ranges and views for directory traversal
  auto yaml_files = std::views::filter(fs::recursive_directory_iterator(registry_path), [](const auto &entry) {
    return entry.path().extension() == ".yaml";
  });

  for (const auto &entry: yaml_files) {
    try {
      const auto registry_name  = entry.path().filename().replace_extension().string();
      registries[registry_name] = YAML::LoadFile(entry.path().string());
    } catch (const std::exception &e) {
      spdlog::error("Could not parse component registry '{}': {}\n", entry.path().string(), e.what());
    }
  }
}

// Using std::expected for error handling in component registry addition
std::expected<void, std::error_code> workspace::add_component_registry(std::string_view url)
{
  return fetch_registry(url);
}

// Using std::optional for registry component lookup
std::optional<YAML::Node> workspace::find_registry_component(std::string_view name) const
{
  for (const auto &r: registries) {
    const auto &registry = r.second;
    if (const auto &components = registry["provides"]["components"]; components[std::string(name)].IsDefined()) {
      return components[std::string(name)];
    }
  }
  return std::nullopt;
}

// Modern implementation of component finding with structured bindings
std::optional<std::pair<fs::path, fs::path>> workspace::find_component(std::string_view component_dotname, component_database::flag flags)
{
  const bool try_update_the_database = false;
  const auto component_id            = yakka::component_dotname_to_id(std::string(component_dotname));

  const auto [local, shared] = std::tuple{ local_database.get_component(component_id, flags), shared_database.get_component(component_id, flags) };

  if (!local.has_value() && !shared.has_value()) {
    // Using ranges to search package databases
    auto found = std::ranges::find_if(package_databases, [&](const auto &db) {
      return db.get_component(component_id, flags).has_value();
    });

    if (found != package_databases.end()) {
      return std::pair{ found->get_component(component_id, flags).value(), found->get_path() };
    }
    return std::nullopt;
  }

  if (local.has_value())
    return std::pair{ *local, fs::path{} };
  if (shared.has_value())
    return std::pair{ *shared, fs::path{} };

  if (!local_database.has_done_scan() && try_update_the_database) {
    local_database.clear();
    local_database.scan_for_components();
    return find_component(component_dotname, flags);
  }

  return std::nullopt;
}

// Feature finding with modern C++ features
std::optional<nlohmann::json> workspace::find_feature(std::string_view feature) const
{
  // Using structured bindings and if-init statement
  if (auto node = local_database.get_feature_provider(std::string(feature)); node.has_value()) {
    return *node;
  }

  if (auto node = shared_database.get_feature_provider(std::string(feature)); node.has_value()) {
    return *node;
  }

  // Using ranges to search package databases
  auto found = std::ranges::find_if(package_databases, [&](const auto &db) {
    return db.get_feature_provider(std::string(feature)).has_value();
  });

  if (found != package_databases.end()) {
    return found->get_feature_provider(std::string(feature));
  }

  return std::nullopt;
}

// Blueprint finding with modern features
std::optional<nlohmann::json> workspace::find_blueprint(std::string_view blueprint) const
{
  if (auto node = local_database.get_blueprint_provider(std::string(blueprint)); node.has_value()) {
    return *node;
  }

  if (auto node = shared_database.get_blueprint_provider(std::string(blueprint)); node.has_value()) {
    return *node;
  }

  auto found = std::ranges::find_if(package_databases, [&](const auto &db) {
    return db.get_blueprint_provider(std::string(blueprint)).has_value();
  });

  if (found != package_databases.end()) {
    return found->get_blueprint_provider(std::string(blueprint));
  }

  return std::nullopt;
}

// Config file loading with modern error handling
std::expected<void, std::error_code> workspace::load_config_file(const fs::path &config_file_path)
{
  try {
    if (!fs::exists(config_file_path))
      return {};

    configuration = YAML::LoadFile(config_file_path.string());

    if (configuration["path"]) {
      std::string path;
      for (const auto &p: configuration["path"]) {
        path += std::format("{}{}", p.as<std::string>(), host_os_path_seperator);
      }
      path += std::getenv("PATH");

#if defined(_WIN64) || defined(_WIN32) || defined(__CYGWIN__)
      _putenv_s("PATH", path.c_str());
#else
      setenv("PATH", path.c_str(), 1);
#endif
    }

    if (configuration["packages"]) {
      for (const auto &p: configuration["packages"]) {
        auto path = p.as<std::string>();
        if (path.starts_with('~')) {
#if defined(_WIN64) || defined(_WIN32) || defined(__CYGWIN__)
          std::string homepath = std::getenv("HOMEPATH");
          std::ranges::replace(homepath, '\\', '/');
#else
          std::string homepath = std::getenv("HOME");
#endif
          path = homepath + path.substr(1);
        }
        packages.push_back(path);
      }
    }

    if (configuration["home"]) {
      yakka_shared_home = fs::path(configuration["home"].Scalar());
    }

    return {};
  } catch (const std::exception &e) {
    spdlog::error("Couldn't read '{}': {}\n", config_file_path.string(), e.what());
    return std::unexpected(std::make_error_code(std::errc::invalid_argument));
  }
}

// Modern implementation of component fetching
std::future<fs::path> workspace::fetch_component(std::string_view name, const YAML::Node &node, std::function<void(std::string_view, size_t)> progress_handler)
{
  const auto url = try_render(inja_environment, node["packages"]["default"]["url"].as<std::string>(), configuration_json);

  const auto branch = try_render(inja_environment, node["packages"]["default"]["branch"].as<std::string>(), configuration_json);

  const bool shared_components_write_access = (fs::status(shared_components_path).permissions() & fs::perms::owner_write) != fs::perms::none;

  const auto git_location = (node["type"] && node["type"].as<std::string>() == "tool" && shared_components_write_access) ? shared_components_path / "repos" : workspace_path / ".yakka/repos";

  const auto checkout_location = (node["type"] && node["type"].as<std::string>() == "tool" && shared_components_write_access) ? shared_components_path / "repos" / std::string(name) : workspace_path / "components" / std::string(name);

  return std::async(std::launch::async, [=]() -> fs::path {
    auto result = do_fetch_component(std::string(name), url, branch, git_location, checkout_location, progress_handler);
    if (result) {
      return *result;
    } else {
      spdlog::error("Failed to fetch '{}'. error: {}", std::string(name), result.error().message());
      return {};
    }
  });
}

// Modern implementation of registry fetching using std::expected
std::expected<void, std::error_code> workspace::fetch_registry(std::string_view url)
{
  constexpr auto GIT_STRING = "git";
  const auto fetch_string   = std::format("-C .yakka/registries/ clone {} --progress --single-branch", url);

  auto [output, result] = yakka::exec(GIT_STRING, fetch_string);
  spdlog::info("{}\n", output);

  if (result != 0) {
    return std::unexpected(std::make_error_code(std::errc::protocol_error));
  }
  return {};
}

// Modern implementation of component updating using std::expected
std::expected<void, std::error_code> workspace::update_component(std::string_view name)
{
  constexpr auto GIT_STRING = "git";
  std::string git_directory_string;

  // Determine git directory string based on component location
  if (local_database.get_component(std::string(name)).has_value()) {
    git_directory_string = std::format("--git-dir .yakka/repos/{0}/.git --work-tree components/{0} ", name);
  } else if (shared_database.get_component(std::string(name)).has_value()) {
    git_directory_string = std::format("-C {}/repos/{} ", shared_components_path.string(), name);
  } else {
    return std::unexpected(std::make_error_code(std::errc::no_such_file_or_directory));
  }

  // Using structured bindings for cleaner error handling
  const auto execute_git_command = [&](std::string_view command) -> std::expected<void, std::error_code> {
    auto [output, result] = yakka::exec(GIT_STRING, std::string(git_directory_string) + std::string(command));

    if (result != 0) {
      spdlog::error(output);
      return std::unexpected(std::make_error_code(std::errc::protocol_error));
    }
    spdlog::debug(output);
    return {};
  };

  // Execute git commands in sequence
  if (auto result = execute_git_command("stash"); !result) {
    return result;
  }

  if (auto result = execute_git_command("pull --progress"); !result) {
    return result;
  }

  return execute_git_command("stash pop");
}

// Modern implementation using C++23 features
std::expected<fs::path, std::error_code> workspace::do_fetch_component(std::string_view name,
                                                                       std::string_view url,
                                                                       std::string_view branch,
                                                                       const fs::path &git_location,
                                                                       const fs::path &checkout_location,
                                                                       std::function<void(std::string_view, size_t)> progress_handler)
{
  // Modern enum class for Git phases
  enum class GitPhase { Counting, Compressing, Receiving, Resolving, Updating, LfsCheckout };

  // Phase names as compile-time array
  using namespace std::literals;
  static constexpr std::array phase_names = { "Counting"sv, "Compressing"sv, "Receiving"sv, "Resolving"sv, "Updating"sv, "Fetch LFS"sv };

  try {
    // Setup logging with RAII
    auto fetch_log = spdlog::basic_logger_mt(std::format("fetchlog-{}", name), std::format("yakka-fetch-{}.log", name));
    fetch_log->flush_on(spdlog::level::info);

    // Directory setup using ranges and modern fs operations
    const std::array setup_paths = { git_location, checkout_location };

    for (const auto &path: setup_paths) {
      if (!fs::exists(path)) {
        spdlog::info("Creating {}", path.string());
        if (auto ec = std::error_code{}; !fs::create_directories(path, ec)) {
          return std::unexpected(ec);
        }
      }
    }

    // Cleanup existing repository if needed
    const auto repo_path = git_location / std::string(name);
    if (fs::exists(repo_path)) {
      spdlog::info("Removing {}", repo_path.string());
      if (auto ec = std::error_code{}; !fs::remove_all(repo_path, ec)) {
        return std::unexpected(ec);
      }
    }

    // Helper function to handle Git command execution
    auto execute_git_command = [&fetch_log](std::string_view cmd, std::string_view args, const auto &progress_callback) -> std::expected<int, std::error_code> {
      auto start_time = std::chrono::steady_clock::now();

      auto result = yakka::exec(std::string(cmd), std::string(args), [&](std::string_view data) {
        fetch_log->info(std::string(data));
        progress_callback(data);
      });

      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time).count();

      if (result < 0) {
        return std::unexpected(std::make_error_code(std::errc::protocol_error));
      }

      return duration;
    };

    // Progress tracking state
    /*static constexpr*/ auto progress_pattern = std::regex(R"(\((.*)/(.*))\))");
    struct {
      GitPhase current_phase = GitPhase::Counting;
      int last_progress      = 0;
    } progress_state;

    // Progress callback
    auto handle_progress = [&](std::string_view data) {
      // Update phase based on output
      const auto update_phase = [&](GitPhase new_phase, std::string_view marker) {
        if (progress_state.current_phase < new_phase && data.contains(marker)) {
          progress_state.current_phase = new_phase;
        }
      };

      // Check for phase transitions
      using enum GitPhase;
      update_phase(Compressing, "Comp");
      update_phase(Receiving, "Rece");
      update_phase(Resolving, "Reso");
      update_phase(Updating, "Updat");
      update_phase(LfsCheckout, "Filt");

      // Parse progress information
      std::string data_str{ data };
      std::smatch match;
      if (std::regex_search(data_str, match, progress_pattern)) {
        const int phase_progress = std::stoi(match[1]);
        const int end_value      = std::stoi(match[2]);
        const int progress       = (100 * phase_progress) / end_value;

        if (progress != progress_state.last_progress) {
          progress_handler(phase_names[std::to_underlying(progress_state.current_phase)], progress);
          progress_state.last_progress = progress;
        }
      }
    };

    // Execute clone command
    const auto clone_args = std::format(R"(-C "{}" clone {} {} -b {} --progress --single-branch --no-checkout)", git_location.string(), url, name, branch);

    auto clone_result = execute_git_command("git", clone_args, handle_progress);
    if (!clone_result) {
      return std::unexpected(clone_result.error());
    }
    spdlog::info("{}: cloned in {}ms", name, *clone_result);

    // Reset progress state for checkout
    progress_state = {};

    // Execute checkout command
    const auto checkout_args = std::format(R"(--git-dir "{}/{}.git" --work-tree "{}" checkout {} --progress --force)", git_location.string(), name, checkout_location.string(), branch);

    auto checkout_result = execute_git_command("git", checkout_args, handle_progress);
    if (!checkout_result) {
      return std::unexpected(checkout_result.error());
    }
    spdlog::info("{}: checkout in {}ms", name, *checkout_result);

    // Signal completion
    progress_handler("Complete"sv, 100);
    return checkout_location;

  } catch (const std::exception &e) {
    spdlog::error("Error in do_fetch_component: {}", e.what());
    return std::unexpected(std::make_error_code(std::errc::operation_canceled));
  }
}

/**
 * @brief Returns the path corresponding to the home directory of BOB
 *        Typically this would be ~/.yakka or /Users/<username>/.yakka or $HOME/.yakka
 * @return std::string
 */
fs::path workspace::get_yakka_shared_home()
{
  // Try read HOME environment variable
  char *sys_home = std::getenv("HOME");
  if (sys_home != nullptr)
    return fs::path(sys_home) / ".yakka";

  // Otherwise try the Windows USERPROFILE
  char *sys_user_profile = std::getenv("USERPROFILE");
  if (sys_user_profile != nullptr)
    return fs::path(std::string(sys_user_profile)) / ".yakka";

  // Otherwise we default to using the local .yakka folder
  return ".yakka";
}

#if 0
// Modern implementation of get_yakka_shared_home using C++23 features
static std::expected<fs::path, std::error_code> get_yakka_shared_home()
{
  // Define possible environment variables to check
  static constexpr std::array env_vars = {
    "HOME"sv,
    "USERPROFILE"sv,
    "XDG_DATA_HOME"sv // Added support for XDG Base Directory specification
  };

  try {
    // Check environment variables in order of preference
    for (const auto &env_var: env_vars) {
      if (auto *path = std::getenv(env_var.data())) {
        auto home_path = fs::path(path) / ".yakka";

        // Verify path is valid and accessible
        std::error_code ec;
        if (fs::create_directories(home_path, ec); !ec) {
          return home_path;
        }

        // If path exists but couldn't create directories, check if it's usable
        if (ec == std::errc::file_exists) {
          fs::file_status status = fs::status(home_path, ec);
          if (!ec && fs::is_directory(status)) {
            return home_path;
          }
        }
      }
    }

// Platform-specific fallback paths
#if defined(_WIN64) || defined(_WIN32) || defined(__CYGWIN__)
    // Windows: Try AppData folder
    if (auto *app_data = std::getenv("APPDATA")) {
      return fs::path(app_data) / "yakka";
    }
#else
    // Unix-like: Try XDG_DATA_HOME or ~/.local/share
    auto xdg_path = fs::path(std::getenv("HOME")) / ".local/share/yakka";
    std::error_code ec;
    if (fs::create_directories(xdg_path, ec); !ec || ec == std::errc::file_exists) {
      return xdg_path;
    }
#endif

    // Last resort: use current directory
    return fs::current_path() / ".yakka";

  } catch (const fs::filesystem_error &e) {
    return std::unexpected(e.code());
  } catch (const std::exception &e) {
    return std::unexpected(std::make_error_code(std::errc::invalid_argument));
  }
}

// Helper method to validate a potential yakka home directory
static bool is_valid_yakka_home(const fs::path &path) noexcept
{
  try {
    // clang-format off
    return fs::exists(path) && 
          fs::is_directory(path) && 
          (fs::status(path).permissions() & fs::perms::owner_write) != fs::perms::none;
    // clang-format on
  } catch (...) {
    return false;
  }
}

// Helper to ensure proper permissions on yakka home directory
static std::expected<void, std::error_code> ensure_yakka_home_permissions(const fs::path &path) noexcept
{
  try {
    std::error_code ec;

    // Ensure proper directory permissions
    auto permissions = fs::perms::owner_all | fs::perms::group_read | fs::perms::group_exec | fs::perms::others_read | fs::perms::others_exec;

    fs::permissions(path, permissions, fs::perm_options::replace, ec);

    if (ec) {
      return std::unexpected(ec);
    }

    return {};
  } catch (const fs::filesystem_error &e) {
    return std::unexpected(e.code());
  }
}

// Helper to create standard yakka directory structure
static std::expected<void, std::error_code> create_yakka_directory_structure(const fs::path &base_path) noexcept
{
  try {
    // Define standard subdirectories
    static constexpr std::array subdirs = { "registries"sv, "repos"sv, "components"sv, "cache"sv, "logs"sv };

    // Create each subdirectory
    for (const auto &subdir: subdirs) {
      auto full_path = base_path / subdir;
      std::error_code ec;

      if (!fs::create_directories(full_path, ec) && ec != std::errc::file_exists) {
        return std::unexpected(ec);
      }

      // Set appropriate permissions
      if (auto result = ensure_yakka_home_permissions(full_path); !result) {
        return result;
      }
    }

    return {};
  } catch (const fs::filesystem_error &e) {
    return std::unexpected(e.code());
  }
}

// Method to initialize yakka home directory with proper structure
static std::expected<fs::path, std::error_code> initialize_yakka_home()
{
  auto home_result = get_yakka_shared_home();
  if (!home_result) {
    return std::unexpected(home_result.error());
  }

  auto &yakka_home = *home_result;

  // Create and set up directory structure
  if (auto result = create_yakka_directory_structure(yakka_home); !result) {
    return std::unexpected(result.error());
  }

  // Set permissions on base directory
  if (auto result = ensure_yakka_home_permissions(yakka_home); !result) {
    return std::unexpected(result.error());
  }

  return yakka_home;
}
#endif

} // namespace yakka