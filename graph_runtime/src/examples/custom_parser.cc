#include <fstream>
#include <string>
#include <vector>

#include "src/framework/utils/logger.h"

#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "src/framework/config/i_graph_config_parser.h"
#include "src/framework/config/parser_registry.h"
#include "src/framework/config/config_validator.h"

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
  Logger::Info("graphrt::example", "Custom KeyValueParser registered for .kv extension");

  auto parser = ParserRegistry::CreateForFile("test.kv");
  if (!parser) {
    Logger::Error("graphrt::example", "Parser not found for .kv extension");
    return 1;
  }
  Logger::Info("graphrt::example", "Parser created successfully via registry");
  return 0;
}
