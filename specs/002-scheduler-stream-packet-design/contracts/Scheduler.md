# Contract: Scheduler

**File**: `graph_runtime/src/scheduler/scheduler.h`

```cpp
namespace graph::runtime {

class Graph;
class InputStreamHandler;
class Executor;
class SchedulerQueue;

using ErrorCallback = std::function<void(absl::Status error)>;

enum class SchedulerState {
  kNotStarted = 0,
  kRunning = 1,
  kPaused = 2,
  kCancelling = 3,
  kTerminated = 4,
};

class Scheduler {
 public:
  virtual ~Scheduler() = default;

  // Pluggable components (set before Schedule())
  void SetInputStreamHandler(std::unique_ptr<InputStreamHandler> handler);
  void SetErrorCallback(ErrorCallback cb);

  // Executor management — supports heterogeneous executors.
  // "" = default executor (must be set); named = non-default.
  virtual absl::Status SetDefaultExecutor(std::shared_ptr<Executor> executor);
  virtual absl::Status SetNonDefaultExecutor(const std::string& name,
                                              std::shared_ptr<Executor> executor);

  // Lifecycle — Schedule() is NON-BLOCKING, sets up event observers and returns.
  virtual absl::Status Schedule(Graph& graph) = 0;

  // Blocks until graph completes naturally or Shutdown() is called.
  virtual absl::Status WaitUntilDone() = 0;

  virtual void Shutdown() = 0;
  virtual absl::Status Pause();
  virtual absl::Status Resume();

  SchedulerState state() const;
  bool IsTerminated() const;
  bool IsPaused() const;
  bool HasError() const;

  // Assign a node to its designated scheduler queue based on executor name.
  // Called by GraphBuilder during initialization.
  void AssignNodeToQueue(Node* node);

  // Reserved for Phase 2 dynamic graph support
  virtual absl::Status AddNode(Node* node);
  virtual absl::Status RemoveNode(Node* node);

 protected:
  Scheduler() = default;

  // --- Internal state ---
  SchedulerQueue& GetQueue(const std::string& executor_name);

  SchedulerQueue default_queue_;
  std::map<std::string, std::unique_ptr<SchedulerQueue>> non_default_queues_;
  std::vector<SchedulerQueue*> all_queues_;
  int non_idle_queue_count_ = 0;
  int handling_idle_ = 0;
  bool stopping_ = false;

  std::unique_ptr<InputStreamHandler> input_stream_handler_;
  std::shared_ptr<Executor> default_executor_;
};

}  // namespace graph::runtime
```

**Semantics**:
- `SetInputStreamHandler()` injects a custom readiness policy. Default: `DefaultInputStreamHandler` (all inputs ready → process).
- `SetErrorCallback()` registers a handler invoked when a Node returns a non-OK, non-Stop status. The callback sets `HasError()` and initiates the `kCancelling` transition.
- `SetDefaultExecutor()` sets the executor for the default queue (name = ""). Must be called before `Schedule()`. Typically a `ThreadPoolExecutor` or `ApplicationThreadExecutor`.
- `SetNonDefaultExecutor()` creates a new `SchedulerQueue` bound to the given named executor. The queue is registered with idle callbacks and added to `all_queues_`. Per-node executor assignment uses these names.

**State machine**:
```
kNotStarted ──Schedule()──► kRunning ──Pause()──► kPaused
                                 ▲                   │
                                 └────Resume()───────┘
                                   │
                      Shutdown()/error──► kCancelling ──► kTerminated
                                   │
                  All closed ──────┴──────► kTerminated
```

- `Schedule()` is **non-blocking**. It performs one-time initialization and installs event observers, then returns immediately:

  1. Build topological ordering from Graph's Node/Stream wiring.
  2. Group Source Nodes by `source_layer` (Phase 2: layer-ordered; Phase 1: all concurrent).
  3. Set up `InputStreamManager` per Node per input port:
     - `SetArrivalCallback()` → `InputStreamHandler::NotifyPacketArrival()`.
     - `SetQueueSizeCallbacks()` → throttle/unthrottle.
  4. Create `OutputStream` per Node per output port with mirrors.
  5. Register each queue's idle callback → `QueueIdleStateChanged()` (contributes to `non_idle_queue_count_`).
  6. For each Node: `AssignNodeToQueue(node)` — routes to `default_queue_` or named queue based on `Node::ExecutorName()`.
  7. Activate all Source Nodes (schedule their `Open` tasks).

