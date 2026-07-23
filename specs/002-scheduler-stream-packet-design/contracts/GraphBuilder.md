# Contract: GraphBuilder

**File**: `graph_runtime/src/public/graph_builder.h`

```cpp
namespace graph::runtime {

// Constructs a fully-wired GraphRuntime from a GraphConfig.
// Validation and instantiation all happen in Build() before any runtime starts.
class GraphBuilder {
 public:
  // Build a GraphRuntime from config. Returns error on validation failure.
  static absl::StatusOr<std::unique_ptr<GraphRuntime>> Build(
      const GraphConfig& config);

 private:
  // Internal steps (called sequentially by Build()):
  // 1. ValidateContracts — call NodeFactory::GetContract per node:
  //    a. Check stream port types (Inputs/Outputs)
  //    b. Check side packet types and names (InputSidePackets/OutputSidePackets)
  //    c. Validate options against OptionsRegistry
  // 2. CreateExecutors — instantiate ThreadPoolExecutor per ExecutorDef
  // 3. CreateNodes — NodeFactoryRegistry::CreateByName for each NodeDef
  // 4. CreateInputStreamManagers — one per node input port
  // 5. CreateOutputStreams + OutputStreamManagers — one per node output port
  // 6. WireMirrors — populate OutputStreamManager::mirrors_ from StreamDef
  // 7. CreateGraphInputStreams — virtual source nodes for external input
  // 8. CreateGraphOutputStreams — virtual sink nodes for external output
  // 9. CreateOutputStreamHandlers — one per node
  // 10. CreateScheduler — with configured executors and InputStreamHandler
  // 11. AssignNodesToQueues — Scheduler::AssignNodeToQueue per node
  // 12. Return GraphRuntime
};

}  // namespace graph::runtime
```

**Semantics**:
- `Build(config)` is the single entry point for constructing a runnable graph. It performs the full build pipeline: validate → create → wire → return.
- Validation errors (unknown node type, port type mismatch, duplicate names, missing references) are reported at build time, not at runtime.
- The returned `GraphRuntime` is fully initialized and ready for `Start()`.
- GraphBuilder is a pure static utility class — no state is stored between calls.
