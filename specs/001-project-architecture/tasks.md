---

description: "Task list for Graph Runtime project architecture implementation"

---

# Tasks: Project Architecture Design — Graph Runtime

**Input**: Design documents from `/specs/001-project-architecture/`

**Prerequisites**: plan.md (required), spec.md (required for user stories), research.md, data-model.md, contracts/

**Tests**: Test tasks are included per the project plan (GoogleTest cc_test per module + integration test).

**Organization**: Tasks grouped by module dependency order, mapped to user stories for independent verification.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

## Path Conventions

- **Workspace root** (run `bazel` from here): `graph_runtime/`
- **Project root files**: `graph_runtime/WORKSPACE`, `graph_runtime/BUILD.bazel`, `graph_runtime/.bazelversion`, `graph_runtime/.bazelrc`, `graph_runtime/graph_runtime_deps.bzl`
- **Public API**: `graph_runtime/src/public/include/graph_runtime/`
- **Internal modules**: `graph_runtime/src/config/`, `graph_runtime/src/graph/`, `graph_runtime/src/node/`, `graph_runtime/src/scheduler/`, `graph_runtime/src/stream/`, `graph_runtime/src/runtime/`
- **Examples**: `graph_runtime/src/examples/`
- **Tests**: `graph_runtime/src/tests/`

---

## Module Dependency Graph

Bazel BUILD target dependency order (each must be buildable before dependents):

```
//src/stream:stream          (Packet, Stream — no internal deps)
//src/config:config          (GraphConfig, IGraphConfigParser — no internal deps)
//src/config/json:json       (JsonParser — depends on //src/config:config)
//src/node:node              (Node with Open/Process/Close lifecycle, GraphContext, NodeFactory — depends on //src/stream:stream)
//src/graph:graph            (Graph, GraphBuilder — depends on //src/config:config, //src/node:node, //src/stream:stream)
//src/scheduler:scheduler    (Scheduler — depends on //src/graph:graph, //src/node:node)
//src/runtime:runtime        (Runtime — depends on //src/graph:graph, //src/scheduler:scheduler)
//src/public:runtime         (Umbrella aggregator — depends on all internal modules)
//src/examples:string_pipeline  (MVP example — depends on runtime + config)
//src/tests/*                (Verification — depends on respective modules)
```

Mapping to user stories:

| Module | US1 (Bazel Lib) | US2 (Config-Driven) | US3 (String MVP) | US4 (Extensible) |
|--------|:-:|:-:|:-:|:-:|
| stream/ (Packet, Stream) | ✓ | ✓ | ✓ | ✓ |
| config/ (GraphConfig, Parser interface) | ✓ | ✓ | ✓ | ✓ |
| config/json/ (JsonParser) | | ✓ | ✓ | |
| node/ (Node, GraphContext, NodeFactory) | ✓ | | ✓ | ✓ |
| graph/ (Graph, GraphBuilder) | ✓ | ✓ | ✓ | |
| scheduler/ | ✓ | | ✓ | ✓ |
| runtime/ | ✓ | | ✓ | |
| public/ (umbrella, export) | ✓ | ✓ | ✓ | ✓ |

---

## Phase 1: Setup — Bazel Project Scaffolding

**Purpose**: Initialize Bazel workspace, platform definitions, external dependency bootstrap. Establish the build foundation before any module design begins.

- [X] T001 Create project directory structure per plan.md (`graph_runtime/src/public/include/graph_runtime/`, `graph_runtime/src/config/json/`, `graph_runtime/src/graph/`, `graph_runtime/src/node/`, `graph_runtime/src/scheduler/`, `graph_runtime/src/stream/`, `graph_runtime/src/runtime/`, `graph_runtime/src/examples/`, `graph_runtime/src/tests/`, `graph_runtime/platforms/`, `graph_runtime/third_party/nlohmann_json/`)
- [X] T002 [P] Initialize Bazel workspace (`graph_runtime/WORKSPACE` with `workspace(name = "graph_runtime")`, `graph_runtime/.bazelversion` with `6.5.0`, `graph_runtime/.bazelrc` with platform config aliases)
- [X] T003 [P] Create root `graph_runtime/BUILD.bazel` with `//:runtime` alias to `//src/public:runtime` and platform `config_setting` entries
- [X] T004 [P] Create `graph_runtime/platforms/` directory (`graph_runtime/platforms/BUILD` with `config_setting_and_platform` macro for `macos_arm64` and `linux_x86_64`, `graph_runtime/platforms/platforms.bzl` with `graph_runtime_select()`)
- [X] T005 [P] Create `graph_runtime/graph_runtime_deps.bzl` with `graph_runtime_setup()` macro pulling nlohmann/json, abseil-cpp, and googletest via `http_archive`

