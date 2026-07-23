#ifndef GRAPH_RUNTIME_GRAPH_CONFIG_H_
#define GRAPH_RUNTIME_GRAPH_CONFIG_H_

#include <string>
#include <vector>

#include "src/node/node_options.h"

namespace graph::runtime {

struct GraphConfig {
  int max_queue_size = 100;
  bool report_deadlock = false;

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
};

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_GRAPH_CONFIG_H_
