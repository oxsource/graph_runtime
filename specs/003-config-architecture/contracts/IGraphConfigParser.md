# Contract: IGraphConfigParser

**File**: `graph_runtime/src/config/i_graph_config_parser.h`

```cpp
namespace graph::runtime {

// Abstract interface for all config format parsers.
// Each config format (JSON, YAML, Protobuf, etc.) implements this interface.
class IGraphConfigParser {
 public:
  virtual ~IGraphConfigParser() = default;

  // Parse a config file at the given path into a GraphConfig.
  // Returns:
  //   - OkStatus + valid GraphConfig on success
  //   - NotFoundError if file does not exist
  //   - InvalidArgumentError if config content is invalid
  //   - InternalError for unexpected parse failures
  //
  // Implementations MUST NOT throw exceptions across this boundary.
  // Error messages MUST include the file path for context.
  virtual absl::StatusOr<GraphConfig> Parse(const std::string& file_path) = 0;
};

}  // namespace graph::runtime
```

**Semantics**:
- `Parse(file_path)` reads the file at `file_path`, validates the content, and returns a populated `GraphConfig`.
- The interface is the **sole boundary** between config format and runtime. `GraphBuilder` and `GraphRuntime` never see the parser.
- Implementations MUST NOT depend on `GraphRuntime`, `GraphBuilder`, `NodeFactoryRegistry`, or any runtime module — they operate purely on the file→GraphConfig transformation.
- Error messages should be descriptive: include the file path, line/position information where available, and the specific validation failure.
- Thread safety: `Parse()` is called once during graph initialization. No concurrent calls are expected.
