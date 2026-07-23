# Implementation Plan: Project Architecture Design

**Branch**: `001-project-architecture` | **Date**: 2026-07-23 | **Spec**: `/specs/001-project-architecture/spec.md`

**Input**: Feature specification from `/specs/001-project-architecture/spec.md`

**Note**: This template drives Stream-Based Graph Runtime architecture, referencing MediaPipe and Atlas patterns.

## Summary

Design the Stream-Based Graph Runtime framework architecture as defined in `project_bootstrap.md`. Deliverable is a complete set of design artifacts: Bazel build scaffolding, public API layout, core module interfaces, platform definitions, and a String Pipeline MVP example. All code complies with Google C++ Style, Conventional Commits, and Bazel 6.5 conventions.

## Architecture Flow

The core execution flow of Graph Runtime follows a linear build-then-run pipeline:

```
┌─────────────────────────────────────────────────────────────────┐
│                      BUILD TIME                                 │
│                                                                  │
│  ┌──────────────┐                                                │
│  │  GraphConfig │  programmatic or JSON                          │
│  │  (NodeDef,   │                                                │
│  │   StreamDef, │                                                │
│  │   ExecutorDef)│                                                │
│  └──────┬───────┘                                                │
│         ▼                                                        │
│  ┌────────────────┐                                              │
│  │  GraphBuilder  │  validates contracts, creates nodes,          │
│  │  (Build())     │  wires OutputStream mirrors to                │
│  │                │  InputStreamManager deques                   │
│  └──────┬─────────┘                                              │
│         ▼                                                        │
│  ┌────────────────┐                                              │
│  │  GraphRuntime  │  owns Nodes, Scheduler, Executors            │
│  └──────┬─────────┘                                              │
│         ▼                                                        │
│  ┌────────────────┐                                              │
│  │  Scheduler     │  5-state machine, event-driven               │
│  │  (Schedule())  │  drives Node::Process() when inputs ready    │
│  │  (src/scheduler│)                                             │
│  └──────┬─────────┘                                              │
└─────────┼───────────────────────────────────────────────────────┘
          │ RUN TIME
          ▼
┌──────────────────────────────────────────────────────────────────┐
│                    NODE EXECUTION LOOP (Event-Driven)             │
│                                                                   │
│  ┌──────────┐   OutputStream    ┌──────────┐   OutputStream    ┌──────────┐
│  │  Node A  │──Manager::Send()──►│  Node B  │──Manager::Send()──►│  Node C  │
│  │(Producer)│   → mirrors       │(Transform│   → mirrors       │(Consumer)│
│  └──────────┘   → InputStreamMgr │          │   → InputStreamMgr │          │
│       │         → deque          └──────────┘   → deque          └──────────┘
│       │              │                              │
│       ▼              ▼                              ▼
│  InputStreamManager::AddPackets/SetNextTimestampBound
│       → arrival_callback_ → NotifyPacketArrival
│       → InputStreamHandler::ScheduleInvocations
│         → SyncSet::GetReadiness → kReadyForProcess
│         → SchedulerQueue::AddNode
│           → Executor::ScheduleTask
│             → ThreadPoolExecutor::Schedule
└──────────────────────────────────────────────────────────────────┘
```

**Key difference from traditional designs**: No intermediate "Stream" class. The `OutputStreamManager` writes directly to downstream `InputStreamManager` deques via mirror references. Data flows through `Node → OutputStreamShard → OutputStreamManager::PropagateUpdatesToMirrors → InputStreamManager::AddPackets/MovePackets → InputStreamShard → downstream Node`.

### Core Element Interactions

```
GraphContext (per Process() call)
┌─────────────────────────────────────────┐
│  Inputs:  InputStreamShardSet           │  ← pre-popped from InputStreamManager
│  Outputs: OutputStreamShardSet          │  → written during Process(), propagated after
│  Options: NodeOptions                   │  config-specified parameters (typed via OptionsRegistry)
│                                         │
│  Node::Process(context)                 │  user-defined business logic
│    ├── read input from Inputs().Get()   │
│    ├── compute result                   │
│    └── write output via Outputs().Get() │
└─────────────────────────────────────────┘
```

