#include <gtest/gtest.h>
#include "component_database.hpp"
#include <filesystem>

namespace yakka::test {

namespace fs = std::filesystem;

class ComponentDatabaseTest : public ::testing::Test {
protected:
  void SetUp() override
  {
    // Create temporary test directory
    test_path = fs::temp_directory_path() / "yakka_test";
    fs::create_directories(test_path);
  }

  void TearDown() override
  {
    // Cleanup test directory
    fs::remove_all(test_path);
  }

  fs::path test_path;
};

TEST_F(ComponentDatabaseTest, InitializationTest)
{
  component_database db;
  EXPECT_FALSE(fs::exists(db.get_path()));
}

TEST_F(ComponentDatabaseTest, LoadEmptyDatabase)
{
  component_database db;
  auto result = db.load(test_path);
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(db.get_path(), test_path);
  EXPECT_TRUE(fs::exists(test_path / "yakka-components.json"));
}

TEST_F(ComponentDatabaseTest, InsertComponent)
{
  component_database db;
  db.load(test_path).value();

  fs::path config_file = test_path / "test.yakka";
  std::ofstream(config_file).close();

  db.insert("test_component", config_file);
  auto result = db.get_component("test_component");
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), config_file);
}

TEST_F(ComponentDatabaseTest, ComponentScanning)
{
  component_database db;
  db.load(test_path).value();

  // Create test component files
  fs::create_directories(test_path / "comp1");
  fs::create_directories(test_path / "comp2");
  std::ofstream(test_path / "comp1" / "comp1.yakka").close();
  std::ofstream(test_path / "comp2" / "comp2.yakka").close();

  db.scan_for_components();

  EXPECT_TRUE(db.get_component("comp1").has_value());
  EXPECT_TRUE(db.get_component("comp2").has_value());
}

TEST_F(ComponentDatabaseTest, SaveAndReload)
{
  {
    component_database db;
    db.load(test_path).value();
    db.insert("test_component", test_path / "test.yakka");
  } // Destructor should save

  component_database db2;
  db2.load(test_path).value();
  auto result = db2.get_component("test_component");
  EXPECT_TRUE(result.has_value());
}

TEST_F(ComponentDatabaseTest, ErrorHandling)
{
  component_database db;

  // Test non-existent path
  auto result = db.get_component("non_existent");
  EXPECT_FALSE(result.has_value());

  // Test invalid file path
  auto add_result = db.add_component("test", fs::path("non_existent_path"));
  EXPECT_FALSE(add_result.has_value());
}

TEST_F(ComponentDatabaseTest, DatabaseModification)
{
  component_database db;
  db.load(test_path).value();

  // Test clear
  db.insert("test", test_path / "test.yakka");
  db.clear();
  EXPECT_FALSE(db.get_component("test").has_value());

  // Test erase
  db.insert("test2", test_path / "test2.yakka");
  db.erase();
  EXPECT_FALSE(fs::exists(test_path / "yakka-components.json"));
}

} // namespace yakka::test