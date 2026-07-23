# Contract: IGraphConfigParser

**File**: `src/config/i_graph_config_parser.h`

```cpp
namespace graph::runtime {

class IGraphConfigParser {
 public:
  virtual ~IGraphConfigParser() = default;

  virtual GraphConfig Parse(const std::string& file_path) = 0;
};

}  // namespace graph::runtime
```

**Semantics**:
- `Parse()` reads the file at `file_path`, parses it into a `GraphConfig` struct.
- Throws `std::runtime_error` on parse failure.
- Caller owns the returned `GraphConfig`.
