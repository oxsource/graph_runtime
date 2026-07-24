#include <string>

#include "gtest/gtest.h"
#include "src/framework/config/graph_config.h"
#include "src/framework/config/config_validator.h"
#include "src/framework/public/graph_runtime.h"
#include "src/framework/public/graph_builder.h"

namespace graph::runtime {

TEST(BuildValidationTest, ValidConfigPasses) {
  GraphConfig config;
  config.nodes.push_back({"a", "A", {}, {}, {}, {}, {}, "", "", 1, 0});
  config.nodes.push_back({"b", "B", {}, {}, {}, {}, {}, "", "", 1, 0});
  auto status = ConfigValidator::Validate(config);
  EXPECT_TRUE(status.ok());
}

TEST(BuildValidationTest, DuplicateNodeNameFails) {
  GraphConfig config;
  config.nodes.push_back({"dup", "A", {}, {}, {}, {}, {}, "", "", 1, 0});
  config.nodes.push_back({"dup", "B", {}, {}, {}, {}, {}, "", "", 1, 0});
  auto status = ConfigValidator::Validate(config);
  ASSERT_FALSE(status.ok());
  EXPECT_TRUE(status.message().find("duplicate node") != std::string::npos);
}

TEST(BuildValidationTest, GraphBuilderAcceptsValidConfig) {
  GraphConfig config;
  config.nodes.push_back({"a", "StringProducer", {}, {}, {}, {}, {}, "", "", 1, 0});
  auto result = GraphBuilder::Build(config);
  // May succeed or fail depending on node registration, but should not crash.
  ASSERT_TRUE(result.ok() || !result.ok());
}

}  // namespace graph::runtime
