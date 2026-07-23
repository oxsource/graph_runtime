---

description: "Task list for Scheduler-Node-Stream-Packet module interaction implementation"

---

# Tasks: Scheduler-Node-Stream-Packet Module Interaction Implementation

**Input**: Design documents from `/specs/002-scheduler-stream-packet-design/`

**Prerequisites**: plan.md (required), spec.md (required for user stories), research.md, data-model.md, contracts/

**Tests**: Test tasks are included per the implementation plan (GoogleTest cc_test per module + integration test).

**Organization**: Tasks grouped by module dependency order, mapped to user stories for independent verification.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

## Path Conventions

- **Workspace root** (run `bazel` from here): `graph_runtime/`
- **Contracts reference**: `specs/002-scheduler-stream-packet-design/contracts/` for interface definitions
- **Timestamp**: `graph_runtime/src/stream/timestamp.h/.cc`
- **Packet**: `graph_runtime/src/stream/packet.h/.cc`
- **Input stream**: `graph_runtime/src/stream/input_stream.h`, `input_stream_manager.h/.cc`
- **Output stream**: `graph_runtime/src/stream/output_stream.h`, `output_stream_manager.h/.cc`, `output_stream_shard.h/.cc`, `output_stream_handler.h/.cc`
- **Node**: `graph_runtime/src/node/node.h/.cc`, `node_contract.h`, `node_factory.h/.cc`, `node_registry.h`, `node_options.h`, `options_registry.h`
- **Scheduler**: `graph_runtime/src/scheduler/scheduler.h/.cc`, `scheduler_queue.h/.cc`, `executor.h`, `thread_pool_executor.h/.cc`
- **Runtime**: `graph_runtime/src/public/graph_runtime.h/.cc`, `graph_builder.h/.cc`, `side_packet.h`
- **Config**: `graph_runtime/src/config/graph_config.h`
- **Tests**: `graph_runtime/src/tests/`

---

## Module Dependency Graph

```
Foundational (Phase 2)
├── Types + Timestamp + Packet + NodeOptions + OptionsRegistry
│
├── US1: Module Interaction Contracts (Phase 3)
│   ├── Input chain:  InputStream → InputStreamShard → InputStreamManager → InputStreamHandler + SyncSet
│   ├── Output chain: OutputStream → OutputStreamShard → OutputStreamManager → OutputStreamHandler
│   ├── Node chain:   Node → NodeContract → NodeFactory → NodeFactoryRegistry
│   └── Context:      GraphContext → GraphContextManager
│
├── US2: Data Flow & Scheduling (Phase 4)
│   ├── Execution:    SchedulerQueue → Executor → ThreadPoolExecutor
│   ├── Orchestrator: Scheduler (event-driven, 5-state, stopping_, HandleIdle)
│   ├── I/O:          SidePacket → GraphConfig → GraphBuilder → GraphRuntime
│
├── US3: Node Boundaries (Phase 5)
│   └── Lifecycle edge cases, error scenarios
│
└── Polish (Phase 6)
    └── Integration tests, documentation
```

---

## Phase 1: Setup — Build Infrastructure

**Purpose**: Create directory structure and BUILD.bazel files for all modules.

- [ ] T001 Create directory structure per plan.md (`graph_runtime/src/stream/`, `graph_runtime/src/node/`, `graph_runtime/src/scheduler/`, `graph_runtime/src/public/`, `graph_runtime/src/config/`, `graph_runtime/src/tests/`)
- [ ] T002 [P] Create BUILD.bazel for stream module (`graph_runtime/src/stream/BUILD.bazel`) with cc_library targets for Timestamp, Packet, InputStreamManager, InputStream, OutputStream, OutputStreamShard, OutputStreamManager, OutputStreamHandler
- [ ] T003 [P] Create BUILD.bazel for node module (`graph_runtime/src/node/BUILD.bazel`) with cc_library targets for Node, NodeContract, NodeFactory, NodeFactoryRegistry, NodeOptions, OptionsRegistry, GraphContext, GraphContextManager
- [ ] T004 [P] Create BUILD.bazel for scheduler module (`graph_runtime/src/scheduler/BUILD.bazel`) with cc_library targets for Scheduler, SchedulerQueue, Executor, ThreadPoolExecutor
- [ ] T005 [P] Create BUILD.bazel for public module (`graph_runtime/src/public/BUILD.bazel`) with cc_library for runtime aggregating all modules
- [ ] T006 [P] Create BUILD.bazel for config module (`graph_runtime/src/config/BUILD.bazel`) with GraphConfig
- [ ] T007 Create BUILD.bazel for tests (`graph_runtime/src/tests/BUILD.bazel`) with cc_test targets

