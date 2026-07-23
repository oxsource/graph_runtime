# Contract: GraphConfig

**File**: `graph_runtime/src/config/graph_config.h`

```cpp
namespace graph::runtime {

// Configuration data for a single graph pipeline.
// Passed to GraphRuntime::Initialize() or GraphBuilder::Build().
struct GraphConfig {
  // Global settings
  int max_queue_size = 100;              // default max queue depth (-1 = unlimited)
  bool report_deadlock = false;          // true = deadlock fails graph instead of auto-unthrottle

  struct NodeDef {
    std::string name;                     // unique node name
    std::string type;                     // registered type name (NodeFactoryRegistry)
    std::vector<std::string> input_streams;   // "port_name:stream_name" mapping
    std::vector<std::string> output_streams;  // "port_name:stream_name" mapping
    std::vector<std::string> input_side_packets;  // "TAG:name" for side packet inputs
    std::vector<std::string> output_side_packets; // "TAG:name" for side packet outputs
    NodeOptions options;                  // node-specific configuration
    std::string executor;                 // named executor (empty = default)
    std::string input_stream_handler;     // "" = DefaultInputStreamHandler
    int max_in_flight = 1;               // concurrent Process invocations (Phase 2)
    int source_layer = 0;                // source activation order (Phase 2)
  };

  struct StreamDef {
    std::string name;                     // unique stream name
    std::string source_node;
    std::string source_port;
    std::string dest_node;
    std::string dest_port;
  };

  struct ExecutorDef {
    std::string name;                     // "" = default executor
    std::string type;                     // "ThreadPoolExecutor" or "ApplicationThreadExecutor"
    int num_threads = 0;                  // 0 = auto (min(CPUs, node_count))
  };

  // Graph topology
  std::vector<NodeDef> nodes;
  std::vector<StreamDef> streams;
  std::vector<ExecutorDef> executors;

  // External input/output stream declarations
  // These create virtual GraphInputStream/GraphOutputStream nodes.
  std::vector<std::string> input_streams;
  std::vector<std::string> output_streams;
};

}  // namespace graph::runtime
```

**Semantics**:
- `NodeDef` describes one calculator node. `type` must match a registered name in `NodeFactoryRegistry`. `input_streams` and `output_streams` use the format `"port_name:stream_name"` (e.g., `"input:audio_stream"`). `executor` assigns the node to a named executor defined in `executors`.
- `StreamDef` connects one node's output port to another node's input port. All referenced node names must exist in `nodes`. No duplicate stream names.
- `ExecutorDef` defines a named executor pool. `""` (empty string) is the default executor name. `num_threads = 0` means auto-detect based on `min(CPUs, node_count)`.
- `input_streams` and `output_streams` declare external I/O ports. Each name in `input_streams` creates a virtual `GraphInputStream` source node that accepts data via `GraphRuntime::AddPacketToInputStream()`. Each name in `output_streams` creates a virtual `GraphOutputStream` sink node that fires callbacks registered via `GraphRuntime::SetOutputStreamCallback()`.
