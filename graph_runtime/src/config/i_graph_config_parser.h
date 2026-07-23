#ifndef GRAPH_RUNTIME_I_GRAPH_CONFIG_PARSER_H_
#define GRAPH_RUNTIME_I_GRAPH_CONFIG_PARSER_H_

#include <string>

#include "absl/status/statusor.h"
#include "src/config/graph_config.h"

namespace graph::runtime {

class IGraphConfigParser {
 public:
  virtual ~IGraphConfigParser() = default;

  virtual absl::StatusOr<GraphConfig> Parse(
      const std::string& file_path) = 0;
};

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_I_GRAPH_CONFIG_PARSER_H_
