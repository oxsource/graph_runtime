#include "src/framework/config/config_validator.h"

#include <map>
#include <set>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"

namespace graph::runtime {

namespace {

// Extract the stream name from "port:stream" format. Returns the part
// after the colon, or the full string if no colon is present.
std::string StreamName(const std::string& port_stream) {
  auto pos = port_stream.find(':');
  if (pos == std::string::npos) return port_stream;
  return port_stream.substr(pos + 1);
}

// Build a set of all output stream names from all nodes.
std::set<std::string> CollectOutputStreams(const GraphConfig& config) {
  std::set<std::string> streams;
  for (const auto& node : config.nodes) {
    for (const auto& os : node.output_streams) {
      streams.insert(StreamName(os));
    }
  }
  for (const auto& os : config.output_streams) {
    streams.insert(StreamName(os));
  }
  return streams;
}

// Build a set of graph-level input stream names (external inputs).
std::set<std::string> CollectGraphInputStreams(const GraphConfig& config) {
  return {config.input_streams.begin(), config.input_streams.end()};
}

// Build adjacency list: node index → indices of nodes it depends on.
// Node A depends on node B if A has an input stream that B has as output.
std::vector<std::vector<int>> BuildDependencyGraph(const GraphConfig& config) {
  int n = static_cast<int>(config.nodes.size());
  std::vector<std::vector<int>> deps(n);

  // Map: stream name → list of node indices that produce it.
  std::map<std::string, std::vector<int>> producers;
  for (int i = 0; i < n; ++i) {
    for (const auto& os : config.nodes[i].output_streams) {
      producers[StreamName(os)].push_back(i);
    }
  }

  // For each node, its input streams connect to producer nodes.
  for (int i = 0; i < n; ++i) {
    for (const auto& is : config.nodes[i].input_streams) {
      auto stream = StreamName(is);
      auto it = producers.find(stream);
      if (it != producers.end()) {
        deps[i].insert(deps[i].end(), it->second.begin(), it->second.end());
      }
    }
  }
  return deps;
}

}  // namespace

absl::Status ConfigValidator::Validate(const GraphConfig& config) {
  auto status = ValidateUniqueNames(config);
  if (!status.ok()) return status;
  status = ValidateUniqueExecutors(config);
  if (!status.ok()) return status;
  status = ValidateConnectivity(config);
  if (!status.ok()) return status;
  status = ValidateNoCycles(config);
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

absl::Status ConfigValidator::ValidateConnectivity(const GraphConfig& config) {
  auto output_streams = CollectOutputStreams(config);
  auto graph_inputs = CollectGraphInputStreams(config);

  for (const auto& node : config.nodes) {
    for (const auto& is : node.input_streams) {
      auto stream = StreamName(is);
      // Graph-level inputs don't need a producer node.
      if (graph_inputs.count(stream)) continue;
      if (!output_streams.count(stream)) {
        return absl::InvalidArgumentError(absl::StrCat(
            "node '", node.name, "': input stream '", stream,
            "' has no matching output stream in any node"));
      }
    }
  }
  return absl::OkStatus();
}

absl::Status ConfigValidator::ValidateNoCycles(const GraphConfig& config) {
  int n = static_cast<int>(config.nodes.size());
  auto deps = BuildDependencyGraph(config);

  // DFS state: 0 = unvisited, 1 = in-progress, 2 = done.
  std::vector<int> state(n, 0);
  std::vector<int> path;

  auto has_cycle = false;
  std::vector<int> cycle_nodes;

  // Non-recursive DFS to detect cycles.
  struct Frame {
    int node;
    size_t next_child;
  };

  for (int start = 0; start < n; ++start) {
    if (state[start] != 0) continue;

    std::vector<Frame> stack{{start, 0}};
    state[start] = 1;
    path.push_back(start);

    while (!stack.empty()) {
      auto& frame = stack.back();
      if (frame.next_child >= deps[frame.node].size()) {
        state[frame.node] = 2;
        stack.pop_back();
        path.pop_back();
        continue;
      }
      int child = deps[frame.node][frame.next_child++];
      if (state[child] == 1) {
        // Found a cycle: trace from child to end of path.
        has_cycle = true;
        auto it = std::find(path.begin(), path.end(), child);
        for (; it != path.end(); ++it) {
          cycle_nodes.push_back(*it);
        }
        cycle_nodes.push_back(child);  // close the loop
        break;
      }
      if (state[child] == 0) {
        state[child] = 1;
        path.push_back(child);
        stack.push_back({child, 0});
      }
    }
    if (has_cycle) break;

    // Reset path for next start node.
    path.clear();
  }

  if (has_cycle && !cycle_nodes.empty()) {
    std::string cycle_str;
    for (int idx : cycle_nodes) {
      if (!cycle_str.empty()) cycle_str += " → ";
      cycle_str += "'" + config.nodes[idx].name + "'";
    }
    return absl::InvalidArgumentError(
        absl::StrCat("cycle detected: ", cycle_str));
  }

  return absl::OkStatus();
}

}  // namespace graph::runtime
