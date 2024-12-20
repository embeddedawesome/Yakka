#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "yakka_workspace.hpp"
#include "component_database.hpp"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

// Mock classes
class MockComponentDatabase {
public:
  MOCK_METHOD(void, load, (const fs::path &));
  MOCK_METHOD(std::string, get_component, (const std::string &, yakka::component_database::flag));
  MOCK_METHOD(nlohmann::json, get_feature_provider, (const std::string &));
  MOCK_METHOD(nlohmann::json, get_blueprint_provider, (const std::string &));
  MOCK_METHOD(fs::path, get_path, ());
  MOCK_METHOD(void, clear, ());
  MOCK_METHOD(void, scan_for_components, ());
};

class WorkspaceTest : public ::testing::Test {
protected:
  void SetUp() override
  {
    // Create temporary test directory structure
    test_dir = fs::temp_directory_path() / "yakka_test";
    fs::create_directories(test_dir / ".yakka/registries");
    fs::create_directories(test_dir / ".yakka/repos");
    fs::create_directories(test_dir / "components");

    // Create a test config file
    std::ofstream config_file(test_dir / "config.yaml");
    config_file << R"(
            #path:
            #  - /usr/local/bin
            #  - /usr/bin
            #packages:
            #  - ~/yakka/packages
            #  - /usr/local/share/yakka
        )";
    config_file.close();

    workspace = std::make_unique<yakka::workspace>();
  }

  void TearDown() override
  {
    fs::remove_all(test_dir);
  }

  fs::path test_dir;
  std::unique_ptr<yakka::workspace> workspace;
};

// Test initialization
TEST_F(WorkspaceTest, InitializationCreatesRequiredDirectories)
{
  auto result = workspace->init(test_dir);
  EXPECT_TRUE(result) << "Workspace initialization failed";

  EXPECT_TRUE(fs::exists(test_dir / ".yakka/registries"));
  EXPECT_TRUE(fs::exists(test_dir / ".yakka/repos"));
  EXPECT_TRUE(fs::exists(test_dir / "components"));
}

// Test config loading
TEST_F(WorkspaceTest, LoadsConfigurationCorrectly)
{
  auto result = workspace->init(test_dir);
  EXPECT_TRUE(result);

  // Add specific configuration checks based on your config structure
  // Note: You might need to add getter methods to workspace class for testing
}

// Test component registry operations
TEST_F(WorkspaceTest, AddsComponentRegistrySuccessfully)
{
  workspace->init(test_dir);

  // Create a mock registry
  const std::string registry_url = "git@github.com:embeddedawesome/registry.git";
  auto result                    = workspace->add_component_registry(registry_url);

  EXPECT_TRUE(result) << "Failed to add component registry";
}

// Test component finding
TEST_F(WorkspaceTest, FindsComponentInLocalDatabase)
{
  workspace->init(test_dir);

  const std::string component_name = "cpp-semver";
  auto result                      = workspace->find_component(component_name, yakka::component_database::flag::ALL_COMPONENTS);

  EXPECT_TRUE(result.has_value());
  // Add more specific checks based on expected component data
}

// Test feature finding
TEST_F(WorkspaceTest, FindsFeatureAcrossDatabases)
{
  workspace->init(test_dir);

  const std::string feature_name = "test.feature";
  auto result                    = workspace->find_feature(feature_name);

  EXPECT_TRUE(result.has_value());
  // Add more specific checks based on expected feature data
}

// Test component fetching
TEST_F(WorkspaceTest, FetchesComponentSuccessfully)
{
  workspace->init(test_dir);

  YAML::Node component_node;
  component_node["packages"]["default"]["url"]    = "git@github.com:embeddedawesome/cpp-semver.git";
  component_node["packages"]["default"]["branch"] = "main";

  bool progress_called = false;
  auto future          = workspace->fetch_component("cpp-semver", component_node, [&progress_called](std::string_view phase, size_t progress) {
    progress_called = true;
  });

  auto result = future.get();
  EXPECT_FALSE(result.empty());
  EXPECT_TRUE(progress_called);
}

