# Contract: GraphContext

**File**: `graph_runtime/src/node/graph_context.h`

```cpp
namespace graph::runtime {

// --- InputStreamShard (per input port, per invocation) ---

// --- InputStreamShard (per input port, per invocation) ---
// Implements InputStream — see separate contract at InputStream.md for the interface.

class InputStreamShard : public InputStream {
 public:
  const std::string& Name() const final;
  const Packet& Value() const final;
  Packet& Value() final;
  bool IsEmpty() const final;
  bool IsDone() const final;
  Packet Header() const final;

 private:
  std::queue<Packet> packet_queue_;
  bool is_done_ = false;
};

// --- InputStreamShardSet (all input ports) ---

class InputStreamShardSet {
 public:
  InputStreamShard& Get(const std::string& port_name);         // "TAG" or "TAG:index"
  InputStreamShard& Get(const std::string& tag, int index);
  InputStreamShard& Index(int i);
  int NumEntries() const;

  // Range iteration
  using iterator = InputStreamShard*;   // simplified
  iterator begin();
  iterator end();
};

// --- OutputStreamShard (see separate contract at OutputStreamShard.md) ---
// #include "graph_runtime/stream/output_stream_shard.h"

// --- OutputStreamShardSet (all output ports) ---

class OutputStreamShardSet {
 public:
  OutputStreamShard& Get(const std::string& port_name);
  OutputStreamShard& Get(const std::string& tag, int index);
  OutputStreamShard& Index(int i);
  int NumEntries() const;

  using iterator = OutputStreamShard*;
  iterator begin();
  iterator end();
};

// --- GraphContext (per-invocation, analogous to MediaPipe's CalculatorContext) ---

class GraphContext {
 public:
  // Constructor called by Scheduler task runner
  GraphContext(
      const std::string& node_name,
      int node_id,
      const std::string& calculator_type,
      Timestamp input_timestamp,
      InputStreamShardSet inputs,
      OutputStreamShardSet outputs,
      const NodeOptions* options);

  // Identity
  const std::string& NodeName() const;
  int NodeId() const;
  const std::string& CalculatorType() const;

  // Current invocation timestamp
  //   Open()   → Timestamp::Unstarted()
  //   Process() → scheduled timestamp
  //   Close()  → Timestamp::Done()
  Timestamp InputTimestamp() const;

  // Input/output
  InputStreamShardSet& Inputs();
  const InputStreamShardSet& Inputs() const;
  OutputStreamShardSet& Outputs();
  const OutputStreamShardSet& Outputs() const;

  // Options
  template <typename T>
  const T& Options() const;
  template <typename T>
  bool HasOptions() const;

  // Set offset on ALL output streams (Open only)
  void SetOffset(TimestampDiff offset);

 private:
  std::string node_name_;
  int node_id_;
  std::string calculator_type_;
  Timestamp input_timestamp_;
  InputStreamShardSet inputs_;
  OutputStreamShardSet outputs_;
  const NodeOptions* options_;
};

}  // namespace graph::runtime
```

**Semantics**:

- `GraphContext` is created by the Scheduler's task runner for each lifecycle invocation. It is valid only during the scope of that call.

- **InputStreamShard**: One per input port. The Scheduler's `FillInputSet()` populates each shard's `packet_queue_` with one Packet (from `InputStreamManager::PopPacketAtTimestamp()`). The Node reads via `Value()` / `Get<T>()`. If no packet was sent at this timestamp, the shard is populated with an empty Packet (`IsEmpty() == true`). `IsDone()` signals that the stream is closed and no more data will arrive.

- **OutputStreamShard**: One per output port. Implements the `OutputStream` abstract interface. The Node writes output via `AddPacket()`. After `Process()` returns, `OutputStreamHandler::PostProcess()` drains each shard via `OutputStreamManager::PropagateUpdatesToMirrors()`, which sends packets directly to downstream `InputStreamManager` deques. `SetOffset()` and `SetHeader()` are only valid during `Open()`.

- **InputStreamShardSet / OutputStreamShardSet**: Tag/index-addressable collections. `Get("TAG")` or `Get("TAG", index)` or `Index(i)`. Support range iteration.

- **Open/Process/Close lifecycle**:

| Aspect | Open | Process | Close |
|--------|------|---------|-------|
| InputTimestamp | `Unstarted()` | Scheduled ts | `Done()` |
| Inputs state | Header available, no data | Current batch packet per shard | All IsDone() = true |
| Outputs state | SetHeader/SetOffset allowed | AddPacket allowed | AddPacket + Close allowed |

- **Options<T>()**: Typed access to node configuration from the graph config. Delegates to the `NodeOptions` map stored in the node's state.

- **SetOffset(offset)**: Applies a timestamp offset to ALL output streams, meaning output timestamp >= input_timestamp + offset. Per-stream override via `OutputStreamShard::SetOffset()`. Can only be called during `Open()`.

---

### GraphContextManager

Manages lifecycle of `GraphContext` instances. Owned by `CalculatorNode`. Analogous to MediaPipe's `CalculatorContextManager`.

```cpp
class GraphContextManager {
 public:
  void Initialize(
      const std::string& node_name, int node_id,
      const std::string& calculator_type,
      InputStreamShardSet inputs, OutputStreamShardSet outputs,
      const NodeOptions* options);

  // Default context — used for Open(), Close(), and sequential Process()
  GraphContext* GetDefaultCalculatorContext();

  // Parallel execution (Phase 2):
  GraphContext* PrepareCalculatorContext(Timestamp input_timestamp);
  void RecycleCalculatorContext();
  void CleanupAfterRun();

 private:
  std::unique_ptr<GraphContext> default_context_;
  // Phase 2: std::map<Timestamp, std::unique_ptr<GraphContext>> active_contexts_;
  // Phase 2: std::deque<std::unique_ptr<GraphContext>> idle_contexts_;
};
```

**Semantics**:
- One `GraphContextManager` per Node. Manages the per-invocation `GraphContext` instances.
- `GetDefaultCalculatorContext()` returns a single reusable context. Used for `Open()`, `Close()`, and sequential `Process()` calls. Source Nodes always use this context.
- `PrepareCalculatorContext(ts)` (Phase 2): for parallel execution, creates or reuses a context per distinct input timestamp. Each active timestamp gets its own `GraphContext` with independent shards.
- `RecycleCalculatorContext()` (Phase 2): returns a context to the idle pool after its outputs have been propagated.
- `CleanupAfterRun()`: destroys all contexts after graph run completes.
