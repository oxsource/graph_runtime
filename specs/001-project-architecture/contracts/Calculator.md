# Contract: Calculator

**File**: `src/node/calculator.h`

```cpp
namespace graph::runtime {

class CalculatorContext;

class Calculator {
 public:
  virtual ~Calculator() = default;

  virtual absl::Status Open(CalculatorContext& context) = 0;
  virtual absl::Status Process(CalculatorContext& context) = 0;
  virtual absl::Status Close(CalculatorContext& context) = 0;
};

}  // namespace graph::runtime
```

**Semantics**:
- `Open()` called once before any `Process()` calls. Initialize resources here.
- `Process()` called when all input streams have packets available. Read inputs via `context.inputs`, write outputs via `context.outputs`.
- `Close()` called once after all `Process()` calls complete. Release resources here.
- Return `absl::OkStatus()` on success, error status on failure.