// Test component updating
TEST_F(WorkspaceTest, UpdatesComponentSuccessfully)
{
  workspace->init(test_dir);

  const std::string component_name = "cpp-semver";
  auto result                      = workspace->update_component(component_name);

  EXPECT_TRUE(result) << "Component update failed";
}

// Test error cases
TEST_F(WorkspaceTest, HandlesInvalidConfigFile)
{
  // Create invalid config file
  std::ofstream config_file(test_dir / "config.yaml");
  config_file << "invalid: yaml: content: ][";
  config_file.close();

  auto result = workspace->init(test_dir);
  EXPECT_FALSE(result);
}

TEST_F(WorkspaceTest, HandlesNonExistentComponent)
{
  workspace->init(test_dir);

  const std::string non_existent = "nonexistent.component";
  auto result                    = workspace->find_component(non_existent, yakka::component_database::flag::ALL_COMPONENTS);

  EXPECT_FALSE(result.has_value());
}

// Test concurrent operations
TEST_F(WorkspaceTest, HandlesConcurrentComponentFetching)
{
  workspace->init(test_dir);

  YAML::Node component_node;
  component_node["packages"]["default"]["url"]    = "https://example.com/component.git";
  component_node["packages"]["default"]["branch"] = "main";

  std::vector<std::future<fs::path>> futures;
  for (int i = 0; i < 5; ++i) {
    futures.push_back(workspace->fetch_component(std::format("test-component-{}", i), component_node, [](std::string_view phase, size_t progress) {
    }));
  }

  for (auto &future: futures) {
    auto result = future.get();
    EXPECT_FALSE(result.empty());
  }
}

// Test with mock filesystem
class WorkspaceTestWithMockFS : public ::testing::Test {
protected:
  void SetUp() override
  {
    workspace = std::make_unique<yakka::workspace>();
  }

  std::unique_ptr<yakka::workspace> workspace;
};

TEST_F(WorkspaceTestWithMockFS, HandlesFilesystemErrors)
{
  // Test with a path that we don't have permissions for
  fs::path invalid_path = "/root/invalid_path";
  auto result           = workspace->init(invalid_path);

  EXPECT_FALSE(result);
  EXPECT_EQ(result.error().value(), static_cast<int>(std::errc::permission_denied));
}

// Integration test
TEST_F(WorkspaceTest, PerformsFullWorkflowSuccessfully)
{
  // 1. Initialize workspace
  ASSERT_TRUE(workspace->init(test_dir));

  // 2. Add component registry
  ASSERT_TRUE(workspace->add_component_registry("https://github.com/embeddedawesome/registry.git"));

  // 3. Find component
  const std::string component_name = "cpp-semver";
  auto node                        = workspace->find_registry_component(component_name);
  // auto component                   = workspace->find_component(component_name, yakka::component_database::flag::ALL_COMPONENTS);
  ASSERT_TRUE(node.has_value());

  // 4. Fetch component
  auto fetch_future = workspace->fetch_component(component_name, *node, [](std::string_view phase, size_t progress) {
  });

  auto fetch_result = fetch_future.get();
  ASSERT_FALSE(fetch_result.empty());

  // 5. Update component
  auto update_result = workspace->update_component(component_name);
  ASSERT_TRUE(update_result);
}

// Performance tests
TEST_F(WorkspaceTest, PerformanceTest)
{
  workspace->init(test_dir);

  const auto start_time = std::chrono::high_resolution_clock::now();

  // Perform a series of operations
  for (int i = 0; i < 100; ++i) {
    workspace->find_component(std::format("component.{}", i), yakka::component_database::flag::ALL_COMPONENTS);
  }

  const auto end_time = std::chrono::high_resolution_clock::now();
  const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

  EXPECT_LT(duration, 1000) << "Performance test took too long";
}