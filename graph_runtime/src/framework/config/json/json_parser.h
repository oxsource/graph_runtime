#ifndef GRAPH_RUNTIME_JSON_PARSER_H_
#define GRAPH_RUNTIME_JSON_PARSER_H_

#include <string>

#include "absl/status/statusor.h"
#include "src/config/i_graph_config_parser.h"
#include "src/config/graph_config.h"

namespace graph::runtime {

class JsonParser : public IGraphConfigParser {
 public:
  JsonParser() = default;

  absl::StatusOr<GraphConfig> Parse(
      const std::string& file_path) override;
};

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_JSON_PARSER_H_
