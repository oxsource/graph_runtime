#include "src/framework/config/config_validator.h"

#include <set>
#include <string>

#include "absl/strings/str_cat.h"

namespace graph::runtime {

absl::Status ConfigValidator::Validate(const GraphConfig& config) {
  auto status = ValidateUniqueNames(config);
  if (!status.ok()) return status;
  status = ValidateUniqueExecutors(config);
  if (!status.ok()) return status;
  return absl::OkStatus();
}

absl::Status ConfigValidator::ValidateUniqueNames(
    const GraphConfig& config) {
  std::set<std::string> node_names;
  for (const auto& node : config.nodes) {
    if (!node_names.insert(node.name).second) {
      return absl::InvalidArgumentError(
          absl::StrCat("duplicate node name: ", node.name));
    }
  }
  return absl::OkStatus();
}

absl::Status ConfigValidator::ValidateUniqueExecutors(
    const GraphConfig& config) {
  std::set<std::string> executor_names;
  for (const auto& executor : config.executors) {
    if (!executor_names.insert(executor.name).second) {
      return absl::InvalidArgumentError(
          absl::StrCat("duplicate executor name: ", executor.name));
    }
  }
  return absl::OkStatus();
}

}  // namespace graph::runtime