---

## Phase 2: Foundational — Core Data Types & Options

**Purpose**: Implement the zero-dependency types that all other modules depend on.

- [ ] T008 Implement `Types` (`graph_runtime/src/public/types.h`): CollectionItemId (int), ErrorCallback, IsStopStatus(), StatusStop()
- [ ] T009 Implement `Timestamp` (`graph_runtime/src/stream/timestamp.h/.cc`): 8 special values, int64_t encoding, IsSpecialValue/IsRangeValue/IsAllowedInStream, NextAllowedInStream/PreviousAllowedInStream, DebugString, TimestampDiff
- [ ] T010 Implement `Packet` (`graph_runtime/src/stream/packet.h/.cc`): MakePacket/Adopt factories, shallow copy via shared_ptr, Get/ValidateAsType/Share, At(timestamp), operator== (pointer equality), DebugString/DebugTypeName
- [ ] T011 Implement `NodeOptions` (`graph_runtime/src/node/node_options.h/.cc`): Set/Get/Has/Keys, std::any storage
- [ ] T012 Implement `OptionsRegistry` (`graph_runtime/src/node/options_registry.h/.cc`): Register/Deserialize/IsRegistered, GRAPH_RUNTIME_REGISTER_OPTIONS macro
- [ ] T013 Create foundational unit test (`graph_runtime/src/tests/foundational_test.cc`): Timestamp special values, Packet copy/move/Get/ValidateAsType, NodeOptions set/get

**Checkpoint**: `bazel test //src/tests:foundational_test` passes. Core types ready.

---

## Phase 3: User Story 1 — Module Interaction Contracts (Priority: P1) 🎯 MVP

**Goal**: Define and implement all interface contracts and interaction patterns between core modules. Each module can be tested independently with mock collaborators.

**Independent Test**: A mock InputStreamManager can be driven by a real OutputStream, and a mock OutputStream can be driven by a real InputStreamManager — proving bidirectional substitution.

### Sub-phase 3a — Input Stream Chain

- [ ] T014 [P] [US1] Implement `InputStream` abstract interface (`graph_runtime/src/stream/input_stream.h`): Name/Value/IsEmpty/IsDone/Header/Get<T>
- [ ] T015 [P] [US1] Implement `InputStreamManager` (`graph_runtime/src/stream/input_stream_manager.h/.cc`): std::deque<Packet> queue, AddPackets/MovePackets, SetNextTimestampBound, Close, PopPacketAtTimestamp, PopQueueHead, MinTimestampOrBound, IsDone, QueueSize, MaxQueueSize, SetQueueSizeCallbacks, SetArrivalCallback, NumPacketsAdded
- [ ] T016 [US1] Implement `InputStreamShard` (`graph_runtime/src/node/graph_context.h` inline): inherits InputStream, packet_queue_, is_done_, implements Value/IsEmpty/IsDone/Header/Name
- [ ] T017 [P] [US1] Implement `SyncSet` (`graph_runtime/src/scheduler/input_stream_handler.h`): GetReadiness (min_packet vs min_bound), FillInputSet, FillInputBounds
- [ ] T018 [P] [US1] Implement `InputStreamHandler` interface (`graph_runtime/src/scheduler/input_stream_handler.h/.cc`): SetScheduleCallback, ScheduleInvocations, GetNodeReadiness, FillInputSet, NotifyPacketArrival, SetNextTimestampBound, Close
- [ ] T019 [US1] Implement `DefaultInputStreamHandler` (`graph_runtime/src/scheduler/input_stream_handler.cc`): single SyncSet, all-inputs-barrier readiness
- [ ] T020 [US1] Create input chain test (`graph_runtime/src/tests/input_chain_test.cc`): mock OutputStream → InputStreamManager → InputStreamHandler → ScheduleInvocations

