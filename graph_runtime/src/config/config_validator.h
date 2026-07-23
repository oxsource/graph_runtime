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
