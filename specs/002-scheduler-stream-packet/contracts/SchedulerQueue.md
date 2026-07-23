# Contract: SchedulerQueue

**File**: `graph_runtime/src/scheduler/scheduler_queue.h`

```cpp
namespace graph::runtime {

class Node;
class GraphContext;

class SchedulerQueue : public TaskQueue {
 public:
  using IdleCallback = std::function<void(bool idle)>;

  explicit SchedulerQueue(std::string name);

  const std::string& name() const;

  // Bind to an executor (called during setup).
  void SetExecutor(Executor* executor);

  // Idle notification (called during setup, links to Scheduler).
  void SetIdleCallback(IdleCallback cb);

  // Lifecycle.
  void SetRunning(bool running);
  void Reset();
  void CleanupAfterRun();

  // Add a node to the queue for ProcessNode execution.
  void AddNode(Node* node);

  // Add a node to the queue for OpenNode execution.
  void AddNodeForOpen(Node* node);

  // TaskQueue interface — pops the highest-priority node and runs it.
  void RunNextTask() override;

  // State.
  bool IsIdle() const;  // queue_.empty() && num_pending_tasks_ == 0
  int NumPendingTasks() const;

 private:
  struct Item {
    Node* node;
    bool is_open_node;     // true → run Open(), false → run Process()
    int source_layer;      // for source ordering (0 for non-sources)
    int64_t node_id;       // tiebreaker
    // operator< defines priority: OpenNode > non-source > source
    bool operator<(const Item& other) const;
  };

  void RunNode(Node* node, bool is_open);

  std::string name_;
  Executor* executor_ = nullptr;
  IdleCallback idle_callback_;
  bool running_ = false;

  std::priority_queue<Item> queue_;
  int num_pending_tasks_ = 0;
};

}  // namespace graph::runtime
```

**Semantics**:
- One `SchedulerQueue` per executor. The default executor has a default queue; each non-default executor has its own named queue.
- `SetExecutor()` binds the queue to an executor. All tasks added via `AddNode()` are dispatched via `executor_->AddTask(this)`.
- `SetIdleCallback()` registers a callback that fires when `IsIdle()` changes. The Scheduler uses this to aggregate `non_idle_queue_count_` across all queues.
- `AddNode()` pushes an `Item` onto the priority queue and, if running, calls `executor_->AddTask(this)` to submit work to the executor.
- `AddNodeForOpen()` pushes an `Item` with `is_open_node = true`, giving it highest priority.
- `RunNextTask()` pops the highest-priority `Item`, calls `RunNode()` (which dispatches to `Node::Open()` or `Node::Process()`), then decrements `num_pending_tasks_`. If now idle, fires `idle_callback_(true)`.
- **Priority ordering** (`operator<`):
  1. `OpenNode` tasks (highest, run before any ProcessNode).
  2. Non-sources (scheduled before sources).
  3. Sources (ordered by source_layer → node_id).

**Task dispatch path**:
```
Node becomes ready
  → node->GetSchedulerQueue()->AddNode(node)
    → queue_.push(Item)
    → executor_->AddTask(this)
      → Schedule([this]{ RunNextTask(); })
        → executor thread: RunNextTask()
          → pop Item → RunNode()
          → if idle: idle_callback_(true)
```
