#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <vector>

#include "../RuleEngine.hpp"  // Include the class we want to test
#include "../types.hpp"

namespace fs = std::filesystem;

// Test fixture for RuleEngine tests.
// This class handles setting up and tearing down a temporary test environment.
class RuleEngineTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create a unique temporary directory for each test run.
    test_dir = fs::temp_directory_path() / "organizer_test_run";
    fs::create_directories(test_dir);

    // Clear the directory in case a previous run failed to clean up.
    for (const auto& entry : fs::directory_iterator(test_dir)) {
      fs::remove_all(entry.path());
    }
  }

  void TearDown() override {
    // Clean up the temporary directory after the test.
    std::error_code ec;
    fs::remove_all(test_dir, ec);
    // Ignore errors during cleanup as they are not part of the test result.
  }

  // Helper function to create a dummy file in our test directory.
  // It can now create files in subdirectories too.
  void CreateDummyFile(const fs::path& relative_path) {
    fs::path full_path = test_dir / relative_path;
    // Ensure the parent directory exists before creating the file.
    if (full_path.has_parent_path()) {
      fs::create_directories(full_path.parent_path());
    }
    std::ofstream ofs(full_path);
    ofs << "dummy content";
    ofs.close();
  }

  fs::path test_dir;
};

// Test case 1: Verify that a simple file rule (e.g., for PDFs) works correctly.
TEST_F(RuleEngineTest, CorrectlyIdentifiesPdfFile) {
  // 1. Arrange: Set up the test conditions.
  Config config;
  config.categories[".pdf"] = "Documents";
  CreateDummyFile("mydocument.pdf");
  RuleEngine engine(config);

  // 2. Act: Call the method we are testing.
  std::vector<Action> plan = engine.generate_plan(test_dir);

  // 3. Assert: Check if the result is what we expect.
  ASSERT_EQ(plan.size(), 1);
  const auto& action = plan[0];
  EXPECT_EQ(action.from, test_dir / "mydocument.pdf");
  EXPECT_EQ(action.to, test_dir / "Documents" / "mydocument.pdf");
  EXPECT_EQ(action.reason, "Documents");
}

// ===================================================================
// NEW TEST CASE 2
// ===================================================================
// Verify the default behavior for files that do not match any rule.
// They should be categorized into the "Other" folder.
TEST_F(RuleEngineTest, HandlesUncategorizedFileAsOther) {
  // 1. Arrange
  Config config;  // An empty config with no specific rules
  CreateDummyFile("unclassified.log");
  RuleEngine engine(config);

  // 2. Act
  std::vector<Action> plan = engine.generate_plan(test_dir);

  // 3. Assert
  ASSERT_EQ(plan.size(), 1);  // Expect one action for the default rule.
  const auto& action = plan[0];
  EXPECT_EQ(action.from, test_dir / "unclassified.log");
  EXPECT_EQ(action.to.parent_path().filename(),
            "Other");  // Should go to "Other" dir
  EXPECT_EQ(action.reason, "Other");
}

// ===================================================================
// NEW TEST CASE 3
// ===================================================================
// Verify that a directory-based rule works correctly.
// A folder containing a 'package.json' file should be categorized as
// "Projects".
TEST_F(RuleEngineTest, CorrectlyIdentifiesProjectFolderByContent) {
  // 1. Arrange
  Config config;
  Rule projectRule;
  projectRule.category = "Projects";
  projectRule.priority = 10;
  // This condition checks for the existence of a specific file in a directory.
  projectRule.conditions.push_back({"contains_filename", {"package.json"}});
  config.rules.push_back(projectRule);

  // Create a subdirectory with a package.json file inside it.
  fs::path project_folder = test_dir / "my-web-app";
  CreateDummyFile(project_folder.filename() / "package.json");

  RuleEngine engine(config);

  // 2. Act
  std::vector<Action> plan = engine.generate_plan(test_dir);

  // 3. Assert
  ASSERT_EQ(plan.size(), 1);
  const auto& action = plan[0];

  // The action should be to move the entire directory.
  EXPECT_EQ(action.from, project_folder);
  EXPECT_EQ(action.to, test_dir / "Projects" / "my-web-app");
  EXPECT_EQ(action.reason, "Projects");
}