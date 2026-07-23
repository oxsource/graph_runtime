# Contract: Scheduler

**File**: `graph_runtime/src/scheduler/scheduler.h`

```cpp
namespace graph::runtime {

class Graph;
class InputStreamHandler;
class Executor;

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
  void SetExecutor(std::shared_ptr<Executor> executor);
  void SetErrorCallback(ErrorCallback cb);

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

  // Reserved for Phase 2 dynamic graph support
  virtual absl::Status AddNode(Node* node);
  virtual absl::Status RemoveNode(Node* node);

 protected:
  Scheduler() = default;
};

}  // namespace graph::runtime
```

**Semantics**:
- `SetInputStreamHandler()` injects a custom readiness policy. Default: `DefaultInputStreamHandler` (all inputs ready → process).
- `SetExecutor()` injects a task execution strategy. Default: `ApplicationThreadExecutor` (single-threaded, synchronous).
- `SetErrorCallback()` registers a handler invoked when a Node returns a non-OK, non-Stop status. The callback sets `HasError()` and initiates the `kCancelling` transition.

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
- `kNotStarted` (0): Initial state before `Schedule()` is called.
- `kRunning` (1): Actively scheduling and running Nodes.
- `kPaused` (2): All queues stopped, no new scheduling. Source Nodes pause production.
- `kCancelling` (3): Error recorded; pending tasks may still drain but no new scheduling. `Pause()` returns `FailedPreconditionError` in this state.
- `kTerminated` (4): Final state. `WaitUntilDone()` unblocks.

- `Schedule()` is **non-blocking**. It performs one-time initialization and installs event observers, then returns immediately. Execution is driven entirely by data events:

  1. Build topological ordering from Graph's Node/Stream wiring.
  2. Group Source Nodes by `source_layer` (default 0). (Phase 2: layer-ordered activation; Phase 1: all sources activate concurrently.)
  3. Create `InputStreamManager` per Node per input port — owns the `std::deque<Packet>`, tracks timestamp bound. Set up:
     - `SetArrivalCallback()` → `InputStreamHandler::NotifyPacketArrival()` on owning Node.
     - `SetQueueSizeCallbacks()` → Scheduler throttle/unthrottle logic.
  4. Create `OutputStream` per Node per output port — holds `mirrors_` (downstream `InputStreamManager*` list). GraphBuilder populates mirrors.
  5. Register end-of-stream observer: each `InputStreamManager::Close()` sets bound to `Timestamp::Done()`; downstream `InputStreamHandler` detects completion via `IsDone()`.
  6. Activate all Source Nodes (schedule their `Open` tasks). Source Nodes reuse a single default `GraphContext` across all invocations (max_in_flight is always 1 for sources).

- `Pause()` suspends scheduling. Queued tasks complete but no new Nodes are activated. Returns `FailedPreconditionError` if `stopping_ == true` (pausing during shutdown is contradictory).
- `Resume()` resumes scheduling after a pause. Calls `HandleIdle()` to re-evaluate.
- `state()` returns the current scheduler state. `IsTerminated()`/`IsPaused()` are convenience accessors.
- `HasError()` returns true if any Node returned a non-OK, non-Stop status (set via `ErrorCallback`).

- `WaitUntilDone()` blocks the calling thread until the graph enters `kTerminated`. When using `ApplicationThreadExecutor`, tasks are drained on the calling thread inside this method:
  ```
  while (!IsTerminated()) {
    if (app_thread_tasks_.empty()) {
      state_cond_var_.Wait(&mutex_);  // Sleep until signaled
    } else {
      auto task = app_thread_tasks_.front();
      app_thread_tasks_.pop_front();
      task();  // Execute on calling thread
    }
  }
  ```
  Graph completion (transitions to `kTerminated`) is signaled when:
  - All source Nodes have closed.
  - All non-source Nodes have closed (all input streams done).
  - No pending tasks remain on any Executor queue.

- **Stopping flag**: When a non-source Node returns `StatusStop`, the Scheduler sets internal `stopping_ = true` and immediately schedules `CloseNode` for all currently active Source Nodes. Subsequently, any Source ProcessNode task that pops from the Executor queue is intercepted: instead of running Process, it schedules Close directly. This provides a clean, rapid shutdown path when a pipeline signals completion from within.

- **Error propagation**: When a Node returns a non-OK, non-Stop status, the Scheduler's task runner invokes the `ErrorCallback`, which:
  1. Sets `HasError()` via `error_callback(error)`.
  2. Sets `stopping_ = true` to prevent further Process scheduling.
  3. Schedules `CloseNode` for all active Nodes to begin orderly teardown.
  4. Calls `HandleIdle()` to detect completion. HandleIdle will **not** Quit until `non_idle_queue_count_ == 0` (pending CloseNode tasks must drain first). [fix A]

- `Shutdown()` initiates orderly teardown: sets `stopping_ = true`, transitions state to `kCancelling`, and calls `HandleIdle()`. No new tasks are accepted (stopping_ prevents further Process scheduling). Tasks already on the Executor are allowed to drain naturally. `HandleIdle` waits until `non_idle_queue_count_ == 0` before calling `Quit() → kTerminated`. `WaitUntilDone()` blocks transparently through this process. MUST be thread-safe for future multi-thread use. [fix B]

