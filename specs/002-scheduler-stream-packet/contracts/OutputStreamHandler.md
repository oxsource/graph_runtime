# Contract: OutputStreamHandler

**File**: `graph_runtime/src/stream/output_stream_handler.h`

```cpp
namespace graph::runtime {

using OutputStreamManagerSet = internal::Collection<OutputStreamManager*>;

class OutputStreamHandler {
 public:
  explicit OutputStreamHandler(
      std::shared_ptr<tool::TagMap> tag_map,
      GraphContextManager* context_manager);

  virtual ~OutputStreamHandler() = default;

  // --- Setup ---
  void InitializeOutputStreamManagers(OutputStreamManager* flat_array);
  void SetupOutputShards(OutputStreamShardSet* shards);
  void PrepareForRun(ErrorCallback error_callback);

  // --- Lifecycle ---
  // Open: propagate headers, lock intro data (after Node::Open)
  void Open(OutputStreamShardSet* shards);

  // PrepareOutputs: reset all shards before each Process()
  void PrepareOutputs(Timestamp input_timestamp,
                      OutputStreamShardSet* shards);

  // PostProcess: propagate after Node::Process returns
  void PostProcess(Timestamp input_timestamp,
                   OutputStreamShardSet* shards);

  // Close: propagate remaining packets, close all managers
  void Close(OutputStreamShardSet* shards);

  // --- Timestamp bound propagation ---
  void UpdateTaskTimestampBound(Timestamp bound);
  void TryPropagateTimestampBound(Timestamp input_bound);

 protected:
  OutputStreamManagerSet output_stream_managers_;
  GraphContextManager* context_manager_;

  // Phase 2: parallel propagation state machine
  std::set<Timestamp> completed_input_timestamps_;
  Timestamp task_timestamp_bound_;
};

// Default implementation — Phase 1 single-threaded path.
// PostProcess directly calls PropagateOutputPackets without state machine.
class InOrderOutputStreamHandler : public OutputStreamHandler {
 public:
  using OutputStreamHandler::OutputStreamHandler;

  // Phase 2: virtual PropagationLoop() for parallel execution
  virtual void PropagationLoop();
};

}  // namespace graph::runtime
```

**Semantics**:
- One `OutputStreamHandler` per Node. Owned by `Node`. Orchestrates all output streams of that node.
- `InitializeOutputStreamManagers(flat)` receives a pointer into a flat `OutputStreamManager[]` array owned by `Graph`. Each handler's slice is determined by its node's output stream count.
- `SetupOutputShards(shards)` assigns each `OutputStreamShard` its corresponding `OutputStreamSpec` from the manager.
- `Open()`: propagates any packets/bounds set during `Node::Open()`, then calls `PropagateHeader()` and `LockIntroData()` on each manager.
- `PrepareOutputs(ts, shards)`: calls `OutputStreamManager::ResetShard()` for each output stream.
- `PostProcess(ts, shards)`: core propagation entry point. For Phase 1 (sequential):
  1. For each `OutputStreamManager`:
     - `ComputeOutputTimestampBound(shard, ts)` → `output_bound`.
     - `PropagateUpdatesToMirrors(output_bound, shard)`.
     - If `shard.IsClosed()`: `manager->Close()`.
- `Close()`: propagates all remaining packets with bound `Timestamp::Done()`, then closes all managers.
- `UpdateTaskTimestampBound(bound)`: receives input bound from the scheduler; calls `TryPropagateTimestampBound()` which advances offset-enabled output streams' bounds without waiting for a `Process()` call.