---

## Phase 2: Foundational — Core Module Interface & Contract Design

**Purpose**: Define the architectural boundaries, interfaces, and collaboration contracts between all core modules. This phase produces the interface definitions (`.h` files) and design artifacts that all user story implementations will build against. No implementation (`.cc` files) is produced here — only contracts.

**⚠️ CRITICAL**: This phase MUST be complete before any user story implementation begins. It establishes the shared vocabulary and module boundaries.

### Step 1 — Scheduling Collaboration Model (Scheduler ↔ Node ↔ Stream ↔ Packet)

Define the execution flow contracts: how the Scheduler drives Node execution, how Nodes read/write Packets via Streams, and the data flow lifecycle.

- [ ] T006 Define `Packet` interface contract (`graph_runtime/src/public/include/graph_runtime/packet.h`): type-erased `std::any` payload, `int64_t` timestamp, `is_empty` marker. Reference: `data-model.md` Packet entity, `contracts/PublicAPI.md`
- [ ] T007 Define `Stream` interface contract (`graph_runtime/src/stream/stream.h`): bounded queue with `Push(Packet)`, `Pop() -> Packet`, `IsClosed()`, max queue size for back-pressure. Reference: `data-model.md` Stream entity
- [ ] T008 Define `Node` interface with embedded lifecycle (`graph_runtime/src/node/node.h`): virtual `Open(GraphContext&) -> absl::Status`, `Process(GraphContext&) -> absl::Status`, `Close(GraphContext&) -> absl::Status`. Name and input/output stream references. `Created → Opened → Processing → Closed` state machine. Reference: `data-model.md` Node entity
- [ ] T009 Define `GraphContext` (`graph_runtime/src/node/graph_context.h`): per-invocation context with `inputs: map<string, Packet&>`, `outputs: map<string, PacketProducer>`, `options: NodeOptions`. Reference: `data-model.md` Node execution model
- [ ] T010 Define `Scheduler` interface (`graph_runtime/src/scheduler/scheduler.h`): `Schedule(Graph&) -> absl::Status`, `Shutdown()`, Phase-2 stubs `AddNode(Node*)`/`RemoveNode(Node*)`. Reference: `contracts/Scheduler.md`, `data-model.md` Scheduler entity
- [ ] T011 Define `Graph` interface (`graph_runtime/src/graph/graph.h`): `Initialize(const GraphConfig&)`, `Start()`, `WaitUntilDone()`, `Shutdown()`, owns Node/Stream/Scheduler. Reference: `data-model.md` Graph entity
- [ ] T012 Define `NodeFactory` contract (`graph_runtime/src/node/node_factory.h`): `Create(const std::string& type_name) -> unique_ptr<Node>`, registration pattern for node types by name from config

**Checkpoint (Step 1)**: All scheduling collaboration interfaces are defined. The data flow sequence `Scheduler → Node → Stream → Packet` is fully specified.

### Step 2 — Config Parsing Module & Data Structures

Define the configuration-driven pipeline: how JSON configs are parsed into `GraphConfig`, how `GraphBuilder` consumes `GraphConfig` to produce a `Graph`, and the validation rules.

- [ ] T013 Define `GraphConfig` data structures (`graph_runtime/src/config/graph_config.h`): `NodeDef` (name, type, options), `StreamDef` (source_node, source_port, dest_node, dest_port), `GraphConfig` (nodes, streams, version), validation rules (no duplicate names, all refs valid). Reference: `data-model.md` GraphConfig entity
- [ ] T014 Define `IGraphConfigParser` interface (`graph_runtime/src/config/i_graph_config_parser.h`): pure virtual `Parse(const string& file_path) -> GraphConfig`, throws `std::runtime_error` on failure. Reference: `contracts/IGraphConfigParser.md`
- [ ] T015 Define `GraphBuilder` interface (`graph_runtime/src/graph/graph_builder.h`): `Build(const GraphConfig&) -> std::unique_ptr<Graph>`, creates and wires Node/Stream instances from config. Reference: `data-model.md` relationships

