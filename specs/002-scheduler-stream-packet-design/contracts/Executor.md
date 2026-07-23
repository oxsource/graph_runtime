# Contract: Executor

**File**: `graph_runtime/src/scheduler/executor.h`

```cpp
namespace graph::runtime {

class Executor {
 public:
  virtual ~Executor() = default;

  // Schedule a task for execution. Must be thread-safe.
  virtual void ScheduleTask(std::function<void()> task) = 0;
};

}  // namespace graph::runtime
```

**Semantics**:
- `ScheduleTask()` accepts a closure and guarantees it will be executed. The execution policy (synchronous, thread pool, dedicated thread, etc.) is determined by the concrete subclass.
- MUST be thread-safe — multiple callers may invoke `ScheduleTask()` concurrently.

**Built-in implementations**:

| Executor | Description | Phase |
|----------|-------------|-------|
| `ApplicationThreadExecutor` | Runs all tasks synchronously on the calling thread (the thread that invoked `Schedule()`). Tasks are queued and drained during `ApplicationThreadAwait()`. **Default for Phase 1.** | Phase 1 |
| `ThreadPoolExecutor` | Distributes tasks across a fixed-size thread pool. `num_threads` is configurable. Defaults to `min(CPUs, node_count)`. | Phase 2 |

**Selection**:
- Set via `Scheduler::SetExecutor()` before `Schedule()`.
- Can be configured per-node via JSON config (`node.executor`), enabling heterogeneous executors (e.g., GPU-bound nodes on a dedicated executor).