- `AddNode()` / `RemoveNode()` return `absl::UnimplementedError()` in Phase 1.

**Event-driven execution flow** (not a central loop — reactions to events):

```
--- OpenNode task runs ---
  Node::Open(context)
  After Open → Scheduler fires OnNodeOpened(node):
    if Source:
      schedule initial ProcessNode  [fix #2: source needs explicit first Process]
      add to active_sources_ set
    if non-source:
      register InputStreamManagers → NotifyPacketArrival if inputs buffered

--- ProcessNode task runs (called by Executor) ---
  [if stopping_=true AND node is Source]
    → schedule CloseNode, skip Process  [fix #3: rapid source teardown]

  // Back-pressure check: Pop side effect  [fix #5]
  InputStreamHandler::FillInputSet(node, context)
    → for each input port: InputStreamManager::Pop()
    → if queue dropped < max_queue_size/2 AND upstream throttled:
      → unthrottle upstream → NotifyPacketArrival(upstream)

  // Execute
  absl::Status status = Node::Process(context)

  // Error handling  [fix #4]
  if (!status.ok() && !IsStopStatus(status)):
    error_callback_(status)
    → HasError() = true, stopping_ = true
    → schedule CloseNode for all active Nodes
    → HandleIdle()
    return

  if (IsStopStatus(status)):
    stopping_ = true
    schedule CloseNode for all active Sources  [fix #3]
    return

  // Output propagation [inline PostProcess — no separate OutputStreamHandler]
  for each output port (from GraphContext::outputs):
    status = OutputStream::Send(packet)
      → calls OutputStream::Send() which writes directly to each mirror:
        last mirror: manager->MovePackets()  (zero-copy)
        prior mirrors: manager->AddPackets() (shared_ptr bump)
      → each AddPackets/MovePackets sets *notify if queue was empty
      → if notify: arrival_callback_ → NotifyPacketArrival(downstream Node)
      → if queue crosses max_queue_size: becomes_full_callback_ → throttle upstream

--- Non-source returns StatusStop ---
  → stopping_ = true
  → schedule CloseNode for all active Sources  [fix #3]

--- Source Node closes ---
  → remove from active_sources_
  [Phase 2: advance source_layer, activate next layer sources]
  → OnNodeClosed → check completion

--- Non-source Node closes ---
  → OnNodeClosed → check completion

--- Graph completion check (called from OnNodeClosed) ---
  if active_sources_.empty()
     AND all non-sources closed
     AND non_idle_queue_count_ == 0:
    → state_ = kTerminated
    → signal WaitUntilDone()
```

**HandleIdle** — triggered from these call sites [fix #7]:
- `Schedule()` after initialization (spawns initial tasks).
- `Resume()` after pause (re-activates scheduling).
- Error callback after recording error (checks if termination is possible).
- `non_idle_queue_count_` reaches 0 (all Executor queues drained).

```
HandleIdle():
  [handling_idle_++, reentrancy guard]
  if handling_idle_ > 1 → return immediately

  if HasError() && non_idle_queue_count_ == 0:  // [fix A]
    Quit() → state_ = kTerminated

  CleanupActiveSources()  // remove closed sources from tracking

  if all sources closed AND all inputs closed AND no pending tasks:
    Quit() → state_ = kTerminated

  if throttled sources exist:
    auto-unthrottle (deadlock prevention)  // may re-schedule ProcessNode
    → re-enter HandleIdle when queues drain again

  if sources active but nothing else to do:
    → wait for events

  [handling_idle_--]
```

**Default implementation strategy** (Phase 1):
- `DefaultInputStreamHandler`: Readiness when ALL input Streams have at least one Packet.
- `ApplicationThreadExecutor`: Tasks are enqueued to `app_thread_tasks_` and drained inside `WaitUntilDone()` on the calling thread.
- No central ready_queue. Task scheduling is distributed via `Executor::ScheduleTask()` callbacks.
- **Idle tracking**: Each Executor queue has a `non_idle_count` — incremented before submitting a task, decremented after the task callback completes. `HandleIdle()` fires when `non_idle_count` reaches 0.
- **HandleIdle reentrancy guard**: Uses `int handling_idle_` counter — incremented on entry, decremented on exit. If >1, returns immediately. Prevents reentrant issues when `state_mutex_` is unlocked/relocked inside `HandleIdle()`.
- **Throttle tracking**: `flat_hash_set<Node*>` for throttled sources. Unthrottle triggered when `InputStreamManager::PopPacketAtTimestamp()` drains queue below `max_queue_size / 2` and fires `becomes_not_full_callback_`.
- `OnNodeOpened()` callback: after Node::Open() completes. For Sources, schedules initial ProcessNode. For non-sources, registers with InputStreamHandler.
- **CloseNode idempotency** [fix C]: `Node::Close()` is idempotent — if the Node is already closed, calling `Close()` again is a no-op and returns `OkStatus()`. This prevents double-close scenarios when `stopping_` fires concurrently with an in-flight ProcessNode intercept.