### Sub-phase 3b — Output Stream Chain

- [ ] T021 [P] [US1] Implement `OutputStream` abstract interface (`graph_runtime/src/stream/output_stream.h`): Name/AddPacket/SetNextTimestampBound/NextTimestampBound/Close/IsClosed/SetOffset/OffsetEnabled/Offset/SetHeader/Header
- [ ] T022 [P] [US1] Implement `OutputStreamShard` (`graph_runtime/src/stream/output_stream_shard.h/.cc`): inherits OutputStream, output_queue_, next_timestamp_bound_, updated_next_timestamp_bound_, Reset, friend OutputStreamManager
- [ ] T023 [P] [US1] Implement `OutputStreamSpec` and `OutputStreamManager` (`graph_runtime/src/stream/output_stream_manager.h/.cc`): mirrors_, ComputeOutputTimestampBound, PropagateUpdatesToMirrors (last mirror move), ResetShard, Close, PropagateHeader, LockIntroData, Spec
- [ ] T024 [US1] Implement `OutputStreamHandler` (`graph_runtime/src/stream/output_stream_handler.h/.cc`): InitializeOutputStreamManagers, SetupOutputShards, Open, PrepareOutputs, PostProcess, Close, UpdateTaskTimestampBound, TryPropagateTimestampBound
- [ ] T025 [US1] Implement `InOrderOutputStreamHandler` (`graph_runtime/src/stream/output_stream_handler.cc`): direct PropagateOutputPackets (Phase 1 sequential path)
- [ ] T026 [US1] Create output chain test (`graph_runtime/src/tests/output_chain_test.cc`): OutputStreamShard → OutputStreamManager::PropagateUpdatesToMirrors → mock InputStreamManager

### Sub-phase 3c — Node & Context Chain

- [ ] T027 [P] [US1] Implement `NodeContract` (`graph_runtime/src/node/node_contract.h/.cc`): PacketType, PacketTypeSet, Inputs/Outputs, InputSidePackets/OutputSidePackets, Options, SetMaxInFlight, SetProcessTimestampBounds
- [ ] T028 [P] [US1] Implement `Node` base class (`graph_runtime/src/node/node.h/.cc`): name, input_port_managers_, output_streams_, executor_name_, scheduler_queue_, source_layer_, Open/Process/Close virtual, SetInputPort/SetOutputPort, SetExecutorName, SetSchedulerQueue, SetSourceLayer, SourceProcessOrder
- [ ] T029 [P] [US1] Implement `NodeFactory` + `NodeFactoryFor<T>` (`graph_runtime/src/node/node_factory.h/.cc`): GetContract, CreateNode, NodeFactoryFor<T> template with static_assert for HasGetContract
- [ ] T030 [P] [US1] Implement `NodeFactoryRegistry` (`graph_runtime/src/node/node_registry.h/.cc`): Register, Unregister, CreateByName, CreateByNameInNamespace, GetFactory, IsRegistered, RegisteredTypes, GRAPH_RUNTIME_REGISTER_NODE macro, NodeRegistrationToken
- [ ] T031 [P] [US1] Implement `GraphContext` (`graph_runtime/src/node/graph_context.h/.cc`): NodeName/NodeId/CalculatorType/InputTimestamp, InputStreamShardSet/OutputStreamShardSet, Options<T>, InputSidePackets/OutputSidePackets, SetOffset
- [ ] T032 [US1] Implement `GraphContextManager` (`graph_runtime/src/node/graph_context.h/.cc`): GetDefaultCalculatorContext, PrepareCalculatorContext (Phase 2 stub), RecycleCalculatorContext (Phase 2 stub), CleanupAfterRun
- [ ] T033 [US1] Create node chain test (`graph_runtime/src/tests/node_chain_test.cc`): NodeFactoryRegistry::CreateByName → NodeFactory::CreateNode → Node::Open/Process/Close lifecycle with GraphContext

