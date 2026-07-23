#include <string>

#include "gtest/gtest.h"
#include "src/config/graph_config.h"
#include "src/config/config_validator.h"
#include "src/public/graph_runtime.h"
#include "src/public/graph_builder.h"

namespace graph::runtime {

TEST(BuildValidationTest, ValidConfigPasses) {
  GraphConfig config;
  config.nodes.push_back({"a", "A", {}, {}, {}, {}, {}, "", "", 1, 0});
  config.nodes.push_back({"b", "B", {}, {}, {}, {}, {}, "", "", 1, 0});
  config.streams.push_back({"s1", "a", "out", "b", "in"});
  auto status = ConfigValidator::Validate(config);
  EXPECT_TRUE(status.ok());
}

TEST(BuildValidationTest, MissingNodeRefFails) {
  GraphConfig config;
  config.nodes.push_back({"a", "A", {}, {}, {}, {}, {}, "", "", 1, 0});
  config.streams.push_back({"s1", "a", "out", "nonexistent", "in"});
  auto status = ConfigValidator::Validate(config);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_TRUE(status.message().find("nonexistent") != std::string::npos);
}

TEST(BuildValidationTest, DuplicateNodeNameFails) {
  GraphConfig config;
  config.nodes.push_back({"dup", "A", {}, {}, {}, {}, {}, "", "", 1, 0});
  config.nodes.push_back({"dup", "B", {}, {}, {}, {}, {}, "", "", 1, 0});
  auto status = ConfigValidator::Validate(config);
  ASSERT_FALSE(status.ok());
  EXPECT_TRUE(status.message().find("duplicate node") != std::string::npos);
}

TEST(BuildValidationTest, DuplicateStreamNameFails) {
  GraphConfig config;
  config.nodes.push_back({"a", "A", {}, {}, {}, {}, {}, "", "", 1, 0});
  config.nodes.push_back({"b", "B", {}, {}, {}, {}, {}, "", "", 1, 0});
  config.streams.push_back({"dup", "a", "out", "b", "in"});
  config.streams.push_back({"dup", "b", "out", "a", "in"});
  auto status = ConfigValidator::Validate(config);
  ASSERT_FALSE(status.ok());
  EXPECT_TRUE(status.message().find("duplicate stream") != std::string::npos);
}

TEST(BuildValidationTest, SelfLoopFails) {
  GraphConfig config;
  config.nodes.push_back({"self", "S", {}, {}, {}, {}, {}, "", "", 1, 0});
  config.streams.push_back({"s1", "self", "out", "self", "in"});
  auto status = ConfigValidator::Validate(config);
  ASSERT_FALSE(status.ok());
  EXPECT_TRUE(status.message().find("self-loop") != std::string::npos);
}

TEST(BuildValidationTest, GraphBuilderCallsValidator) {
  // Invalid config should be rejected by GraphBuilder::Build
  GraphConfig config;
  config.nodes.push_back({"a", "StringProducer", {}, {}, {}, {}, {}, "", "", 1, 0});
  config.streams.push_back({"s1", "a", "out", "missing", "in"});
  auto result = GraphBuilder::Build(config);
  ASSERT_FALSE(result.ok());
}

}  // namespace graph::runtime
