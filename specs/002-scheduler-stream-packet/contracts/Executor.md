# Contract: Executor

**File**: `graph_runtime/src/scheduler/executor.h`

```cpp
namespace graph::runtime {

class TaskQueue {
 public:
  virtual ~TaskQueue() = default;
  // Called by the executor to run the next ready task. Must be invoked
  // exactly once for each AddTask() call.
  virtual void RunNextTask() = 0;
};

class Executor {
 public:
  virtual ~Executor() = default;

  // High-level entry point used by the scheduler. Default implementation
  // wraps task_queue->RunNextTask() in a closure and calls Schedule().
  // Subclasses may override for custom batching or prioritization.
  virtual void AddTask(TaskQueue* task_queue) {
    Schedule([task_queue] { task_queue->RunNextTask(); });
  }

  // Core scheduling primitive: enqueues a closure for execution on this
  // executor's thread(s). MUST be thread-safe.
  virtual void Schedule(std::function<void()> task) = 0;
};

// Global registry for named executor types. Used by GraphBuilder to
// instantiate executor implementations from config.
using ExecutorFactory = std::function<absl::StatusOr<std::shared_ptr<Executor>>(const ExecutorOptions&)>;
void RegisterExecutor(const std::string& type_name, ExecutorFactory factory);

}  // namespace graph::runtime
```

**Semantics**:
- `TaskQueue` is an abstract interface for a queue of ready-to-run tasks. `SchedulerQueue` implements it. The separation decouples task prioritization (SchedulerQueue) from task execution (threading model).
- `AddTask()` is called by the scheduler when a task is ready. The default implementation wraps `RunNextTask()` in a lambda via `Schedule()`. Each `AddTask()` call must result in exactly one `RunNextTask()` invocation.
- `Schedule()` is the pure-virtual primitive. Subclasses execute the closure on their thread(s) — synchronously, on a thread pool, or on a dedicated thread.
- MUST be thread-safe — `Schedule()` and `AddTask()` may be called from any thread.

**Executor Registry**:
- `RegisterExecutor("ThreadPoolExecutor", factory)` registers a type for instantiation by name.
- GraphBuilder calls `ExecutorRegistry::CreateByNameInNamespace(type, options)` during graph initialization.
- Custom executor types can be added via registration without modifying framework code.

**Built-in implementations**:

| Executor | Description | Threads |
|----------|-------------|---------|
| `ApplicationThreadExecutor` | Runs all tasks synchronously on the calling thread (via `DelegatingExecutor`). Tasks are enqueued to `app_thread_tasks_` and drained in `WaitUntilDone()`. | 0 (uses app thread) |
| `ThreadPoolExecutor` | Distributes tasks across a fixed-size thread pool. `num_threads` is configurable. Defaults to `min(CPUs, node_count)`. | N |

**Per-node executor assignment**:
- Nodes declare their executor affinity via `Node::SetExecutorName("name")`.
- Empty string (default) → `default_queue_` → default executor.
- Non-empty string → looked up in `non_default_queues_` → named executor.
