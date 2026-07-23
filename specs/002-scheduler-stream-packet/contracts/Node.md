# Contract: Node

**File**: `graph_runtime/src/node/node.h`

```cpp
namespace graph::runtime {

class SchedulerQueue;

class Node {
 public:
  virtual ~Node() = default;

  const std::string& name() const;

  // REQUIRED static method (enforced by static_assert in NodeFactoryFor<T>):
  //   static absl::Status GetContract(NodeContract* contract);
  // Declares input/output port types. Called at graph construction time.

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

  // Source priority for scheduling order within the same layer
  virtual Timestamp SourceProcessOrder(const GraphContext& context) const;

 protected:
  Node(std::string name);

 private:
  std::string name_;
  std::map<std::string, InputStreamManager*> input_ports_;
  std::map<std::string, OutputStream*> output_ports_;
  std::string executor_name_;
  SchedulerQueue* scheduler_queue_ = nullptr;
  int source_layer_ = 0;
};

}  // namespace graph::runtime
```

**Semantics**:
- `GetContract()` is a **required static method** on every Node subclass. It declares port types, enabling compile-time port type validation during graph construction. Enforced by `static_assert` in `NodeFactoryFor<T>`.
- `Open()` called once before any `Process()` calls. Initialize resources, validate options, open external connections.
- `Process()` called when all input Streams have data. Read inputs via `GraphContext::inputs`, write outputs via `GraphContext::outputs`. MUST return promptly (non-blocking).
- `Close()` called once after all `Process()` calls complete. Release resources, flush pending data. Idempotent.
- Port binding is done by GraphBuilder during `Graph::Initialize()` — Nodes do not wire themselves.
- `SetExecutorName()` / `ExecutorName()`: declares which named executor this node should run on. Empty string = default executor. Set from `config.node.executor`.
- `SetSchedulerQueue()` / `GetSchedulerQueue()`: set by `Scheduler::AssignNodeToQueue()` during `Schedule()`. All task scheduling goes through this queue.
- `SourceProcessOrder()` controls the order source nodes are scheduled within the same layer. Lower timestamp = higher priority. Default returns `Timestamp::Min()`.
