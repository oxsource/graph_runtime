# Contract: Node

**File**: `graph_runtime/src/node/node.h`

```cpp
namespace graph::runtime {

class Node {
 public:
  virtual ~Node() = default;

  const std::string& name() const;

  // Lifecycle
  virtual absl::Status Open(GraphContext& context) = 0;
  virtual absl::Status Process(GraphContext& context) = 0;
  virtual absl::Status Close(GraphContext& context) = 0;

  // Port binding (called by GraphBuilder during initialization)
  void SetInputPort(const std::string& name, Stream* stream);
  void SetOutputPort(const std::string& name, Stream* stream);

  Stream* GetInputPort(const std::string& name) const;
  Stream* GetOutputPort(const std::string& name) const;

  size_t input_port_count() const;
  size_t output_port_count() const;

 protected:
  Node(std::string name);

 private:
  std::string name_;
  std::map<std::string, Stream*> input_ports_;
  std::map<std::string, Stream*> output_ports_;
};

}  // namespace graph::runtime
```

**Semantics**:
- `Open()` called once before any `Process()` calls. Initialize resources, validate options, open external connections.
- `Process()` called when all input Streams have data. Read inputs via `GraphContext::inputs`, write outputs via `GraphContext::outputs`. MUST return promptly (non-blocking).
- `Close()` called once after all `Process()` calls complete. Release resources, flush pending data.
- Port binding is done by GraphBuilder during `Graph::Initialize()` — Nodes do not wire themselves.
- Subclasses implement Open/Process/Close; port management and lifecycle state tracking are handled by the base class.
- Source Nodes (zero inputs) are activated by Scheduler immediately on graph start.
- Sink Nodes (zero outputs) have their Process() result discarded by Scheduler.
