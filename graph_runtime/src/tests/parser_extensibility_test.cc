#include <cstdio>
#include <string>

#include "gtest/gtest.h"
#include "src/config/i_graph_config_parser.h"
#include "src/config/parser_registry.h"
#include "src/config/config_validator.h"

namespace graph::runtime {

// A minimal custom parser implementation for testing pluggability.
class TestCustomParser : public IGraphConfigParser {
 public:
  absl::StatusOr<GraphConfig> Parse(
      const std::string& file_path) override {
    if (file_path == "notfound.txt") {
      return absl::NotFoundError("file not found");
    }
    GraphConfig config;
    config.nodes.push_back({"custom_node", "CustomType", {}, {}, {}, {}, {},
                            "", "", 1, 0});
    return config;
  }
};

// Register the custom parser for the .txt extension — file-scope static.
GRAPH_RUNTIME_REGISTER_PARSER("txt", TestCustomParser);

TEST(ParserExtensibilityTest, CustomParserRegistered) {
  EXPECT_TRUE(ParserRegistry::IsFormatSupported("test.txt"));
  EXPECT_FALSE(ParserRegistry::IsFormatSupported("test.unknown"));
}

TEST(ParserExtensibilityTest, CreateViaRegistry) {
  auto parser = ParserRegistry::CreateForFile("config.txt");
  ASSERT_NE(parser, nullptr);
  auto result = parser->Parse("config.txt");
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result->nodes.size(), 1);
  EXPECT_EQ(result->nodes[0].name, "custom_node");
}

TEST(ParserExtensibilityTest, UnregisteredExtension) {
  auto parser = ParserRegistry::CreateForFile("config.yaml");
  EXPECT_EQ(parser, nullptr);
}

TEST(ParserExtensibilityTest, CustomParserErrorPropagation) {
  auto parser = ParserRegistry::CreateForFile("notfound.txt");
  ASSERT_NE(parser, nullptr);
  auto result = parser->Parse("notfound.txt");
  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), absl::StatusCode::kNotFound);
}

}  // namespace graph::runtime
