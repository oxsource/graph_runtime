# Contract: JsonParser

**File**: `graph_runtime/src/config/json/json_parser.h`

```cpp
namespace graph::runtime {

// JSON implementation of IGraphConfigParser.
// Uses the nlohmann/json library (header-only) for parsing.
class JsonParser : public IGraphConfigParser {
 public:
  // Create a JsonParser with default validation.
  JsonParser();

  // IGraphConfigParser interface.
  // Parses the JSON file at file_path, validates content, returns GraphConfig.
  // See IGraphConfigParser for error semantics.
  absl::StatusOr<GraphConfig> Parse(const std::string& file_path) override;

 private:
  // Internal: parse JSON into GraphConfig struct
  absl::StatusOr<GraphConfig> ParseJson(const nlohmann::json& root,
                                         const std::string& file_path);

  // Field parsers
  absl::Status ParseExecutors(const nlohmann::json& json,
                               GraphConfig& config);
  absl::Status ParseNodes(const nlohmann::json& json,
                           GraphConfig& config);
  absl::Status ParseStreams(const nlohmann::json& json,
                             GraphConfig& config);
};

}  // namespace graph::runtime
```

**JSON schema** (see data-model.md for full field mapping):

```json
{
  "max_queue_size": 100,
  "report_deadlock": false,
  "executors": [{ "name": "", "type": "ThreadPoolExecutor", "num_threads": 0 }],
  "nodes": [{
    "name": "...",
    "type": "...",
    "input_streams": [],
    "output_streams": [],
    "input_side_packets": [],
    "output_side_packets": [],
    "options": {},
    "executor": "",
    "input_stream_handler": "",
    "max_in_flight": 1,
    "source_layer": 0
  }],
  "streams": [{
    "name": "...",
    "source_node": "...",
    "source_port": "...",
    "dest_node": "...",
    "dest_port": "..."
  }],
  "input_streams": [],
  "output_streams": []
}
```

**Validation performed during Parse**:
1. File existence check — returns `NotFoundError` if file missing.
2. JSON syntax validation — returns `InvalidArgumentError` with nlohmann/json error details.
3. Required field presence — each required field checked with descriptive error.
4. Node reference validation — every stream's `source_node` and `dest_node` must exist in `nodes[]`.
5. Name uniqueness — no duplicate node names, stream names, or executor names.
6. Self-loop detection — `source_node == dest_node` produces an error.
7. Type-forwarding — remaining validation (node type registration, port types) happens in `GraphBuilder::Build()`.