**Checkpoint**: All module interfaces are implemented and independently testable. `bazel test //src/tests:input_chain_test && bazel test //src/tests:output_chain_test && bazel test //src/tests:node_chain_test` passes.

---

## Phase 4: User Story 2 — Data Flow Lifecycle & Scheduling Semantics (Priority: P1)

**Goal**: Scheduler drives Node execution in topological order; Packets flow through the complete input → process → output chain; external data injection and side packets work.

**Independent Test**: A three-Node linear pipeline processes 1000 Packets with zero data loss and correct ordering, verified via `bazel run //src/examples:string_pipeline`.

### Sub-phase 4a — Execution Infrastructure

- [ ] T034 [P] [US2] Implement `Executor` base class (`graph_runtime/src/scheduler/executor.h`): TaskQueue interface (RunNextTask), Executor (AddTask, Schedule), RegisterExecutor
- [ ] T035 [P] [US2] Implement `SchedulerQueue` (`graph_runtime/src/scheduler/scheduler_queue.h/.cc`): priority_queue<Item>, SetExecutor, SetIdleCallback, AddNode, AddNodeForOpen, RunNextTask (pops highest priority → OpenNode or ProcessNode), SetRunning, Reset, IsIdle, num_pending_tasks_
- [ ] T036 [US2] Implement `ThreadPoolExecutor` (`graph_runtime/src/scheduler/thread_pool_executor.h/.cc`): Create factory, Schedule, min(CPUs, nodes) default thread count, ThreadPool internal class
- [ ] T037 [US2] Create execution infrastructure test (`graph_runtime/src/tests/execution_infra_test.cc`): SchedulerQueue + ThreadPoolExecutor task dispatch, idle callbacks

### Sub-phase 4b — Scheduler

- [ ] T038 [P] [US2] Implement `Scheduler` base + default implementation (`graph_runtime/src/scheduler/scheduler.h/.cc`): 5-state machine, SetInputStreamHandler, SetDefaultExecutor, SetNonDefaultExecutor, SetErrorCallback, Schedule (non-blocking: topological ordering → source layering → InputStreamManager callbacks → register queue idle callbacks → AssignNodeToQueue → activate sources), WaitUntilDone, Shutdown, Pause/Resume, AssignNodeToQueue, HandleIdle (reentrancy guard, non_idle_queue_count_, CleanupActiveSources, auto-unthrottle, deadlock detection)
- [ ] T039 [US2] Implement event-driven execution flow in Scheduler task runner: OpenNode (GraphContext creation with Unstarted → Node::Open → output_stream_handler_->Open → OnNodeOpened), ProcessNode (stopping_ check → FillInputSet → Node::Process → error/stop handling → OutputStreamHandler::PostProcess → source rescheduling), CloseNode (PrepareOutputs → GraphContext with Done → Node::Close → output_stream_handler_->Close)
- [ ] T040 [US2] Implement stopping_ propagation: when Node returns StatusStop → set stopping_ → CloseNode for all active_sources_; subsequent Source ProcessNode intercept → skip Process, schedule Close directly
- [ ] T041 [US2] Implement error propagation: error_callback_ → HasError → stopping_ → CloseNode for all active_sources → HandleIdle → kTerminated
- [ ] T042 [US2] Create scheduler unit test (`graph_runtime/src/tests/scheduler_test.cc`): state machine transitions, stopping_ behavior, error propagation, HandleIdle with throttled sources

### Sub-phase 4c — Side Packets

- [ ] T043 [US2] Implement `SidePacket` (`graph_runtime/src/public/side_packet.h/.cc`): PacketSet (immutable input), OutputSidePacketSet (mutable output)
- [ ] T044 [US2] Integrate side packets into GraphContext: InputSidePackets() returns const PacketSet&, OutputSidePackets() returns OutputSidePacketSet&; side packet validation during Schedule() (missing side packet → FailedPreconditionError)

