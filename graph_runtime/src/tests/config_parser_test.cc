#include <string>

#include "gtest/gtest.h"
#include "src/framework/config/json/json_parser.h"
#include "src/framework/config/config_validator.h"

namespace graph::runtime {

TEST(ConfigParserTest, ValidConfig) {
  JsonParser parser;
  auto result = parser.Parse(
      "src/framework/config/json/testdata/string_pipeline.json");
  ASSERT_TRUE(result.ok());
  const auto& config = *result;
  ASSERT_EQ(config.nodes.size(), 3);
  EXPECT_EQ(config.nodes[0].name, "producer");
  EXPECT_EQ(config.nodes[1].name, "transformer");
  EXPECT_EQ(config.nodes[2].name, "consumer");
}

TEST(ConfigParserTest, EmptyConfig) {
  JsonParser parser;
  auto result = parser.Parse(
      "src/framework/config/json/testdata/empty.json");
  ASSERT_TRUE(result.ok());
  EXPECT_TRUE(result->nodes.empty());
}

TEST(ConfigParserTest, FileNotFound) {
  JsonParser parser;
  auto result = parser.Parse("nonexistent_file.json");
  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), absl::StatusCode::kNotFound);
}

TEST(ConfigParserTest, DuplicateNodeError) {
  JsonParser parser;
  auto result = parser.Parse(
      "src/framework/config/json/testdata/duplicate_node.json");
  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), absl::StatusCode::kInvalidArgument);
}

TEST(ConfigParserTest, CorrectStructure) {
  JsonParser parser;
  auto result = parser.Parse(
      "src/framework/config/json/testdata/string_pipeline.json");
  ASSERT_TRUE(result.ok());
  const auto& config = *result;
  EXPECT_EQ(config.max_queue_size, 100);
  EXPECT_FALSE(config.report_deadlock);
}

// --- US5: Config Validation Tests ---

TEST(ConfigValidatorTest, ValidConnectivityPasses) {
  GraphConfig config;
  config.nodes.push_back(
      {"a", "A", {}, {"out:x"}, {}, {}, {}, "", "", 0, 0});
  config.nodes.push_back(
      {"b", "B", {"in:x"}, {}, {}, {}, {}, "", "", 0, 0});
  auto status = ConfigValidator::Validate(config);
  ASSERT_TRUE(status.ok()) << status.message();
}

TEST(ConfigValidatorTest, MissingOutputStreamFails) {
  GraphConfig config;
  config.nodes.push_back(
      {"a", "A", {"in:missing"}, {}, {}, {}, {}, "", "", 0, 0});
  auto status = ConfigValidator::Validate(config);
  ASSERT_FALSE(status.ok());
  EXPECT_TRUE(status.message().find("missing") != std::string::npos);
}

TEST(ConfigValidatorTest, GraphInputStreamIsExempt) {
  GraphConfig config;
  config.input_streams.push_back("external");
  config.nodes.push_back(
      {"a", "A", {"in:external"}, {}, {}, {}, {}, "", "", 0, 0});
  EXPECT_TRUE(ConfigValidator::Validate(config).ok());
}

TEST(ConfigValidatorTest, NoCyclesPasses) {
  GraphConfig config;
  config.nodes.push_back(
      {"a", "A", {}, {"out:x"}, {}, {}, {}, "", "", 0, 0});
  config.nodes.push_back(
      {"b", "B", {"in:x"}, {"out:y"}, {}, {}, {}, "", "", 0, 0});
  config.nodes.push_back(
      {"c", "C", {"in:y"}, {}, {}, {}, {}, "", "", 0, 0});
  auto status = ConfigValidator::Validate(config);
  EXPECT_TRUE(status.ok()) << status;
}

TEST(ConfigValidatorTest, SimpleCycleDetected) {
  GraphConfig config;
  // A → B → A cycle.
  config.nodes.push_back(
      {"a", "A", {"in:y"}, {"out:x"}, {}, {}, {}, "", "", 0, 0});
  config.nodes.push_back(
      {"b", "B", {"in:x"}, {"out:y"}, {}, {}, {}, "", "", 0, 0});
  auto status = ConfigValidator::Validate(config);
  ASSERT_FALSE(status.ok());
  EXPECT_TRUE(status.message().find("cycle") != std::string::npos);
}

TEST(ConfigValidatorTest, SelfLoopDetected) {
  GraphConfig config;
  // A → A self-loop.
  config.nodes.push_back(
      {"a", "A", {"in:x"}, {"out:x"}, {}, {}, {}, "", "", 0, 0});
  auto status = ConfigValidator::Validate(config);
  ASSERT_FALSE(status.ok());
  EXPECT_TRUE(status.message().find("cycle") != std::string::npos);
}

TEST(ConfigValidatorTest, TransitiveCycleDetected) {
  GraphConfig config;
  // A → B → C → A cycle (3 nodes).
  config.nodes.push_back(
      {"a", "A", {"in:z"}, {"out:x"}, {}, {}, {}, "", "", 0, 0});
  config.nodes.push_back(
      {"b", "B", {"in:x"}, {"out:y"}, {}, {}, {}, "", "", 0, 0});
  config.nodes.push_back(
      {"c", "C", {"in:y"}, {"out:z"}, {}, {}, {}, "", "", 0, 0});
  auto status = ConfigValidator::Validate(config);
  ASSERT_FALSE(status.ok());
  EXPECT_TRUE(status.message().find("cycle") != std::string::npos);
}

}  // namespace graph::runtime
