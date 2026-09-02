// Error message format for all config validation errors:
//   "<entity_type> '<entity_name>': <failure_description>"
// Examples:
//   "config.json: duplicate node name: dup"
// All messages use single quotes around entity names for readability.

#ifndef GRAPH_RUNTIME_CONFIG_VALIDATOR_H_
#define GRAPH_RUNTIME_CONFIG_VALIDATOR_H_

#include "absl/status/status.h"
#include "src/framework/config/graph_config.h"

namespace graph::runtime {

class ConfigValidator {
 public:
  static absl::Status Validate(const GraphConfig& config);

  static absl::Status ValidateUniqueNames(const GraphConfig& config);
  static absl::Status ValidateUniqueExecutors(const GraphConfig& config);
  static absl::Status ValidateNoDuplicateNodeInputs(const GraphConfig& config);
  static absl::Status ValidateConnectivity(const GraphConfig& config);
  static absl::Status ValidateNoCycles(const GraphConfig& config);
};

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_CONFIG_VALIDATOR_H_
