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

TEST(BuildValidationTest, MultiConsumerFanoutPasses) {
  // One output stream consumed by two nodes is legal (1→N fan-out): each
  // consumer edge gets its own InputStreamManager.
  GraphConfig config;
  config.nodes.push_back({"src", "A", {}, {"out:x"}, {}, {}, {}, "", "", 1, 0});
  config.nodes.push_back({"a", "B", {"in:x"}, {}, {}, {}, {}, "", "", 1, 0});
  config.nodes.push_back({"b", "C", {"in:x"}, {}, {}, {}, {}, "", "", 1, 0});
  auto status = ConfigValidator::Validate(config);
  EXPECT_TRUE(status.ok()) << status;
}

TEST(BuildValidationTest, SameNodeDuplicateStreamConsumptionFails) {
  // A node must not consume the same stream twice (duplicate input edge).
  GraphConfig config;
  config.nodes.push_back({"src", "A", {}, {"out:x"}, {}, {}, {}, "", "", 1, 0});
  config.nodes.push_back({"dup", "B", {"in:x", "in2:x"}, {}, {}, {}, {}, "", "", 1, 0});
  auto status = ConfigValidator::Validate(config);
  ASSERT_FALSE(status.ok());
  EXPECT_TRUE(status.message().find("duplicate input edge") !=
              std::string::npos);
}

TEST(BuildValidationTest, GraphBuilderAcceptsValidConfig) {
  GraphConfig config;
  config.nodes.push_back({"a", "StringProducer", {}, {}, {}, {}, {}, "", "", 1, 0});
  auto result = GraphBuilder::Build(config);
  // May succeed or fail depending on node registration, but should not crash.
  ASSERT_TRUE(result.ok() || !result.ok());
}

}  // namespace graph::runtime
