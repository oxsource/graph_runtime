#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "src/config/i_graph_config_parser.h"
#include "src/config/parser_registry.h"
#include "src/config/config_validator.h"

namespace graph::runtime {

class KeyValueParser : public IGraphConfigParser {
 public:
  absl::StatusOr<GraphConfig> Parse(
      const std::string& file_path) override {
    std::ifstream file(file_path);
    if (!file.is_open()) {
      return absl::NotFoundError(
          absl::StrCat("config file not found: ", file_path));
    }
    GraphConfig config;
    std::string line;
    while (std::getline(file, line)) {
      if (line.empty() || line[0] == '#') continue;
      auto parts = absl::StrSplit(line, ':', absl::SkipEmpty());
      std::vector<std::string> tokens(parts.begin(), parts.end());
      if (tokens.empty()) continue;
      if (tokens[0] == "node" && tokens.size() >= 3) {
        GraphConfig::NodeDef def;
        def.name = tokens[1];
        def.type = tokens[2];
        if (tokens.size() > 3) def.input_streams.push_back(tokens[3]);
        if (tokens.size() > 4) def.output_streams.push_back(tokens[4]);
        config.nodes.push_back(std::move(def));
      } else if (tokens[0] == "stream" && tokens.size() >= 6) {
        GraphConfig::StreamDef def;
        def.name = tokens[1];
        def.source_node = tokens[2];
        def.source_port = tokens[3];
        def.dest_node = tokens[4];
        def.dest_port = tokens[5];
        config.streams.push_back(std::move(def));
      }
    }
    auto status = ConfigValidator::Validate(config);
    if (!status.ok()) return status;
    return config;
  }
};

}  // namespace graph::runtime

GRAPH_RUNTIME_REGISTER_PARSER("kv", graph::runtime::KeyValueParser);

int main() {
  using namespace graph::runtime;
  std::cout << "Custom KeyValueParser registered for .kv extension" << std::endl;

  auto parser = ParserRegistry::CreateForFile("test.kv");
  if (!parser) {
    std::cerr << "Parser not found for .kv extension" << std::endl;
    return 1;
  }
  std::cout << "Parser created successfully via registry" << std::endl;
  return 0;
}
