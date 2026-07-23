# Contract: ConfigValidator

**File**: `graph_runtime/src/config/config_validator.h`

```cpp
namespace graph::runtime {

// Shared validation logic for GraphConfig.
// Used by parsers (during Parse) and GraphBuilder (during Build).
class ConfigValidator {
 public:
  // Run all validation checks. Returns first error encountered.
  static absl::Status Validate(const GraphConfig& config);

  // Individual checks (called by Validate):
  static absl::Status ValidateNodeRefs(const GraphConfig& config);
  static absl::Status ValidateUniqueNames(const GraphConfig& config);
  static absl::Status ValidateNoSelfLoops(const GraphConfig& config);
  static absl::Status ValidateUniqueExecutors(const GraphConfig& config);
};

}  // namespace graph::runtime
```

**Validation rules**:

| Rule | Check | Error |
|------|-------|-------|
| Node reference | Every `StreamDef::source_node` and `dest_node` exists in `NodeDef::name` | `InvalidArgumentError("stream 'X': source node 'Y' not found")` |
| Unique names | No duplicate `NodeDef::name`, `StreamDef::name`, or `ExecutorDef::name` | `InvalidArgumentError("duplicate node name: X")` |
| Self-loop | No stream where `source_node == dest_node` | `InvalidArgumentError("stream 'X': self-loop detected")` |
| Unique executors | No duplicate `ExecutorDef::name` | `InvalidArgumentError("duplicate executor name: X")` |

**Semantics**:
- `Validate()` runs all checks in order and returns the first error found.
- Checks operate purely on the `GraphConfig` struct — no access to NodeFactory or runtime state.
- Called by parsers after constructing GraphConfig from file content.
- Called again by `GraphBuilder::Build()` to ensure programmatic configs also pass validation.
