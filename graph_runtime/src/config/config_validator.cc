#include "src/config/config_validator.h"

#include <set>
#include <string>

#include "absl/strings/str_cat.h"

namespace graph::runtime {

absl::Status ConfigValidator::Validate(const GraphConfig& config) {
  auto status = ValidateUniqueNames(config);
  if (!status.ok()) return status;
  status = ValidateNodeRefs(config);
  if (!status.ok()) return status;
  status = ValidateNoSelfLoops(config);
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
  std::set<std::string> stream_names;
  for (const auto& stream : config.streams) {
    if (!stream_names.insert(stream.name).second) {
      return absl::InvalidArgumentError(
          absl::StrCat("duplicate stream name: ", stream.name));
    }
  }
  return absl::OkStatus();
}

absl::Status ConfigValidator::ValidateNodeRefs(
    const GraphConfig& config) {
  std::set<std::string> node_names;
  for (const auto& node : config.nodes) {
    node_names.insert(node.name);
  }
  for (const auto& stream : config.streams) {
    if (node_names.find(stream.source_node) == node_names.end()) {
      return absl::InvalidArgumentError(absl::StrCat(
          "stream '", stream.name, "': source node '", stream.source_node,
          "' not found"));
    }
    if (node_names.find(stream.dest_node) == node_names.end()) {
      return absl::InvalidArgumentError(absl::StrCat(
          "stream '", stream.name, "': dest node '", stream.dest_node,
          "' not found"));
    }
  }
  return absl::OkStatus();
}

absl::Status ConfigValidator::ValidateNoSelfLoops(
    const GraphConfig& config) {
  for (const auto& stream : config.streams) {
    if (stream.source_node == stream.dest_node) {
      return absl::InvalidArgumentError(
          absl::StrCat("stream '", stream.name, "': self-loop detected"));
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
