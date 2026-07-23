# Contract: GraphContext

**File**: `graph_runtime/src/node/graph_context.h`

```cpp
namespace graph::runtime {

// --- InputStreamShard (per input port, per invocation) ---

class InputStreamShard {
 public:
  // Access the current packet
  const Packet& Value() const;
  Packet& Value();               // mutable — allows moving out

  template <typename T>
  const T& Get() const;          // syntactic sugar: Value().Get<T>()

  // State
  bool IsEmpty() const;          // Value().IsEmpty()
  bool IsDone() const;           // stream closed and no more packets

  // Metadata
  const std::string& Name() const;
  Packet Header() const;         // header set by upstream during Open()

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

// --- OutputStreamShard (per output port, per invocation) ---

class OutputStreamShard {
 public:
  // Add packets
  void AddPacket(const Packet& packet);   // copy
  void AddPacket(Packet&& packet);        // move
  template <typename T>
  void Add(T* ptr, Timestamp timestamp);  // sugar: Adopt(ptr).At(timestamp)

  // Timestamp control
  void SetNextTimestampBound(Timestamp bound);
  Timestamp NextTimestampBound() const;
  void SetOffset(TimestampDiff offset);   // output ts = input ts + offset (Open only)

  // Lifecycle
  void Close();                           // sends Done bound
  bool IsClosed() const;

  // Header (Open only)
  void SetHeader(const Packet& packet);
  Packet Header() const;

  // State
  bool IsEmpty() const;
  const std::string& Name() const;

 private:
  std::list<Packet> output_queue_;
  bool closed_ = false;
  Timestamp next_timestamp_bound_{Timestamp::Unset()};
  Timestamp updated_next_timestamp_bound_{Timestamp::Unset()};
};

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

- **OutputStreamShard**: One per output port. The Node writes output via `AddPacket()`. After `Process()` returns, the Scheduler's task runner collects all shards' `output_queue_` and calls `OutputStream::Send()` to propagate to downstream `InputStreamManager` deques. `SetOffset()` and `SetHeader()` are only valid during `Open()`.

- **InputStreamShardSet / OutputStreamShardSet**: Tag/index-addressable collections. `Get("TAG")` or `Get("TAG", index)` or `Index(i)`. Support range iteration.

- **Open/Process/Close lifecycle**:

| Aspect | Open | Process | Close |
|--------|------|---------|-------|
| InputTimestamp | `Unstarted()` | Scheduled ts | `Done()` |
| Inputs state | Header available, no data | Current batch packet per shard | All IsDone() = true |
| Outputs state | SetHeader/SetOffset allowed | AddPacket allowed | AddPacket + Close allowed |

- **Options<T>()**: Typed access to node configuration from the graph config. Delegates to the `NodeOptions` map stored in the node's state.

- **SetOffset(offset)**: Applies a timestamp offset to ALL output streams, meaning output timestamp >= input_timestamp + offset. Per-stream override via `OutputStreamShard::SetOffset()`. Can only be called during `Open()`.
