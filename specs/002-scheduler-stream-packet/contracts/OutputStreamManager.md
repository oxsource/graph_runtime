# Contract: OutputStreamManager

**File**: `graph_runtime/src/stream/output_stream_manager.h`

```cpp
namespace graph::runtime {

struct Mirror {
  InputStreamHandler* handler;
  CollectionItemId id;
};

struct OutputStreamSpec {
  std::string name;
  bool locked_intro_data = false;  // after Open(), no more SetOffset/SetHeader
  bool offset_enabled = false;
  TimestampDiff offset;
  Packet header;
};

class OutputStreamManager {
 public:
  explicit OutputStreamManager(std::string name);

  const std::string& Name() const;

  // --- Graph construction ---
  void AddMirror(InputStreamHandler* handler, CollectionItemId id);
  void PrepareForRun(ErrorCallback error_callback);

  // --- Propagation ---
  // Compute the next timestamp bound from shard contents and input timestamp.
  Timestamp ComputeOutputTimestampBound(
      const OutputStreamShard& shard, Timestamp input_timestamp) const;

  // Propagate packets and bound to all mirrors.
  // Last mirror receives MovePackets (zero-copy); others receive AddPackets (copy).
  void PropagateUpdatesToMirrors(Timestamp next_bound,
                                  OutputStreamShard* shard);

  // Header propagation and metadata locking (after Open()).
  void PropagateHeader();
  void LockIntroData();

  // --- Reset shard before each Process() ---
  void ResetShard(OutputStreamShard* shard);

  // --- Lifecycle ---
  void Close();              // set bound=Done, notify all mirrors
  bool IsClosed() const;
  Timestamp NextTimestampBound() const;

  // --- Spec access ---
  OutputStreamSpec* Spec();

 private:
  OutputStreamSpec spec_;
  std::vector<Mirror> mirrors_;
  Timestamp next_timestamp_bound_{Timestamp::Unset()};
  bool closed_ = false;
  int64_t num_packets_added_ = 0;
};

}  // namespace graph::runtime
```

**Semantics**:
- One `OutputStreamManager` per output stream in the entire graph. Persistent across invocations.
- `AddMirror()` registers a downstream `InputStreamHandler` + `CollectionItemId`. Called by `GraphBuilder` during `Initialize()` when wiring input streams to their upstream output.
- `ComputeOutputTimestampBound(shard, input_ts)` computes:
  ```
  new_bound = max(
      AddOffset(input_timestamp) + 1,                    // input-based bound
      shard.LastAddedPacketTimestamp().NextAllowedInStream(),  // shard-based bound
      shard.updated_next_timestamp_bound_                 // explicit SetNextBound
  )
  ```
- `PropagateUpdatesToMirrors()` writes packets and bounds to all mirrors:
  - Updates internal `next_timestamp_bound_` and `num_packets_added_`.
  - Mirror loop: last mirror gets `MovePackets()`, earlier mirrors get `AddPackets()`.
  - If bound changed: calls `InputStreamHandler::SetNextTimestampBound(id, bound)`.
- `ResetShard(shard)`: sets shard to current `next_timestamp_bound_` and `closed_` state before each `Process()`.
- `Close()`: sets `closed_ = true`, `next_timestamp_bound_ = Timestamp::Done()`, propagates Done bound to all mirrors.
- `Spec()`: returns the `OutputStreamSpec` for shard initialization.
