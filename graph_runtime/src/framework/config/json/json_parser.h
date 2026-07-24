#ifndef GRAPH_RUNTIME_JSON_PARSER_H_
#define GRAPH_RUNTIME_JSON_PARSER_H_

#include <string>

#include "absl/status/statusor.h"
#include "src/framework/config/i_graph_config_parser.h"
#include "src/framework/config/graph_config.h"

namespace graph::runtime {

class JsonParser : public IGraphConfigParser {
 public:
  JsonParser() = default;

  absl::StatusOr<GraphConfig> Parse(
      const std::string& file_path) override;

  /// Parse a JSON string directly, without reading from a file.
  absl::StatusOr<GraphConfig> ParseFromString(
      const std::string& json_text);
};

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_JSON_PARSER_H_