### Data Flow Sequence (per Node activation)

```
1. Scheduler marks Node ready via SchedulerQueue::AddNode
     │
2. SchedulerQueue::RunNextTask
     ▼
3. OutputStreamHandler::PrepareOutputs(shards)
     │
4. InputStreamHandler::FillInputSet(node, context)
     → for each input port: InputStreamManager::PopPacketAtTimestamp(ts)
     → populates InputStreamShard
     │
5. Node::Process(context)
     │
6. Node writes via context.Outputs().Get("port").AddPacket(packet)
     │
7. OutputStreamHandler::PostProcess(input_timestamp, shards)
     → for each OutputStreamManager:
       → ComputeOutputTimestampBound(shard)
       → PropagateUpdatesToMirrors(bound, shard)
         → last mirror: MovePackets()  |  others: AddPackets()
         → arrival_callback_ fires → NotifyPacketArrival
         → InputStreamHandler::ScheduleInvocations()
           → SyncSet::GetReadiness
           → if kReadyForProcess → SchedulerQueue::AddNode(downstream)
```

**Language/Version**: C++17

**Primary Dependencies**: GoogleTest (test framework), nlohmann/json (JSON parsing), Bazel 6.5.x

**Storage**: N/A — in-memory data flow via Packet/Stream

**Testing**: GoogleTest (`cc_test` via Bazel), unit tests per module + integration tests on `//src/public:runtime`

**Target Platform**: macOS ARM64 (development), Linux x86_64 (deployment)

**Project Type**: C++ library (Bazel `cc_library`)

**Performance Goals**: <10us per Packet hop (in-process stream), support 100+ Node graphs, minimal scheduling overhead

**Constraints**:
- Bazel 6.5.x only
- Google C++ Style (2-space indent, 80 cols, `snake_case`/`PascalCase`)
- Public symbols via `GRAPH_RUNTIME_API` macro
- `-fvisibility=hidden` for all translation units
- No dynamic Graph in Phase 1

**Scale/Scope**: Phase 1 — single-process, single-threaded scheduler; <50 Nodes; programmatic config (JSON Phase 2)

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|-----------|--------|-------|
| I. Stream-Based Graph Architecture | ✅ PASS | Core design — Nodes decoupled via InputStreamManagers/OutputStreamManagers |
| II. Configuration Driven | ✅ PASS | GraphConfig supports programmatic config; JSON parser Phase 2 |
| III. Modularity & Extensibility | ✅ PASS | All core modules define replaceable interfaces (Scheduler, Executor, InputStreamHandler) |
| IV. Google C++ Code Style | ✅ PASS | Google C++ Style enforced throughout |
| V. Build System Integrity | ✅ PASS | Bazel 6.5, platforms/, single entry point //src/public:runtime |

**GATE RESULT**: ✅ PASS — all constitutional principles satisfied. No violations requiring Complexity Tracking.

## Project Structure

### Documentation (this feature)

```text
specs/001-project-architecture/
├── plan.md              # This file
├── spec.md              # Feature specification
├── research.md          # Phase 0 — resolved unknowns
├── data-model.md        # Phase 1 — entity definitions
├── quickstart.md        # Phase 1 — build & run guide
└── contracts/           # Phase 1 — public API contracts
```

### Source Code (under `graph_runtime/` subdirectory)