**Checkpoint (Step 2)**: Config parsing contracts are defined. `GraphConfig → IGraphConfigParser → GraphBuilder → Graph` chain is fully specified.

### Step 3 — Public API Surface & BUILD Scaffolding

Define how consumers interact with the library: export visibility macro, type system, and umbrella header layout.

- [ ] T016 Define `GRAPH_RUNTIME_API` export macro (`graph_runtime/src/public/include/graph_runtime/graph_runtime_export.h`): `-fvisibility=hidden` + `__attribute__((visibility("default")))` on non-Windows, `__declspec` on Windows. Reference: `contracts/PublicAPI.md`, `research.md`
- [ ] T017 Define `ErrorCode` enum and base types (`graph_runtime/src/public/include/graph_runtime/types.h`): `kOk`, `kFileNotFound`, `kParseError`, `kGraphError`, `kRuntimeError`. Reference: `contracts/PublicAPI.md`
- [ ] T018 Define umbrella header layout (`graph_runtime/src/public/include/graph_runtime/graph_runtime.h`): includes export, types, graph, packet, node headers. Define `graph_runtime/src/public/graph_runtime_init.cc` with shared-library init function. Reference: `plan.md` project structure, `contracts/PublicAPI.md`
- [ ] T019 [P] Create stub `BUILD.bazel` for each module (`graph_runtime/src/config/BUILD.bazel`, `graph_runtime/src/stream/BUILD.bazel`, `graph_runtime/src/node/BUILD.bazel`, `graph_runtime/src/graph/BUILD.bazel`, `graph_runtime/src/scheduler/BUILD.bazel`, `graph_runtime/src/runtime/BUILD.bazel`, `graph_runtime/src/examples/BUILD.bazel`, `graph_runtime/src/tests/BUILD.bazel`) referencing their respective interface headers
- [ ] T020 Create `graph_runtime/src/public/BUILD` with `cc_library(name = "runtime")` aggregating all module targets and `cc_binary(name = "runtime_shared", linkshared = True, linkstatic = True, alwayslink = 1)`

**Checkpoint (Phase 2 complete)**: All module interfaces and contracts are defined, BUILD targets exist. Foundation is ready — user story implementations can begin.

---

## Phase 3: User Story 1 — Graph Runtime Library as Bazel Dependency (Priority: P1) 🎯 MVP

**Goal**: External Bazel projects can depend on `@graph_runtime//src/public:runtime` and build C++ code using the Graph Runtime API.

**Independent Test**: `bazel build //src/public:runtime` succeeds on macOS ARM64 and Linux x86_64; an external project with `@graph_runtime` in `graph_runtime/WORKSPACE` and `deps = ["@graph_runtime//src/public:runtime"]` builds and links successfully.

- [ ] T021 [P] [US1] Wire all module `graph_runtime/BUILD.bazel` `deps` correctly so `bazel build //src/public:runtime` resolves all transitive dependencies
- [ ] T022 [US1] Verify `bazel build //src/...` succeeds; fix any missing `cc_library` deps or header visibility issues

**Checkpoint**: `bazel build //src/public:runtime` succeeds. Library is consumable as a Bazel dependency.

---

## Phase 4: User Story 2 — Configuration-Driven Graph Construction (Priority: P1)

**Goal**: Pipeline developers can define graph topology in JSON config files and have the runtime construct the corresponding graph without recompiling.

**Independent Test**: Two different JSON configs produce two different `Graph` instances with different node topologies (verified via `config_parser_test.cc`).

