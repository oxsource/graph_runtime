# Research: Configuration Architecture Design

## Phase 0 — Unknowns Resolution

### 1. JSON Schema Design

**Decision**: Mirror GraphConfig struct fields directly in JSON key names, using snake_case.

**Rationale**:
- Direct 1:1 mapping between JSON keys and `GraphConfig` field names minimizes transformation logic.
- snake_case matches C++ member naming convention in the codebase.
- Consumers can write JSON configs that visually match the C++ struct layout.

**Schema** (top-level):
```json
{
  "max_queue_size": 100,
  "report_deadlock": false,
  "executors": [
    { "name": "", "type": "ThreadPoolExecutor", "num_threads": 4 }
  ],
  "nodes": [
    {
      "name": "producer",
      "type": "StringProducer",
      "input_streams": [],
      "output_streams": ["output:producer_out"],
      "input_side_packets": [],
      "output_side_packets": [],
      "options": {},
      "executor": "",
      "input_stream_handler": "",
      "max_in_flight": 1,
      "source_layer": 0
    }
  ],
  "streams": [
    {
      "name": "producer_out",
      "source_node": "producer",
      "source_port": "output",
      "dest_node": "transformer",
      "dest_port": "input"
    }
  ],
  "input_streams": [],
  "output_streams": []
}
```

**Alternatives considered**:
- Protobuf JSON mapping: Over-engineered for Phase 1; adds schema compilation step.
- TOML/YAML: Would require additional parser dependencies; deferred to Phase 2.

### 2. nlohmann/json Integration

**Decision**: Wire nlohmann/json via a separate `json/BUILD.bazel` that exposes a `cc_library` with the include path. The `json_parser` target depends on this; no other module imports nlohmann/json.

**Rationale**:
- nlohmann/json is header-only — no compilation needed, just include path configuration.
- Isolating the dependency in `//src/config/json` prevents accidental spread to other modules.
- Existing `third_party/nlohmann_json/BUILD.bazel` already defines the correct `cc_library`.

**Build file** (`json/BUILD.bazel`):
```python
cc_library(
    name = "json_parser",
    srcs = ["json_parser.cc"],
    hdrs = ["json_parser.h"],
    deps = [
        "//src/config:config",
        "@nlohmann_json//:json",
    ],
)
```

### 3. Validation Strategy

**Decision**: Split validation into two phases:
1. **Parser-level validation** (in JsonParser/ConfigValidator): JSON syntax validity, required field presence, duplicate names, node reference existence, self-loop detection.
2. **Builder-level validation** (in GraphBuilder): Node type registration, port type compatibility, option value validation.

**Rationale**:
- Parser-level catches structural errors early (before any runtime state is created).
- Builder-level validates semantics that require access to registered Node types (which GraphBuilder has but parsers don't).
- `ConfigValidator` contains shared validation logic (duplicate detection, reference checking) that both parsers and GraphBuilder can use.

**Alternatives considered**:
- All validation in parser: Would require parsers to know about NodeFactoryRegistry — violates separation of concerns.
- All validation in GraphBuilder: Would make error messages less specific about file location.

### 4. ParserRegistry Design

**Decision**: Simple extension-based registry mapping file extensions to parser factory functions.

**Rationale**:
- `.json` → `JsonParser::Create()` is the Phase 1 default.
- Phase 2: `.yaml` → `YamlParser::Create()`.
- Registry is populated via `GRAPH_RUNTIME_REGISTER_PARSER(extension, factory)` macro, similar to NodeFactoryRegistry pattern.
- `GraphRuntime::Initialize("pipeline.json")` auto-detects format from extension.

**Registry API**:
```cpp
class ParserRegistry {
 public:
  static void Register(const std::string& extension,
                       std::function<std::unique_ptr<IGraphConfigParser>()> factory);
  static std::unique_ptr<IGraphConfigParser> CreateForFile(const std::string& file_path);
  static bool IsFormatSupported(const std::string& file_path);
};
```

### 5. Error Reporting Strategy

**Decision**: Use `absl::Status` with detailed error messages including file path, line number (where available), and the specific validation failure.

**Rationale**:
- nlohmann/json provides exception-free parsing with detailed error positions.
- `ConfigValidator` functions return `absl::Status` with descriptive messages.
- Error example: `"INVALID_ARGUMENT: pipeline.json: stream 'producer_out': source node 'nonexistent' not found in node list"`
- All errors are returned via `StatusOr<GraphConfig>` — no exceptions cross the interface boundary.

## Technology Choices Summary

| Decision | Choice | Rationale |
|----------|--------|-----------|
| JSON library | nlohmann/json (header-only) | Already in deps, widely used |
| Interface pattern | Abstract base class (IGraphConfigParser) | Clean separation, testable with mocks |
| Validation | Two-phase (parser + builder) | Structural errors early, semantic errors at build |
| Format detection | ParserRegistry by extension | Simple, extensible, zero-config |
| Error handling | absl::Status (no exceptions) | Consistent with existing codebase |