### Sub-phase 4d — GraphConfig, GraphBuilder, GraphRuntime

- [ ] T045 [US2] Implement `GraphConfig` (`graph_runtime/src/config/graph_config.h`): NodeDef, StreamDef, ExecutorDef, input_streams, output_streams, max_queue_size, report_deadlock
- [ ] T046 [US2] Implement `GraphBuilder` (`graph_runtime/src/public/graph_builder.h/.cc`): Build(config) → ValidateContracts (NodeFactory::GetContract per node, check stream + side packet types, validate OptionsRegistry) → CreateExecutors → CreateNodes → CreateInputStreamManagers → CreateOutputStreams + OutputStreamManagers → WireMirrors → CreateGraphInputStreams → CreateGraphOutputStreams → CreateOutputStreamHandlers → CreateScheduler → AssignNodesToQueues → return GraphRuntime
- [ ] T047 [US2] Implement `GraphRuntime` (`graph_runtime/src/public/graph_runtime.h/.cc`): Initialize (delegates to GraphBuilder::Build), Start (calls Scheduler::Schedule), WaitUntilDone, Shutdown, AddPacketToInputStream (inject into virtual GraphInputStream Source), CloseInputStream, SetOutputStreamCallback, ClearOutputStreamCallback, SetInputSidePacket, SetOutputSidePacketCallback
- [ ] T048 [US2] Create string pipeline example (`graph_runtime/src/examples/string_pipeline.cc`): producer Source → uppercase Transform → consumer Sink, JSON config, demonstrates end-to-end flow
- [ ] T049 [US2] Create integration test (`graph_runtime/src/tests/integration_test.cc`): 3-Node linear pipeline, 1000 Packets, verify zero data loss and correct ordering

**Checkpoint**: `bazel run //src/examples:string_pipeline` produces correct output. `bazel test //src/tests:integration_test` passes.

---

## Phase 5: User Story 3 — Node Lifecycle Boundaries (Priority: P2)

**Goal**: Node lifecycle edge cases are handled correctly — source/sink nodes, error recovery, empty/single-node graphs.

**Independent Test**: A single Node with lifecycle logging can be driven by a test harness that invokes Open, then multiple Process calls (including error and stop), then Close. The invocation order and count are verified without a full Graph.

- [ ] T050 [P] [US3] Implement source Node lifecycle behavior: zero inputs → schedule initial ProcessNode after Open, reschedule if !stopping_, close on StatusStop or Shutdown
- [ ] T051 [P] [US3] Implement sink Node lifecycle behavior: zero outputs → Process result discarded, close when all input streams done
- [ ] T052 [US3] Implement empty graph handling: zero Nodes → Schedule() immediately transitions to kTerminated via HandleIdle
- [ ] T053 [US3] Implement single-Node graph handling: Node that is both source and sink (zero inputs, zero outputs) — Open → Process loop → Close on Shutdown
- [ ] T054 [US3] Implement disconnected subgraph support: two independent subgraphs in the same GraphRuntime execute independently, completion waits for both
- [ ] T055 [US3] Create Node lifecycle test (`graph_runtime/src/tests/node_lifecycle_test.cc`): source/sink/empty/single-node/disconnected subgraphs

**Checkpoint**: `bazel test //src/tests:node_lifecycle_test` passes.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Memory management, thread safety preparation, documentation.

- [ ] T056 [P] Add `CleanupAfterRun()` to all manager classes: InputStreamManager::PrepareForRun/reset queue, OutputStreamManager cleanup, SchedulerQueue cleanup, GraphContextManager::CleanupAfterRun
- [ ] T057 [P] Verify CloseNode idempotency across stopping_ + in-flight ProcessNode race conditions
- [ ] T058 Add memory and thread safety documentation: which methods are thread-safe (Executor::Schedule, Scheduler::Shutdown), which are not (Node::Process)
- [ ] T059 [P] Add DEBUG logging and counter support to Scheduler and SchedulerQueue for performance analysis
- [ ] T060 Validate against quickstart.md: run build, test, and example commands; verify end-to-end workflow

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — can start immediately
- **Foundational (Phase 2)**: Depends on Setup — BLOCKS all user stories
- **US1 (Phase 3)**: Depends on Foundational (Packet, Timestamp, Types, NodeOptions)
- **US2 (Phase 4)**: Depends on US1 (all module contracts exist)
- **US3 (Phase 5)**: Depends on US1 (Node lifecycle) + US2 (Scheduler for graph completion)
- **Polish (Phase 6)**: Depends on all user stories