- [ ] T023 [US2] Implement `GraphConfig` validation logic (`graph_runtime/src/config/graph_config.cc`): duplicate node detection, missing reference checks, schema version enforcement
- [ ] T024 [US2] Implement `JsonParser` module (`graph_runtime/src/config/json/json_parser.h/.cc`): parse JSON into `GraphConfig` using nlohmann/json, handle file-not-found and parse errors. `graph_runtime/src/config/json/BUILD.bazel` with nlohmann/json `deps`
- [ ] T025 [US2] Implement `GraphBuilder` (`graph_runtime/src/graph/graph_builder.cc`): `Build(const GraphConfig&)` creates `Node` and `Stream` instances, wires node ports via stream references, returns `unique_ptr<Graph>`. `graph_runtime/src/graph/graph.cc`: `Initialize()`, `Start()`, `WaitUntilDone()`, `Shutdown()` implementations
- [ ] T026 [US2] Create `config_parser_test.cc` in `graph_runtime/src/tests/` with GoogleTest cases for valid JSON, invalid JSON, duplicate nodes, missing references

**Checkpoint**: `bazel test //src/tests:config_parser_test` passes. Graph construction from JSON works.

---

## Phase 5: User Story 3 — String Pipeline MVP (Priority: P1)

**Goal**: Developers can `bazel run //src/examples:string_pipeline` to validate the full framework end-to-end — config parsing, graph building, stream scheduling, and node execution.

**Independent Test**: `bazel run //src/examples:string_pipeline` processes input strings through the graph and produces expected output.

- [ ] T027 [P] [US3] Implement `Packet` internal (`graph_runtime/src/stream/packet.h/.cc`): `std::any` payload storage, timestamp management, empty-packet semantics for stream boundaries
- [ ] T028 [P] [US3] Implement `Stream` (`graph_runtime/src/stream/stream.cc`): bounded `std::queue<Packet>`, `Push()` with back-pressure check, `Pop()` blocking/non-blocking, `Close()` lifecycle
- [ ] T029 [P] [US3] Implement `GraphContext` (`graph_runtime/src/node/graph_context.h/.cc`): input packet accessors, output packet producers, options storage
- [ ] T030 [P] [US3] Implement `Node` (`graph_runtime/src/node/node.cc`): stream registration, `Open()`→`Process()`→`Close()` lifecycle dispatch, input-ready checking (all input streams have data)
- [ ] T031 [P] [US3] Implement `NodeFactory` (`graph_runtime/src/node/node_factory.cc`): type-name to `Node` subclass mapping, registration API, built-in node type registration
- [ ] T032 [US3] Implement `Scheduler` (`graph_runtime/src/scheduler/scheduler.cc`): topological-layer ordering, single-threaded event loop, marks node ready when all inputs available, drives `Node::Process()` in order
- [ ] T033 [US3] Implement `Runtime` (`graph_runtime/src/runtime/runtime.cc`): `Run(GraphConfig)` workflow — parse config → build graph → initialize scheduler → start → wait → shutdown
- [ ] T034 [P] [US3] Create String Pipeline example (`graph_runtime/src/examples/string_pipeline.cc`): producer `Node` subclass (generates strings), transformer `Node` subclass (uppercase), consumer `Node` subclass (collects output), JSON config file for the pipeline. `graph_runtime/src/examples/BUILD.bazel` as `cc_binary`
- [ ] T035 [US3] Create verification tests (`graph_runtime/src/tests/graph_builder_test.cc`, `graph_runtime/src/tests/scheduler_test.cc`, `graph_runtime/src/tests/integration_test.cc` with end-to-end pipeline test)

**Checkpoint**: `bazel run //src/examples:string_pipeline` produces correct output. All tests pass.

---

## Phase 6: User Story 4 — Extensible Module Interfaces (Priority: P2)

**Goal**: Framework developers can replace the default Scheduler or ConfigParser with custom implementations without modifying runtime code.

**Independent Test**: A custom scheduler implementing the `Scheduler` interface is plugged into the runtime and used to drive node execution instead of the default.

- [ ] T036 [P] [US4] Create custom scheduler example (`graph_runtime/src/examples/custom_scheduler.h/.cc` implementing alternative scheduling strategy)
- [ ] T037 [P] [US4] Create custom config parser example (`graph_runtime/src/examples/custom_parser.h/.cc` implementing `IGraphConfigParser` for an alternative format)
- [ ] T038 [US4] Create interface substitution tests verifying custom scheduler and custom parser integration without runtime modification

