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
│  │  JSON File   │                                                │
│  └──────┬───────┘                                                │
│         ▼                                                        │
│  ┌────────────────┐                                              │
│  │JsonParser      │  implements IGraphConfigParser               │
│  │(src/config/json│)                                             │
│  └──────┬─────────┘                                              │
│         ▼                                                        │
│  ┌────────────────┐                                              │
│  │  GraphConfig   │  immutable config object                     │
│  │  (nodes,       │  consumed by builder                         │
│  │   streams)     │                                              │
│  └──────┬─────────┘                                              │
│         ▼                                                        │
│  ┌────────────────┐                                              │
│  │  GraphBuilder  │  creates Node + Stream instances             │
│  │  (src/graph/)  │  wires Node ports via Stream refs            │
│  └──────┬─────────┘                                              │
│         ▼                                                        │
│  ┌────────────────┐                                              │
│  │  RuntimeGraph  │  owns all Node/Stream/Scheduler              │
│  └──────┬─────────┘                                              │
│         ▼                                                        │
│  ┌────────────────┐                                              │
│  │  Scheduler     │  drives Node::Process() when inputs ready    │
│  │  (src/scheduler│)                                             │
│  └──────┬─────────┘                                              │
└─────────┼───────────────────────────────────────────────────────┘
          │ RUN TIME
          ▼
┌──────────────────────────────────────────────────────────────────┐
│                    NODE EXECUTION LOOP                           │
│                                                                   │
│  ┌──────────┐     ┌──────────┐     ┌──────────┐                  │
│  │  Node A  │────►│  Node B  │────►│  Node C  │                  │
│  │(Producer)│     │(Transform│     │(Consumer)│                  │
│  └──────────┘     └──────────┘     └──────────┘                  │
│       │                │                │                         │
│       ▼                ▼                ▼                         │
│  ┌──────────┐     ┌──────────┐     ┌──────────┐                  │
│  │ Stream   │────►│ Stream   │────►│ Stream   │                  │
│  │(Packet   │     │(Packet   │     │(Packet   │                  │
│  │ Queue)   │     │ Queue)   │     │ Queue)   │                  │
│  └──────────┘     └──────────┘     └──────────┘                  │
│                                                                   │
└──────────────────────────────────────────────────────────────────┘
```

### Core Element Interactions

```
ExecutionContext (per Process() call)
┌─────────────────────────────────────────┐
│  CalculatorContext                       │
│  ├── inputs:  map<string, Packet&>      │  ← read from input Streams
│  ├── outputs: map<string, PacketProducer>│  → write to output Streams
│  └── options: NodeOptions                │  config-specified parameters
│                                          │
│  Calculator::Process(context)            │  user-defined business logic
│    ├── read input Packets                │
│    ├── compute result                    │
│    └── push output Packets               │
└─────────────────────────────────────────┘
```

### Data Flow Sequence (per Node activation)

```
 1. Scheduler marks Node ready
       │
 2. For each input Stream:
       ▼
    Pop next Packet → place in context.inputs
       │
 3. Calculator::Process(context) called
       │
 4. Calculator writes to context.outputs
       │
 5. For each output Stream:
       ▼
    Enqueue Packet → notify downstream Scheduler
       │
 6. Scheduler marks downstream Node(s) ready
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

**Scale/Scope**: Phase 1 — single-process, single-threaded scheduler; <50 Nodes; JSON-only config

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|-----------|--------|-------|
| I. Stream-Based Graph Architecture | ✅ PASS | Core design — Nodes decoupled via Streams |
| II. Configuration Driven | ✅ PASS | JSON parser via IGraphConfigParser interface |
| III. Modularity & Extensibility | ✅ PASS | All core modules have replaceable interfaces |
| IV. Google C++ Code Style | ✅ PASS | Strictly enforced; GRAPH_RUNTIME_API macro |
| V. Build System Integrity | ✅ PASS | Bazel 6.5, platforms/, single entry point |

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

### Source Code (repository root)

```text
WORKSPACE                     (workspace(name = "graph_runtime"))
BUILD.bazel                   (root alias: //:runtime)
.bazelversion                 (6.5.0)
.bazelrc
AGENTS.md
graph_runtime_deps.bzl        (external dep bootstrap)
platforms/
├── BUILD                     (config_setting + platform)
└── platforms.bzl             (config_setting_and_platform + graph_runtime_select)

src/
├── public/
│   ├── BUILD                 (public cc_library + cc_binary shared)
│   ├── include/
│   │   └── graph_runtime/
│   │       ├── graph_runtime.h            (umbrella)
│   │       ├── graph_runtime_export.h     (GRAPH_RUNTIME_API)
│   │       ├── graph.h
│   │       ├── packet.h
│   │       ├── node.h
│   │       └── types.h
│   └── graph_runtime_init.cc
├── config/
│   ├── BUILD.bazel
│   ├── i_graph_config_parser.h
│   ├── graph_config.h
│   └── json/
│       ├── BUILD.bazel
│       └── json_parser.h / .cc
├── graph/
│   ├── BUILD.bazel
│   ├── graph.h / .cc
│   └── graph_builder.h / .cc
├── runtime/
│   ├── BUILD.bazel
│   └── runtime.h / .cc
├── scheduler/
│   ├── BUILD.bazel
│   └── scheduler.h / .cc
├── stream/
│   ├── BUILD.bazel
│   └── stream.h / .cc, packet.h / .cc
├── node/
│   ├── BUILD.bazel
│   ├── node.h / .cc
│   └── calculator_factory.h / .cc
├── examples/
│   ├── BUILD.bazel
│   └── string_pipeline.cc
└── tests/
    ├── BUILD.bazel
    ├── graph_builder_test.cc
    ├── config_parser_test.cc
    ├── scheduler_test.cc
    └── integration_test.cc
```

**Structure Decision**: Single C++ library project, following Atlas layout conventions. All source under `src/`, public API under `src/public/include/graph_runtime/` with `strip_include_prefix`.

## Complexity Tracking

> No violations — constitution check passed.
