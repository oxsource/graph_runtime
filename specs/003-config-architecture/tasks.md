---

description: "Task list for Configuration Architecture implementation"

---

# Tasks: Configuration Architecture — IGraphConfigParser & JSON Parser

**Input**: Design documents from `/specs/003-config-architecture/`

**Prerequisites**: plan.md (required), spec.md (required for user stories), research.md, data-model.md, contracts/

**Tests**: Test tasks are included per the implementation plan (GoogleTest cc_test).

**Organization**: Tasks grouped by module dependency order, mapped to user stories.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to
- Include exact file paths in descriptions

## Path Conventions

- **Workspace root**: `graph_runtime/`
- **Config module**: `graph_runtime/src/config/`
- **JSON parser**: `graph_runtime/src/config/json/`
- **Contracts reference**: `specs/003-config-architecture/contracts/`
- **Tests**: `graph_runtime/src/tests/`
- **Test data**: `graph_runtime/src/config/json/testdata/`

---

## Module Dependency Graph

```
Phase 2: Foundational
├── IGraphConfigParser (abstract interface)
├── ConfigValidator (validation rules)
│
├── US1: JSON Config Parsing (P1)
│   ├── JsonParser (implementation)
│   ├── JSON testdata files
│   └── GraphRuntime file-based Initialize overload
│
├── US2: Pluggable Formats (P2)
│   ├── ParserRegistry
│   └── Custom parser example
│
└── US3: Build-time Validation (P1)
    └── ConfigValidator integrated into GraphBuilder
```

---

## Phase 1: Setup — Build Infrastructure

**Purpose**: Create directory structure and BUILD.bazel for config module.

- [X] T001 Create directory structure (`graph_runtime/src/config/json/testdata/`)
- [X] T002 Create/update `graph_runtime/src/config/BUILD.bazel` with cc_library targets for `:config` (i_graph_config_parser + config_validator + parser_registry)
- [X] T003 Create `graph_runtime/src/config/json/BUILD.bazel` with cc_library for `:json_parser` depending on `//src/config:config` and `@nlohmann_json//:json`

---

## Phase 2: Foundational — Abstraction Layer & Validation

**Purpose**: Implement the zero-dependency abstract interface and shared validation logic.

- [X] T004 Implement `IGraphConfigParser` abstract interface (`graph_runtime/src/config/i_graph_config_parser.h`): `virtual Parse(file_path) -> StatusOr<GraphConfig>` pure virtual method, virtual destructor
- [X] T005 Implement `ConfigValidator` (`graph_runtime/src/config/config_validator.h/.cc`): `ValidateNodeRefs()`, `ValidateUniqueNames()`, `ValidateNoSelfLoops()`, `ValidateUniqueExecutors()`, `Validate()` running all checks

**Checkpoint**: `//src/config:config` builds successfully. ConfigValidator logic is testable in isolation.

---

## Phase 3: User Story 1 — JSON Config Parsing (Priority: P1) 🎯 MVP

**Goal**: Pipeline developers can define graph topology in JSON config files and execute them without programmatic config code.

**Independent Test**: A JSON config file defining a 3-Node String Pipeline is parsed into a GraphConfig matching the expected structure, then executed end-to-end.

### Sub-phase 3a — JsonParser Implementation

- [X] - [ ] T006 [US1] Implement `JsonParser` (`graph_runtime/src/config/json/json_parser.h/.cc`): implements IGraphConfigParser, parses JSON into GraphConfig using nlohmann/json, validates required fields, calls ConfigValidator
- [X] - [ ] T007 [P] [US1] Create JSON testdata — valid config (`graph_runtime/src/config/json/testdata/string_pipeline.json`): 3-Node pipeline with producer → transformer → consumer, 2 streams
- [X] - [ ] T008 [P] [US1] Create JSON testdata — missing node reference (`graph_runtime/src/config/json/testdata/missing_node.json`): stream references non-existent node
- [X] - [ ] T009 [P] [US1] Create JSON testdata — duplicate stream (`graph_runtime/src/config/json/testdata/duplicate_stream.json`): two streams with same name
- [X] - [ ] T010 [P] [US1] Create JSON testdata — empty config (`graph_runtime/src/config/json/testdata/empty.json`): zero nodes, zero streams (valid)
- [X] - [ ] T011 [P] [US1] Create JSON testdata — self-loop (`graph_runtime/src/config/json/testdata/self_loop.json`): stream connecting node to itself
- [X] - [ ] T012 [P] [US1] Create JSON testdata — duplicate node (`graph_runtime/src/config/json/testdata/duplicate_node.json`): two nodes with same name
- [X] - [ ] T013 [US1] Create config parser test (`graph_runtime/src/tests/config_parser_test.cc`): valid parse, missing node error, duplicate stream error, empty config, self-loop error, file not found error

### Sub-phase 3b — GraphRuntime Integration

- [X] - [ ] T014 [US1] Add file-based `Initialize()` overload to `GraphRuntime` (`graph_runtime/src/public/graph_runtime.h/.cc`): `Initialize(const std::string& file_path)` that auto-detects format and parses config
- [X] - [ ] T015 [US1] Add `ParserRegistry` (`graph_runtime/src/config/parser_registry.h/.cc`): `Register(ext, factory)`, `CreateForFile(path)`, `IsFormatSupported(path)`, `GRAPH_RUNTIME_REGISTER_PARSER` macro
- [X] - [ ] T016 [US1] Update `GraphRuntime::Initialize()` to validate missing side packets against GraphConfig declarations
- [X] - [ ] T017 [US1] Create integration test with JSON config (`graph_runtime/src/tests/integration_test.cc`): load string_pipeline.json → Build → Start → WaitUntilDone

