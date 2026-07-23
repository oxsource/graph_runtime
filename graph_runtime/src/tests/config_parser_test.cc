#include <string>

#include "gtest/gtest.h"
#include "src/config/json/json_parser.h"
#include "src/config/config_validator.h"

namespace graph::runtime {

TEST(ConfigParserTest, ValidConfig) {
  JsonParser parser;
  auto result = parser.Parse(
      "src/config/json/testdata/string_pipeline.json");
  ASSERT_TRUE(result.ok());
  const auto& config = *result;
  ASSERT_EQ(config.nodes.size(), 3);
  ASSERT_EQ(config.streams.size(), 2);
  EXPECT_EQ(config.nodes[0].name, "producer");
  EXPECT_EQ(config.nodes[1].name, "transformer");
  EXPECT_EQ(config.nodes[2].name, "consumer");
  EXPECT_EQ(config.streams[0].name, "text_stream");
  EXPECT_EQ(config.streams[1].name, "upper_stream");
}

TEST(ConfigParserTest, MissingNodeError) {
  JsonParser parser;
  auto result = parser.Parse(
      "src/config/json/testdata/missing_node.json");
  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_TRUE(result.status().message().find("nonexistent") != std::string::npos);
}

TEST(ConfigParserTest, DuplicateStreamError) {
  JsonParser parser;
  auto result = parser.Parse(
      "src/config/json/testdata/duplicate_stream.json");
  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_TRUE(result.status().message().find("duplicate") != std::string::npos);
}

TEST(ConfigParserTest, EmptyConfig) {
  JsonParser parser;
  auto result = parser.Parse(
      "src/config/json/testdata/empty.json");
  ASSERT_TRUE(result.ok());
  EXPECT_TRUE(result->nodes.empty());
  EXPECT_TRUE(result->streams.empty());
}

TEST(ConfigParserTest, SelfLoopError) {
  JsonParser parser;
  auto result = parser.Parse(
      "src/config/json/testdata/self_loop.json");
  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_TRUE(result.status().message().find("self-loop") != std::string::npos);
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
      "src/config/json/testdata/duplicate_node.json");
  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), absl::StatusCode::kInvalidArgument);
}

TEST(ConfigParserTest, CorrectStructure) {
  JsonParser parser;
  auto result = parser.Parse(
      "src/config/json/testdata/string_pipeline.json");
  ASSERT_TRUE(result.ok());
  const auto& config = *result;
  EXPECT_EQ(config.max_queue_size, 100);
  EXPECT_FALSE(config.report_deadlock);
}

}  // namespace graph::runtime
