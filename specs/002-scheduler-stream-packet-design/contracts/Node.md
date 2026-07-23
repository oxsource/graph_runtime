# Contract: Node

**File**: `graph_runtime/src/node/node.h`

```cpp
namespace graph::runtime {

class SchedulerQueue;

class Node {
 public:
  virtual ~Node() = default;

  const std::string& name() const;

  // Lifecycle
  virtual absl::Status Open(GraphContext& context) = 0;
  virtual absl::Status Process(GraphContext& context) = 0;
  virtual absl::Status Close(GraphContext& context) = 0;

  // Port binding (called by GraphBuilder during initialization)
  void SetInputPort(const std::string& name, InputStreamManager* mgr);
  void SetOutputPort(const std::string& name, OutputStream* stream);

  InputStreamManager* GetInputPort(const std::string& name) const;
  OutputStream* GetOutputPort(const std::string& name) const;

  size_t input_port_count() const;
  size_t output_port_count() const;

  // Executor assignment (called by GraphBuilder from config)
  void SetExecutorName(const std::string& executor_name);
  const std::string& ExecutorName() const;

  // Scheduler queue assignment (called by Scheduler during Setup)
  void SetSchedulerQueue(SchedulerQueue* queue);
  SchedulerQueue* GetSchedulerQueue() const;

  // Source ordering
  void SetSourceLayer(int layer);
  int SourceLayer() const;

 protected:
  Node(std::string name);

 private:
  std::string name_;
  std::map<std::string, InputStreamManager*> input_ports_;
  std::map<std::string, OutputStream*> output_ports_;
  std::string executor_name_;       // "" = default executor
  SchedulerQueue* scheduler_queue_ = nullptr;
  int source_layer_ = 0;
};

}  // namespace graph::runtime
```

**Semantics**:
- Same lifecycle (`Open`/`Process`/`Close`) as before.
- Port binding updated: input ports bind to `InputStreamManager*` (owns the deque), output ports bind to `OutputStream*` (holds mirrors). No intermediate `Stream` class.
- `SetExecutorName()` / `ExecutorName()`: declares which named executor this node should run on. Empty string = default executor. Set from `config.node.executor` by GraphBuilder.
- `SetSchedulerQueue()` / `GetSchedulerQueue()`: set by `Scheduler::AssignNodeToQueue()` during `Schedule()`. All task scheduling for this node goes through this queue.
- `SetSourceLayer()` / `SourceLayer()`: controls source activation order (Phase 2). Phase 1: all sources are layer 0.