**Checkpoint**: Custom scheduler and parser work as drop-in replacements. All tests pass.

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Documentation, build hardening, platform validation, and final verification.

- [ ] T039 [P] Add code cleanup, header include guards consistency, `-fvisibility=hidden` enforcement across all `cc_library` targets, and run `bazel build //src/...` on both platforms
- [ ] T040 Validate against `quickstart.md` — run build, test, and example commands; verify end-to-end workflow matches documented steps

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — can start immediately
- **Foundational (Phase 2)**: Depends on Setup — BLOCKS all user stories. Must complete Steps 1→2→3 in order
- **US1 — Bazel Library (Phase 3)**: Depends on Foundational Phase 2 Step 3 (T016–T020: export macro, BUILD targets)
- **US2 — Config-Driven (Phase 4)**: Depends on Foundational Phase 2 Step 2 (T013–T015: GraphConfig, IGraphConfigParser, GraphBuilder interfaces)
- **US3 — String MVP (Phase 5)**: Depends on Foundational Phase 2 Step 1 (T006–T012: Packet, Stream, Node, GraphContext, NodeFactory, Scheduler, Graph interfaces) AND Step 2 (config parsing interfaces) AND Phase 4 (JsonParser + GraphBuilder implementation)
- **US4 — Extensible (Phase 6)**: Depends on Foundational Phase 2 Step 1 + Phase 5 (needs full runtime for interface substitution)
- **Polish (Phase 7)**: Depends on all desired user stories being complete

### Foundational Phase — Sequential Steps

```
Phase 1: Setup
    │
Phase 2: Foundational (DESIGN PHASE — interfaces only)
    │
    ├── Step 1: Scheduling Collaboration (T006–T012)
    │   └── Scheduler ↔ Node ↔ Stream ↔ Packet contracts
    │
    ├── Step 2: Config Parsing Module (T013–T015)
    │   └── GraphConfig ↔ IGraphConfigParser ↔ GraphBuilder
    │
    └── Step 3: Public API Surface + BUILD (T016–T020)
        └── Export macro, types, umbrella header, BUILD targets
            │
            ├──────────────────────────────────┐
            │                                  │
            ▼                                  ▼
        Phase 3: US1                       Phase 4: US2
        (Bazel Library —                   (Config-Driven —
         depends on Step 3)                 depends on Step 2)
            │                                  │
            └──────────┬───────────────────────┘
                       │
                       ▼
                  Phase 5: US3
                  (String Pipeline MVP —
                   depends on Steps 1+2 + Phase 4)
                       │
                       ▼
                  Phase 6: US4 (P2)
                  (Extensible Interfaces)
                       │
                       ▼
                  Phase 7: Polish
```

### Within Each User Story

- Module `BUILD.bazel` files before implementation
- Implementation builds against interfaces defined in Phase 2
- Core implementation before examples
- Story complete before moving to next priority

---

## Parallel Opportunities

| Phase | Parallel Tasks | Rationale |
|-------|---------------|-----------|
| Phase 1 | T002, T003, T004, T005 | WORKSPACE, root BUILD, platforms, deps — all independent |
| Phase 2 Step 1 | T006, T007 | Packet, Stream — all independent interfaces |
| Phase 2 Step 1 | T008, T009 | Node, GraphContext — independent interfaces |
| Phase 2 Step 1 | T010, T011 | Scheduler, Graph — independent interfaces |
| Phase 2 Step 1 | T012 | NodeFactory — independent of T008–T011 |
| Phase 2 Step 2 | T013, T014 | GraphConfig struct and IGraphConfigParser — independent |
| Phase 2 Step 3 | T016, T017, T018 | Export macro, types, umbrella header — all independent |
| Phase 2 Step 3 | T019, T020 | BUILD files and target aggregation — independent of T016–T018 |
| Phase 4 | T023, T024 | Config validation independent of JsonParser |
| Phase 5 | T027, T028, T029 | Packet, Stream, GraphContext implementations — different files |
| Phase 5 | T030, T031 | Node and NodeFactory — independent implementations |
| Phase 5 | T034, T035 | Example binary independent of test files |

### Cross-Story Parallel Execution

