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
- **Public API**: `graph_runtime/src/public/`
- **Internal modules**: `graph_runtime/src/stream/`, `graph_runtime/src/node/`, `graph_runtime/src/scheduler/`, `graph_runtime/src/config/`
- **Examples**: `graph_runtime/src/examples/`
- **Tests**: `graph_runtime/src/tests/`

---

## Phase 1: Setup — Build Infrastructure (Completed)

**Purpose**: Create directory structure and BUILD.bazel files for all modules.

- [X] T001: Create project directory structure
- [X] T002–T007: Create BUILD.bazel for stream, node, scheduler, public, config, tests

---

## Phase 2: Foundational — Core Data Types (Completed)

**Purpose**: Implement zero-dependency types that all modules depend on.

- [X] T008: Types (CollectionItemId, ErrorCallback, IsStopStatus, StatusStop)
- [X] T009: Timestamp (8 special values, arithmetic, DebugString)
- [X] T010: Packet (MakePacket/Adopt, Get/Share/ValidateAsType, shallow copy)
- [X] T011: NodeOptions (Set/Get/Has/Keys, std::any storage)
- [X] T012: OptionsRegistry (Register/Deserialize, GRAPH_RUNTIME_REGISTER_OPTIONS)
- [X] T013: foundational_test.cc

---

## Phase 3: Module Interaction Contracts (Completed)

**Goal**: All module interfaces implemented and independently testable.

**3a — Input Chain**: [X] T014–T020
- InputStream, InputStreamManager (deque, callbacks), InputStreamShard
- SyncSet, InputStreamHandler, DefaultInputStreamHandler
- input_chain_test.cc

**3b — Output Chain**: [X] T021–T026
- OutputStream, OutputStreamShard (per-invocation buffer)
- OutputStreamManager (mirrors, PropagateUpdatesToMirrors)
- OutputStreamHandler (PostProcess, PrepareOutputs, Close)
- output_chain_test.cc

**3c — Node/Context Chain**: [X] T027–T033
- NodeContract, Node base class, NodeFactory + NodeFactoryFor<T>
- NodeFactoryRegistry + GRAPH_RUNTIME_REGISTER_NODE
- GraphContext (full CalculatorContext alignment), GraphContextManager
- node_chain_test.cc

---

## Phase 4: Data Flow & Scheduling (Completed)

**Goal**: Scheduler drives Node execution; end-to-end pipeline works.

**4a — Execution Infra**: [X] T034–T037
- Executor (TaskQueue, AddTask/Schedule), SchedulerQueue (priority queue)
- ThreadPoolExecutor (multi-threaded thread pool)
- execution_infra_test.cc

**4b — Scheduler**: [X] T038–T042
- Scheduler (5-state, Schedule, WaitUntilDone, HandleIdle)
- Event-driven execution flow (OpenNode → ProcessNode → CloseNode)
- stopping_ propagation, error propagation
- scheduler_test.cc

**4c — Side Packets**: [X] T043–T044
- PacketSet / OutputSidePacketSet, GraphContext integration

**4d — Config/Runtime**: [X] T045–T049
- GraphConfig (NodeDef/StreamDef/ExecutorDef), GraphBuilder (Build pipeline)
- GraphRuntime (Initialize/Start/WaitUntilDone/Shutdown)
- string_pipeline example, integration_test.cc

---

## Phase 5: Node Lifecycle Boundaries (Completed)

**Goal**: Edge cases handled — source/sink, empty/single-node, disconnected subgraphs.

- [X] T050–T055: Source, Sink, Empty graph, Single-Node, Disconnected subgraphs
- node_lifecycle_test.cc (6 subtests, all pass)

---

## Phase 6: Polish & Cross-Cutting (Completed)

**Purpose**: Memory management, thread safety, final validation.

- [X] T056: CleanupAfterRun/PrepareForRun on all managers
- [X] T057: CloseNode idempotency test
- [X] T058: thread_safety.md documentation
- [X] T059: counters.h for performance monitoring
- [X] T060: Final build and test validation
