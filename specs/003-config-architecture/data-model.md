# Data Model: Configuration Architecture

## Entities

### IGraphConfigParser

Abstract interface for all config format parsers.

| Method | Return Type | Description |
|--------|-------------|-------------|
| `Parse(file_path)` | `absl::StatusOr<GraphConfig>` | Read and parse a config file into GraphConfig |

**Boundaries**:
- Implementations MUST NOT access GraphRuntime, GraphBuilder, or NodeFactory.
- Implementations MUST return errors via Status (never throw).
- The interface is the sole boundary between config format and runtime.

---

### JsonParser

Concrete implementation parsing JSON config files.

| Property | Description |
|----------|-------------|
| Parser library | nlohmann/json (header-only) |
| Input format | UTF-8 JSON file |
| Output | Fully populated GraphConfig with validated references |
| Error handling | StatusOr with file path + line number in error messages |

**JSON-to-GraphConfig mapping**:

| JSON key | GraphConfig field | Required |
|----------|------------------|----------|
| `max_queue_size` | `GraphConfig::max_queue_size` | No (default 100) |
| `report_deadlock` | `GraphConfig::report_deadlock` | No (default false) |
| `executors[]` | `GraphConfig::executors` | No (default `ThreadPoolExecutor`) |
| `executors[].name` | `ExecutorDef::name` | Yes |
| `executors[].type` | `ExecutorDef::type` | No (default ThreadPoolExecutor) |
| `executors[].num_threads` | `ExecutorDef::num_threads` | No (default 0 = auto) |
| `nodes[]` | `GraphConfig::nodes` | No (empty config valid) |
| `nodes[].name` | `NodeDef::name` | Yes (unique) |
| `nodes[].type` | `NodeDef::type` | Yes (must be registered) |
| `nodes[].input_streams` | `NodeDef::input_streams` | No |
| `nodes[].output_streams` | `NodeDef::output_streams` | No |
| `nodes[].input_side_packets` | `NodeDef::input_side_packets` | No |
| `nodes[].output_side_packets` | `NodeDef::output_side_packets` | No |
| `nodes[].options` | `NodeDef::options` | No |
| `nodes[].executor` | `NodeDef::executor` | No |
| `nodes[].input_stream_handler` | `NodeDef::input_stream_handler` | No |
| `nodes[].max_in_flight` | `NodeDef::max_in_flight` | No (default 1) |
| `nodes[].source_layer` | `NodeDef::source_layer` | No (default 0) |
| `streams[]` | `GraphConfig::streams` | No |
| `streams[].name` | `StreamDef::name` | Yes (unique) |
| `streams[].source_node` | `StreamDef::source_node` | Yes (must exist in nodes) |
| `streams[].source_port` | `StreamDef::source_port` | Yes |
| `streams[].dest_node` | `StreamDef::dest_node` | Yes (must exist in nodes) |
| `streams[].dest_port` | `StreamDef::dest_port` | Yes |
| `input_streams[]` | `GraphConfig::input_streams` | No |
| `output_streams[]` | `GraphConfig::output_streams` | No |

---

### ConfigValidator

Shared validation logic used by parsers and GraphBuilder.

| Method | Description |
|--------|-------------|
| `ValidateNodeRefs(config)` | Check all stream source/dest nodes exist in node list |
| `ValidateUniqueNames(config)` | Check no duplicate node or stream names |
| `ValidateNoSelfLoops(config)` | Reject streams where source_node == dest_node |
| `ValidateUniqueExecutors(config)` | Check no duplicate executor names |
| `Validate(config) -> Status` | Run all validation checks |

---

### ParserRegistry

Global format-to-parser mapping for auto-detection.

| Method | Description |
|--------|-------------|
| `Register(ext, factory)` | Register a parser for a file extension |
| `CreateForFile(path) -> Parser` | Auto-detect format and create parser |
| `IsFormatSupported(path)` | Check if extension is registered |

---

## Relationships

```
Config File (.json)
  │
  ▼
ParserRegistry::CreateForFile(path)
  │
  ▼
JsonParser::Parse(path)              IGraphConfigParser (interface)
  │                                      ▲
  ├─► nlohmann/json parse                │
  ├─► ConfigValidator::Validate()   ─────┤
  │                                      │
  ▼                                      │
GraphConfig  ─────────────────────────────┘
  │
  ▼
GraphBuilder::Build(config)
  ├─► ConfigValidator (re-validate)
  ├─► NodeFactoryRegistry::CreateByName
  └─► GraphRuntime
```