Once Foundational Phase 2 completes:
- **US1 (Phase 3)** and **US2 (Phase 4)** can proceed in **parallel** — US1 depends on Step 3 only, US2 depends on Step 2 only
- **US3 (Phase 5)** must wait for US2 (needs JsonParser + GraphBuilder implementation)

---

## Parallel Example: Foundational Phase 2

```bash
# Step 1 — all scheduling collaboration interfaces in parallel:
Task: "Define Packet interface contract in graph_runtime/src/public/include/graph_runtime/packet.h"
Task: "Define Stream interface contract in graph_runtime/src/stream/stream.h"
Task: "Define Node interface with lifecycle in graph_runtime/src/node/node.h"
Task: "Define GraphContext in graph_runtime/src/node/graph_context.h"
Task: "Define Scheduler interface in graph_runtime/src/scheduler/scheduler.h"
Task: "Define Graph interface in graph_runtime/src/graph/graph.h"

# Step 2 — config parsing interfaces in parallel:
Task: "Define GraphConfig data structures in graph_runtime/src/config/graph_config.h"
Task: "Define IGraphConfigParser interface in graph_runtime/src/config/i_graph_config_parser.h"
Task: "Define GraphBuilder interface in graph_runtime/src/graph/graph_builder.h"

# Step 3 — public API surface in parallel:
Task: "Define GRAPH_RUNTIME_API export macro in graph_runtime_export.h"
Task: "Define ErrorCode enum and base types in types.h"
Task: "Define umbrella header layout in graph_runtime.h"
Task: "Create stub BUILD.bazel files for all modules"
```

---

## Implementation Strategy

### MVP Scope (User Stories 1 + 2 + 3 — all P1)

The MVP requires all three P1 user stories:

1. **Phase 1**: Setup — Bazel project scaffolding
2. **Phase 2**: Foundational — define all module interfaces and contracts (design, no implementation)
3. **Phase 3**: US1 — wire BUILD targets, verify `bazel build //src/public:runtime` succeeds
4. **Phase 4**: US2 — implement JsonParser + GraphBuilder against Phase 2 Step 2 contracts
5. **Phase 5**: US3 — implement runtime engine + String Pipeline against Phase 2 Step 1 contracts
6. **STOP and VALIDATE**: `bazel build //src/...` + `bazel test //src/tests/...` + `bazel run //src/examples:string_pipeline`

### Incremental Delivery

1. **Phase 1 + 2** → Module architecture designed, interfaces defined, BUILD scaffold ready
2. **+ Phase 3 (US1)** → Library consumable as Bazel dependency
3. **+ Phase 4 (US2)** → JSON-driven graph construction works
4. **+ Phase 5 (US3)** → Full end-to-end pipeline runs (MVP complete!)
5. **+ Phase 6 (US4)** → Extensible interfaces for framework developers
6. **+ Phase 7** → Polish and platform validation

### Parallel Team Strategy

With multiple developers:

1. **Phase 1**: One person sets up Bazel/plaftorms/deps, another creates directory structure
2. **Phase 2**: Developer A does Step 1 (scheduling collaboration interfaces); Developer B does Step 2 (config parsing interfaces); Developer C does Step 3 (public API + BUILD scaffold)
3. **Phase 3 + 4**: Developer C does US1 (verify Bazel targets); Developer B does US2 (implement JsonParser + GraphBuilder)
4. **Phase 5**: Developer A does Stream/Packet/Node implementations; Developer D does Scheduler/Runtime/Example
5. **Integration**: All merge and verify together

---

## Notes

- [P] tasks = different files, no dependencies
- [Story] label maps task to specific user story for traceability
- Phase 2 produces **interface definitions only** (`.h` files); implementation (`.cc` files) happens in user story phases
- Each user story is independently completable and testable against the Phase 2 contracts
- All tasks reference exact source paths from `plan.md`
- Bazel `BUILD.bazel` files follow single `cc_library` per module convention
- All third-party dependencies have dedicated BUILD files under `graph_runtime/third_party/<name>/` — inline `build_file_content` is prohibited
- Platform support: macOS ARM64 (dev) + Linux x86_64 (deployment)
- Phase 1 does NOT include: dynamic graph, multi-threaded scheduler, visual editor, distributed execution