- **AssignNodeToQueue(node)**: If `node->ExecutorName()` is non-empty, looks up the name in `non_default_queues_` and sets `node->SetSchedulerQueue(queue)`. Otherwise uses `default_queue_`. All subsequent scheduling for this node goes through the assigned queue.

- `non_idle_queue_count_` aggregates idle state across all queues. Each queue's `idle_callback_(true)` decrements; `idle_callback_(false)` increments. When the count reaches 0, `HandleIdle()` fires.

**Event-driven execution flow**:

```
--- OpenNode task runs ---
  // Prepare output shards, then call Open
  OutputStreamShardSet output_shards;     // initialized via OutputStreamHandler
  output_stream_handler_->SetupOutputShards(&output_shards);

  GraphContext context(node->name(), node->id(), node->CalculatorType(),
                       Timestamp::Unstarted(), input_shards, output_shards, &options)
  Node::Open(context)
  output_stream_handler_->Open(&output_shards);   // propagate headers, lock intro data

  After Open → Scheduler fires OnNodeOpened(node):
    if Source: schedule initial ProcessNode via node->GetSchedulerQueue()
    if non-source: register InputStreamManagers

--- ProcessNode task runs (via SchedulerQueue::RunNextTask) ---
  [if stopping_=true AND node is Source]
    → schedule CloseNode, skip Process

  // Prepare output shards (reset to manager's current state)
  output_stream_handler_->PrepareOutputs(scheduled_timestamp, &output_shards);

  GraphContext context(node->name(), node->id(), node->CalculatorType(),
                       scheduled_timestamp, input_shards, output_shards, &options)

  InputStreamHandler::FillInputSet(node, context)
    → PopPacketAtTimestamp on each InputStreamManager
    → populates each InputStreamShard's packet_queue_
    → side effect: may fire becomes_not_full_callback_ → unthrottle

  status = Node::Process(context)

  // Output propagation via OutputStreamHandler
  output_stream_handler_->PostProcess(scheduled_timestamp, &output_shards);

  // Source rescheduling via node->GetSchedulerQueue()->AddNode(node)

--- CloseNode task runs ---
  output_stream_handler_->PrepareOutputs(Timestamp::Done(), &output_shards);

  GraphContext context(node->name(), node->id(), node->CalculatorType(),
                       Timestamp::Done(), input_shards, output_shards, &options)
  Node::Close(context)
  output_stream_handler_->Close(&output_shards);

--- Queue idle state change ---
  idle_callback_(true) → QueueIdleStateChanged
    → non_idle_queue_count_--
    → if 0: HandleIdle()
```

**HandleIdle** — triggered from: `Schedule()`, `Resume()`, error callback, `non_idle_queue_count_ == 0`.

```
HandleIdle():
  [handling_idle_++ reentrancy guard]
  if HasError() && non_idle_queue_count_ == 0:
    Quit() → state_ = kTerminated
  CleanupActiveSources()
  if all sources closed AND all inputs closed AND no pending tasks:
    Quit() → state_ = kTerminated
  if throttled sources exist: auto-unthrottle (deadlock prevention)
  if sources active but idle: wait for events
  [handling_idle_--]
```

**Default configuration** (Phase 1):
- Executor: `ThreadPoolExecutor` with `num_threads = min(CPUs, node_count)` — multi-threaded by default.
- Fallback: `ApplicationThreadExecutor` via `DelegatingExecutor` for single-threaded environments.
- Default queue: `default_queue_` bound to default executor.
- Per-node executor assignment via `Node::SetExecutorName()` — empty = default.