```text
graph_runtime/
├── WORKSPACE                     (workspace(name = "graph_runtime"))
├── BUILD.bazel                   (root alias: //:runtime)
├── .bazelversion                 (6.5.0)
├── .bazelrc
├── graph_runtime_deps.bzl        (external dep bootstrap)
├── docs/
│   └── thread_safety.md          (thread safety documentation)
├── third_party/
│   └── nlohmann_json/
│       └── BUILD.bazel           (cc_library for nlohmann/json)
├── platforms/
│   ├── BUILD                     (config_setting + platform)
│   └── platforms.bzl             (config_setting_and_platform + graph_runtime_select)
└── src/
    ├── public/
    │   ├── BUILD                 (runtime cc_library aggregating all modules)
    │   ├── types.h               (CollectionItemId, ErrorCallback, IsStopStatus)
    │   ├── side_packet.h         (PacketSet, OutputSidePacketSet)
    │   ├── graph_builder.h/.cc   (GraphBuilder — validates and constructs graph)
    │   └── graph_runtime.h/.cc   (GraphRuntime — top-level public API)
    ├── config/
    │   ├── BUILD.bazel
    │   └── graph_config.h        (GraphConfig with NodeDef/StreamDef/ExecutorDef)
    ├── stream/
    │   ├── BUILD.bazel
    │   ├── timestamp.h/.cc       (Timestamp with 8 special values)
    │   ├── packet.h/.cc          (Packet with MakePacket/Get/Share)
    │   ├── input_stream.h        (InputStream abstract interface)
    │   ├── input_stream_manager.h/.cc (deque, PopPacketAtTimestamp, callbacks)
    │   ├── output_stream.h       (OutputStream abstract interface)
    │   ├── output_stream_shard.h/.cc (per-invocation write buffer)
    │   ├── output_stream_manager.h/.cc (mirrors, PropagateUpdatesToMirrors)
    │   └── output_stream_handler.h/.cc (PostProcess, PrepareOutputs, Close)
    ├── node/
    │   ├── BUILD.bazel
    │   ├── node.h/.cc            (Node base class — Open/Process/Close lifecycle)
    │   ├── node_contract.h       (NodeContract — port type declaration)
    │   ├── node_factory.h/.cc    (NodeFactory + NodeFactoryFor<T>)
    │   ├── node_registry.h/.cc   (NodeFactoryRegistry + GRAPH_RUNTIME_REGISTER_NODE)
    │   ├── node_options.h/.cc    (NodeOptions — key-value config)
    │   ├── options_registry.h/.cc (OptionsRegistry — typed deserialization)
    │   └── graph_context.h/.cc   (GraphContext, InputStreamShard, OutputStreamShard)
    ├── scheduler/
    │   ├── BUILD.bazel
    │   ├── executor.h            (TaskQueue + Executor interfaces)
    │   ├── scheduler_queue.h/.cc (priority queue, idle callbacks, executor binding)
    │   ├── scheduler.h/.cc       (5-state machine, HandleIdle, stopping_)
    │   ├── thread_pool_executor.h/.cc (Multi-threaded thread pool)
    │   ├── input_stream_handler.h/.cc (SyncSet, ScheduleInvocations)
    │   └── counters.h            (PerfCounters for monitoring)
    ├── examples/
    │   ├── BUILD.bazel
    │   └── string_pipeline.cc    (Producer → Uppercase → Consumer)
    └── tests/
        ├── BUILD.bazel
        ├── foundational_test.cc   (Timestamp, Packet, NodeOptions)
        ├── input_chain_test.cc    (InputStreamManager, Add/Pop)
        ├── output_chain_test.cc   (OutputStreamShard, Propagate)
        ├── node_chain_test.cc     (NodeFactory, lifecycle)
        ├── node_lifecycle_test.cc (Source/Sink/Single/Disconnected)
        └── execution_infra_test.cc(SchedulerQueue, ThreadPool)
```

**Structure Decision**: Single C++ library project under `graph_runtime/` subdirectory. All source under `graph_runtime/src/`, organized by module (stream, node, scheduler, public, config). No intermediate Stream class — OutputStreamManager writes directly to InputStreamManager deques. Bazel commands run from the `graph_runtime/` directory.

**Build Conventions**:
- All third-party dependencies MUST declare their `BUILD.bazel` in `third_party/<name>/` — inline `build_file_content` is prohibited.
- The `graph_runtime_deps.bzl` fetches archives via `http_archive` and references the external `BUILD.bazel` via the `build_file` attribute.
- Platform definitions live in `platforms/` using `config_setting_and_platform()`.
- All internal module BUILD files are named `BUILD.bazel` (lowercase, with extension) per Bazel convention.

## Complexity Tracking

> No violations — constitution check passed.
