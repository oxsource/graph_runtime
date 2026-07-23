// Error message format for all config validation errors:
//   "<file_path>: <entity_type> '<entity_name>': <failure_description>"
// Examples:
//   "pipeline.json: stream 's1': source node 'missing' not found"
//   "config.json: duplicate node name: dup"
//   "test.json: stream 'loop': self-loop detected"
// All messages include the source file path prefix and use single quotes
// around entity names for readability in terminal output.

#ifndef GRAPH_RUNTIME_CONFIG_VALIDATOR_H_
#define GRAPH_RUNTIME_CONFIG_VALIDATOR_H_

#include "absl/status/status.h"
#include "src/config/graph_config.h"

namespace graph::runtime {

class ConfigValidator {
 public:
  static absl::Status Validate(const GraphConfig& config);

  static absl::Status ValidateNodeRefs(const GraphConfig& config);
  static absl::Status ValidateUniqueNames(const GraphConfig& config);
  static absl::Status ValidateNoSelfLoops(const GraphConfig& config);
  static absl::Status ValidateUniqueExecutors(const GraphConfig& config);
};

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_CONFIG_VALIDATOR_H_
