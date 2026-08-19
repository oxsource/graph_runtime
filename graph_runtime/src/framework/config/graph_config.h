#ifndef GRAPH_RUNTIME_GRAPH_CONFIG_H_
#define GRAPH_RUNTIME_GRAPH_CONFIG_H_

#include <string>
#include <vector>

#include "src/framework/node/node_options.h"
#include "src/framework/profiler/profiler_config.h"

namespace graph::runtime {

struct GraphConfig {
  int max_queue_size = 100;
  bool report_deadlock = false;
  ProfilerConfig profiler_config;

  struct NodeDef {
    std::string name;
    std::string type;
    std::vector<std::string> input_streams;
    std::vector<std::string> output_streams;
    std::vector<std::string> input_side_packets;
    std::vector<std::string> output_side_packets;
    NodeOptions options;
    std::string executor;
    std::string input_stream_handler;
    int max_in_flight = 1;
    int source_layer = 0;
  };

  struct ExecutorDef {
    std::string name;
    std::string type;
    int num_threads = 0;
  };

  std::vector<NodeDef> nodes;
  std::vector<ExecutorDef> executors;
  std::vector<std::string> input_streams;
  std::vector<std::string> output_streams;

  /// Reads an option of type `T` from the first node whose `type` matches
  /// `node_type` and has the key set. Falls back to `default_value` when no
  /// node of that type sets the key (or the graph has no such node).
  ///
  /// \code
  ///   int fps = config.GetNodeOption<int>("StreamInputNode", "fps");
  ///   int fps2 = config.GetNodeOption<int>("StreamInputNode", "fps", 30);
  /// \endcode
  ///
  /// Note: the value must have been stored with the same C++ type T
  /// (NodeOptions keys are type-checked at retrieval via std::any).
  template <typename T>
  T GetNodeOption(const std::string& node_type, const std::string& key,
                  const T& default_value = T{}) const {
    for (const auto& def : nodes) {
      if (def.type != node_type) continue;
      if (const T* v = def.options.Get<T>(key)) {
        return *v;
      }
    }
    return default_value;
  }
};

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_GRAPH_CONFIG_H_
