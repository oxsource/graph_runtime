# Contract: GraphRuntime

**File**: `graph_runtime/src/public/graph_runtime.h`

```cpp
namespace graph::runtime {

// Top-level public API for the Graph Runtime framework.
// Users interact with this class to build, run, and control a graph pipeline.
class GraphRuntime {
 public:
  GraphRuntime();
  ~GraphRuntime();

  // --- Lifecycle ---

  // Build the graph from config (parses config, validates contracts, creates nodes/streams).
  // MUST be called before Start().
  absl::Status Initialize(const GraphConfig& config);

  // Start graph execution. Non-blocking — returns after setting up the scheduler.
  absl::Status Start();

  // Block until the graph completes or Shutdown() is called.
  absl::Status WaitUntilDone();

  // Gracefully stop graph execution. May be called from any thread.
  void Shutdown();

  // --- External data injection (GraphInputStream) ---

  // Inject a packet into a graph input stream.
  // The stream must be declared in the graph config as an input stream.
  absl::Status AddPacketToInputStream(const std::string& stream_name,
                                       Packet packet);

  // Close an input stream, signaling end-of-data from the outside.
  absl::Status CloseInputStream(const std::string& stream_name);

  // --- External output observation (GraphOutputStream) ---

  // Register a callback to receive packets from a graph output stream.
  // The stream must be declared in the graph config as an output stream.
  // The callback is invoked on the executor thread for each packet produced.
  void SetOutputStreamCallback(const std::string& stream_name,
                                std::function<void(const Packet&)> callback);

  // Remove a previously registered output callback.
  void ClearOutputStreamCallback(const std::string& stream_name);

 private:
  std::unique_ptr<internal::GraphImpl> impl_;
};

}  // namespace graph::runtime
```

**Semantics**:

- **GraphRuntime** is the top-level public API. Users create one instance, configure it with a `GraphConfig` (JSON/programmatic), and drive the lifecycle.

- **Initialize(config)**: Parses the config, validates node types and port contracts via `NodeFactoryRegistry`, creates `InputStreamManager`s, `OutputStream`s, `OutputStreamManager`s, `Node` instances, wires mirrors, creates the `Scheduler` with configured executors and `InputStreamHandler`. MUST be called before `Start()`.

- **Start()**: Calls `Scheduler::Schedule()` to begin event-driven execution. Non-blocking — returns immediately. Call `WaitUntilDone()` to block until completion.

- **WaitUntilDone()**: Blocks the calling thread. Returns `OkStatus()` on successful completion, or an error status if the graph encountered a runtime error.

- **Shutdown()**: Gracefully stops execution. Signals the Scheduler to terminate, drains pending tasks. Idempotent — safe to call multiple times.

- **AddPacketToInputStream(name, packet)**: Injects an external packet into the graph, as if produced by a virtual source node named `__graph_input_stream_<name>`. The scheduler treats this as data arriving on a Source Node — triggers downstream scheduling normally. If the input stream's queue is full, the call may block or return `ResourceExhaustedError` depending on policy.

- **CloseInputStream(name)**: Closes an external input stream, signaling end-of-data. Propagates `Timestamp::Done()` bound to downstream nodes.

- **SetOutputStreamCallback(name, callback)**: Registers a callback that fires for each packet sent to a graph output stream. Packets are delivered on the executor thread. The callback receives a copy of the packet — for zero-copy, capture by shared_ptr.

**GraphInputStream** (internal):
- Each input stream declared in the graph config creates a virtual Source Node with zero inputs and one output.
- The virtual Source Node's `Process()` is never called directly. Instead, `AddPacketToInputStream()` enqueues packets directly into the node's output `OutputStreamManager`, which propagates to downstream mirrors.
- This avoids the cost of a real Source Node `Process()` call while maintaining the same scheduling semantics.
- When the stream is closed via `CloseInputStream()`, the node transitions to closed state, and downstream completion detection proceeds normally.

**GraphOutputStream** (internal):
- Each output stream declared in the graph config is connected to a virtual Sink Node with one input and zero outputs.
- The virtual Sink Node's `Process()` receives packets from upstream and invokes the user-registered callback.
- If no callback is registered, packets are silently consumed (sink behavior).
- When all upstream streams are closed, the sink node closes and contributes to graph completion detection.
