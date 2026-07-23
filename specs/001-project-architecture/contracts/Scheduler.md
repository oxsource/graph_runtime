# Contract: Scheduler

**File**: `src/scheduler/scheduler.h`

```cpp
namespace graph::runtime {

class Graph;

class Scheduler {
 public:
  virtual ~Scheduler() = default;

  virtual absl::Status Schedule(Graph& graph) = 0;
  virtual void Shutdown() = 0;

  // Reserved for Phase 2 dynamic graph support
  virtual absl::Status AddNode(Node* node) { return absl::UnimplementedError(""); }
  virtual absl::Status RemoveNode(Node* node) { return absl::UnimplementedError(""); }
};

}  // namespace graph::runtime
```

**Semantics**:
- `Schedule()` begins executing the graph. Blocks until the graph completes or `Shutdown()` is called.
- `Shutdown()` signals the scheduler to stop processing and return from `Schedule()`.
- `AddNode()` / `RemoveNode()` are no-ops in Phase 1, returning `UnimplementedError`.