**Checkpoint**: `bazel test //src/tests:config_parser_test && bazel test //src/tests:integration_test` passes with config-driven pipeline.

---

## Phase 4: User Story 3 — Build-Time Validation (Priority: P1)

**Goal**: Invalid configurations are caught at build time with clear error messages.

**Independent Test**: Each invalid config (missing node, duplicate name, self-loop) produces a specific error during `GraphBuilder::Build()`, before any runtime state is created.

- [ ] T018 [US3] Integrate `ConfigValidator` into `GraphBuilder::Build()` (`graph_runtime/src/public/graph_builder.cc`): call `ConfigValidator::Validate()` as first step in Build pipeline
- [ ] T019 [US3] Create build validation test (`graph_runtime/src/tests/build_validation_test.cc`): verify each invalid config produces correct error at Build() time

**Checkpoint**: `bazel test //src/tests:build_validation_test` passes.

---

## Phase 5: User Story 2 — Pluggable Config Format Support (Priority: P2)

**Goal**: Framework developers can add a new config format by implementing IGraphConfigParser and registering it, without modifying Runtime.

**Independent Test**: A custom parser implementing IGraphConfigParser is registered and used to parse a config, producing the same result as JSON.

- [ ] T020 [P] [US2] Add `GRAPH_RUNTIME_REGISTER_PARSER` macro to `parser_registry.h` — file-scope static registration
- [ ] T021 [US2] Create custom parser example (`graph_runtime/src/examples/custom_parser.cc`): implements IGraphConfigParser for key-value format, registered via macro
- [ ] T022 [US2] Create parser extensibility test (`graph_runtime/src/tests/parser_extensibility_test.cc`): register custom parser → parse → verify GraphConfig matches expected

**Checkpoint**: `bazel test //src/tests:parser_extensibility_test` passes.

---

## Phase 6: Polish & Cross-Cutting

**Purpose**: Lock down the abstraction boundary, ensure no JSON headers leak into runtime.

- [ ] T023 Verify nlohmann/json is NOT accessible from any module outside `//src/config/json` — add `-I` restriction or enforce via BUILD visibility
- [ ] T024 Add error message format documentation: error messages should include file path + specific failure (e.g., `"pipeline.json: stream 's1': source node 'missing' not found"`)
- [ ] T025 Validate against quickstart.md: run parse → build → execute end-to-end flow

---

## Dependencies & Execution Order

```
Phase 1: Setup (T001–T003)
    │
Phase 2: Foundational (T004–T005)
    │
    ├──────────────────────────────┐
    │                              │
Phase 3a: JsonParser (T006–T013)   │
    │                              │
Phase 3b: Runtime Integ (T014–T017)│
    │                              │
    ├──────────────┐               │
    │              │               │
Phase 4: Build    │               │
  Validation      │               │
  (T018–T019)     │               │
    │              │               │
    └──────────────┘               │
                   │               │
            Phase 5: Pluggable     │
              Formats (T020–T022)  │
                   │               │
            Phase 6: Polish        │
              (T023–T025) ◄────────┘
```

### Within US1

```
3a JsonParser (T006–T013)
  ├── T006: JsonParser implementation
  ├── T007–T012: Test data files [P — all independent]
  └── T013: config_parser_test

3b Runtime Integration (T014–T017)
  ├── T014: GraphRuntime::Initialize(file)
  ├── T015: ParserRegistry
  ├── T016: Side packet validation
  └── T017: integration_test
```

---

## Parallel Opportunities

| Phase | Parallel Tasks | Rationale |
|-------|---------------|-----------|
| Phase 3a | T007, T008, T009, T010, T011, T012 | All JSON testdata files — independent |
| Phase 3a + 3b | T006 (JsonParser) and T015 (ParserRegistry) | Different files, no dependency |
| Phase 5 | T020, T021 | Macro and example — independent |

---

## Implementation Strategy

### MVP Scope (US1 — JSON Config Parsing, P1)

1. **Phase 1 + 2**: BUILD files + IGraphConfigParser + ConfigValidator
2. **Phase 3a**: JsonParser + testdata + config_parser_test
3. **Phase 3b**: ParserRegistry + GraphRuntime::Initialize(file) + integration_test
4. **STOP and VALIDATE**: `bazel test //src/tests:config_parser_test && bazel test //src/tests:integration_test`

### Incremental Delivery

1. **Phase 1 + 2**: Abstract interface + validation ready
2. **+ US1 (Phase 3)**: JSON parsing from files — config-driven pipeline works
3. **+ US3 (Phase 4)**: Build-time validation catches errors early
4. **+ US2 (Phase 5)**: Pluggable format support for extensibility
5. **+ Phase 6**: Polish, doc, abstraction boundary enforcement

---

## Notes

- [P] tasks = different files, no dependencies
- [Story] label maps task to specific user story
- nlohmann/json is only accessible from `//src/config/json` — never from config/ or public/
- IGraphConfigParser and ConfigValidator live in `//src/config:config` (zero external deps)
- JsonParser lives in `//src/config/json:json_parser` (depends on nlohmann/json)
