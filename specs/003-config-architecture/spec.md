# Feature Specification: Configuration Architecture — IGraphConfigParser & JSON Parser

**Feature Branch**: `003-config-architecture`

**Created**: 2026-07-23

**Status**: Draft

**Input**: "根据 project_bootstrap.md §3.4 Configuration Architecture 设计配置架构规范"

## User Scenarios & Testing

### User Story 1 — Parse Graph Configuration from File (Priority: P1)

As a pipeline developer, I want to define my graph topology in a JSON config file so that I can change pipeline behavior without recompiling.

**Why this priority**: Configuration-driven graph construction is a core architectural principle (FR-002). Without file-based config, pipeline topology is hardcoded.

**Independent Test**: A valid JSON config file is parsed into a `GraphConfig` struct whose nodes, streams, and executors match the file contents exactly.

**Acceptance Scenarios**:

1. **Given** a JSON config file defining a String Pipeline (producer → transformer → consumer), **When** parsed by `IGraphConfigParser`, **Then** the resulting `GraphConfig` contains 3 `NodeDef`s and 2 `StreamDef`s with correct connections.
2. **Given** a JSON config file with an invalid node reference, **When** parsed, **Then** the parser returns a descriptive error indicating the missing node.
3. **Given** a JSON config file with duplicate stream names, **When** parsed, **Then** the parser returns an error indicating the duplicate.

---

### User Story 2 — Pluggable Config Format Support (Priority: P2)

As a framework developer, I want to add a new config format (e.g., YAML, Protobuf) by implementing an interface, so that I don't need to modify the runtime.

**Why this priority**: Important for extensibility (FR-006) but not required for MVP validation.

**Independent Test**: A custom `IGraphConfigParser` implementation parses a non-JSON format and produces a `GraphConfig` identical to the equivalent JSON — without modifying `GraphBuilder` or `GraphRuntime`.

**Acceptance Scenarios**:

1. **Given** a `YamlParser` implementing `IGraphConfigParser`, **When** registered and used to parse a YAML config, **Then** the output `GraphConfig` is identical to the same pipeline defined in JSON.

---

### User Story 3 — Config Validation at Build Time (Priority: P1)

As a pipeline developer, I want invalid configurations to be caught at parse/build time (not at runtime), so that I can fix errors before execution begins.

**Why this priority**: Early error detection is essential for developer productivity.

**Independent Test**: An invalid config (missing node, wrong port name, type mismatch) produces a clear error during `GraphBuilder::Build()`, before `GraphRuntime::Start()` is called.

**Acceptance Scenarios**:

1. **Given** a config referencing a non-existent node in a stream definition, **When** `GraphBuilder::Build()` is called, **Then** it returns `NotFoundError` with the missing node name.
2. **Given** a config where two streams share the same name, **When** parsed, **Then** an error is returned before any runtime state is created.

---

### Edge Cases

- **Empty config**: What happens when a config file has no nodes and no streams? The graph should be valid (zero nodes, zero streams) and `GraphRuntime::Start()` should immediately complete.
- **Self-referencing stream**: A stream that connects a node's output back to its own input — should this be allowed? In Phase 1, self-loops should produce an error.
- **Multiple executors with the same name**: Duplicate executor definitions should produce an error.
- **Unreferenced nodes**: Nodes declared but not connected to any stream should be valid (orphan nodes).
- **Config file not found**: A missing config file should produce a clear `FileNotFoundError`.

## Requirements

### Functional Requirements

- **FR-001**: System MUST provide an `IGraphConfigParser` abstract interface with a pure virtual `Parse(const std::string& file_path) -> GraphConfig` method.
- **FR-002**: System MUST provide a JSON parser (`JsonParser`) implementing `IGraphConfigParser`, capable of parsing the full `GraphConfig` schema (nodes, streams, executors).
- **FR-003**: The JSON parser MUST validate that all node references in stream definitions exist, reporting detailed errors for missing references.
- **FR-004**: The JSON parser MUST validate that stream names are unique, reporting errors for duplicates.
- **FR-005**: `GraphBuilder::Build(config)` MUST accept a `GraphConfig` (programmatic or parsed) and perform validation — unknown node types, port type mismatches, invalid option values.
- **FR-006**: Adding a new config format MUST require only implementing `IGraphConfigParser` — no changes to `GraphBuilder`, `GraphRuntime`, or existing parsers.
- **FR-007**: The parser MUST handle file-not-found errors gracefully, returning a clear error status without crashing.
- **FR-008**: Empty config files (no nodes, no streams) MUST produce a valid empty `GraphConfig` that results in a no-op graph execution.
- **FR-009**: Self-referencing streams (node output → same node input) MUST produce an error in Phase 1.
- **FR-010**: Duplicate executor names MUST produce an error.

### Key Entities

- **IGraphConfigParser**: Abstract interface with `Parse(file_path) -> GraphConfig`. Base class for all config format implementations.
- **JsonParser**: Concrete implementation parsing JSON files into `GraphConfig`. Uses nlohmann/json library.
- **GraphConfig**: Configuration data structure (already implemented). Consumed by `GraphBuilder::Build()`.
- **GraphBuilder**: Validates and constructs runtime state from `GraphConfig`. Already handles programmatic config; extends to parsed config seamlessly.
- **ConfigValidator**: Shared validation logic used by parsers and GraphBuilder — checks node references, duplicate names, type compatibility.
- **ParserRegistry**: Global registry mapping file extensions to parser implementations, enabling auto-detection of format from file extension.

## Success Criteria

### Measurable Outcomes

- **SC-001**: A JSON config file defining a 3-Node String Pipeline can be parsed and executed end-to-end without any programmatic config code — `JsonParser::Parse(file) → GraphBuilder::Build(config) → GraphRuntime::Start()`.
- **SC-002**: All validation errors (missing node, duplicate stream, bad type) are caught during `Parse()` or `Build()` and reported with clear messages — zero runtime failures from invalid configs.
- **SC-003**: A custom parser implementing `IGraphConfigParser` can be written and registered in under 50 lines of code, without modifying runtime internals.
- **SC-004**: Parsing a 50-node config file completes in under 100ms.
- **SC-005**: `bazel test //src/tests:config_parser_test && bazel test //src/tests:integration_test` both pass with config-driven pipeline.

## Assumptions

- JSON is the only Phase 1 config format; YAML and Protobuf are Phase 2.
- Config files are read from the local filesystem only (no HTTP, no remote config).
- Graph topology is static — no runtime config reload in Phase 1.
- Config encoding is UTF-8; other encodings are not supported in Phase 1.
- The `max_queue_size` and `report_deadlock` fields in `GraphConfig` have sensible defaults; only explicitly set values override.
- Config validation is split between parser (schema validation) and GraphBuilder (semantic validation).
