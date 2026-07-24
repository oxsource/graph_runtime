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

}  // namespace graph::runtime
