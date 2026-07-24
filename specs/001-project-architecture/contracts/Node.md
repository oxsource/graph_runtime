# Contract: Node

**File**: `src/framework/node/node.h`

```cpp
namespace graph::runtime {

class Node {
 public:
  virtual ~Node() = default;

  virtual absl::Status Open(GraphContext& context) = 0;
  virtual absl::Status Process(GraphContext& context) = 0;
  virtual absl::Status Close(GraphContext& context) = 0;
};

}  // namespace graph::runtime
```

**Semantics**:
- `Open()` called once before any `Process()` calls. Initialize resources here.
- `Process()` called when all input streams have packets available. Read inputs via `context.Inputs()`, write outputs via `context.Outputs()`.
- `Close()` called once after all `Process()` calls complete. Release resources here.
- Return `absl::OkStatus()` on success, error status on failure.