### Sub-phase Dependencies within US1

```
3a Input Chain       3b Output Chain      3c Node/Context Chain
├── InputStream      ├── OutputStream      ├── NodeContract
├── InputStreamMgr   ├── OutputStreamShard ├── Node
├── InputStreamShard ├── OutputStreamMgr   ├── NodeFactory
├── SyncSet          ├── OutputStreamHdlr  ├── NodeFactoryRegistry
├── InputStreamHdlr  ├── InOrderHandler    ├── GraphContext
│   └── DefaultHdlr  └── test              └── GraphContextMgr
│   └── test                                 └── test
└────────────────────┴────────────────────┘
                    All three sub-phases are INDEPENDENT — can run in parallel
```

### Within US2

```
4a Execution Infra      4b Scheduler       4c SidePackets   4d Config/Runtime
├── Executor            ├── Scheduler       ├── PacketSet     ├── GraphConfig
├── SchedulerQueue      ├── event-driven    ├── OutputSet     ├── GraphBuilder
├── ThreadPoolExecutor  │   flow            └── integrate     ├── GraphRuntime
│   └── test            ├── stopping_       └── test          └── example + int test
│                       ├── error handling
│                       └── test
│
└── 4a, 4c can run in parallel. 4b depends on 4a. 4d depends on 4b + 4c.
```

---

## Parallel Opportunities

| Phase | Parallel Tasks | Rationale |
|-------|---------------|-----------|
| Phase 1 | T002, T003, T004, T005, T006 | All BUILD.bazel files, different modules |
| Phase 2 | T008, T009, T010, T011, T012 | Types, Timestamp, Packet, NodeOptions, OptionsRegistry — all independent |
| Phase 3 | T014–T020 (3a), T021–T026 (3b), T027–T033 (3c) | Three sub-chains fully independent |
| Phase 4 | T034, T043, T045 | Executor/SchedulerQueue, SidePacket, GraphConfig — independent |
| Phase 4 | T038, T043 | Scheduler and SidePacket — independent |
| Phase 4 | T046, T047, T048 | GraphBuilder, GraphRuntime, example — after Scheduler |
| Phase 5 | T050, T051 | Source and sink — independent |
| Phase 6 | T056, T057, T059 | Cleanup, idempotency, logging — different concerns |

---

## Implementation Strategy

### MVP Scope (US1 + US2 — both P1)

The MVP requires both P1 user stories:

1. **Phase 1 + 2**: Setup + Foundational (Timestamps, Packets, Types, Options)
2. **Phase 3**: US1 — all module interface contracts implemented and independently testable
3. **Phase 4**: US2 — Scheduler, GraphRuntime, end-to-end string pipeline
4. **STOP and VALIDATE**: `bazel run //src/examples:string_pipeline` + `bazel test //src/tests/...`

### Incremental Delivery

1. **Phase 1 + 2**: Core types ready
2. **+ US1 (Phase 3)**: All contracts implemented, each module independently testable
3. **+ US2 (Phase 4)**: Full runtime with Scheduler, Graph, external I/O, string pipeline (MVP complete!)
4. **+ US3 (Phase 5)**: Node lifecycle edge cases
5. **+ Phase 6**: Polish, cleanup, documentation

---

## Notes

- [P] tasks = different files, no dependencies
- [Story] label maps task to specific user story for traceability
- All file paths are under `graph_runtime/src/` (Bazel workspace root)
- Bazel BUILD.bazel files follow single cc_library per module convention
- Phase 3 sub-phases (3a, 3b, 3c) are independent and can be assigned to different developers
- Test tasks are required per the implementation plan (GoogleTest cc_test per module)
