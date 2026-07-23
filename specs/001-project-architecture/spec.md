# Feature Specification: Project Architecture Design

**Feature Branch**: `001-project-architecture`

**Created**: 2026-07-23

**Status**: Draft

**Input**: project_bootstrap.md — Stream-Based Graph Runtime framework definition

## User Stories & Testing

### User Story 1 - Graph Runtime Library as a Bazel dependency (Priority: P1)

As a C++ developer building vision pipelines (DVR/AVM/DMS), I want to depend on Graph Runtime as a Bazel library so that I can build Stream-Based dataflow graphs from JSON configuration without writing boilerplate scheduling code.

**Why this priority**: Core value proposition — without this, there is no product.

**Independent Test**: A consumer project can add `@graph_runtime//src/public:runtime` to its `deps`, include `"graph_runtime/graph_runtime.h"`, and build successfully.

**Acceptance Scenarios**:
1. **Given** an external Bazel project with Graph Runtime as a dependency, **When** building, **Then** compilation succeeds and the library is linkable.
2. **Given** a JSON graph config file describing a string pipeline, **When** the runtime parses and executes it, **Then** packets flow correctly through all nodes.

---

### User Story 2 - Configuration-driven Graph construction (Priority: P1)

As a pipeline developer, I want to define my graph topology in a JSON config file so that I can change pipeline behavior without recompiling.

**Why this priority**: Principle II (Configuration Driven) — MUST be fully satisfied for MVP.

**Independent Test**: Two different JSON configs produce two different graphs with different node topologies.

**Acceptance Scenarios**:
1. **Given** a valid JSON config, **When** parsed by the config parser, **Then** a `GraphConfig` proto is produced.
2. **Given** a `GraphConfig`, **When** passed to GraphBuilder, **Then** a runtime Graph with correct nodes and streams is constructed.

---

### User Story 3 - String Pipeline MVP (Priority: P1)

As a developer evaluating Graph Runtime, I want to run a complete string pipeline example so that I can validate the framework end-to-end.

**Why this priority**: FR-007 requires a complete MVP example.

**Independent Test**: `bazel run //src/examples:string_pipeline` processes input strings through the graph and produces correct output.

**Acceptance Scenarios**:
1. **Given** the string pipeline binary, **When** executed, **Then** it reads a JSON config, builds the graph, runs the pipeline, and produces expected string output.

---

### User Story 4 - Extensible module interfaces (Priority: P2)

As a framework developer, I want to replace the default scheduler or config parser with a custom implementation so that I can adapt Graph Runtime to different execution environments.

**Why this priority**: Important for Principle III (Modularity & Extensibility) but not required for MVP validation.

**Independent Test**: A custom scheduler implementing the `Scheduler` interface can be plugged in without modifying runtime code.

**Acceptance Scenarios**:
1. **Given** a custom scheduler implementation, **When** registered with the runtime, **Then** it is used for node scheduling instead of the default.

## Requirements

### Functional Requirements

- **FR-001**: Graph MUST use Stream-Based dataflow model. Nodes MUST NOT directly reference other Nodes.
- **FR-002**: Graph topology MUST be fully described by configuration files. Runtime MUST NOT hardcode business flow.
- **FR-003**: Runtime MUST support config parsing, Graph building, Node creation, Stream creation, and Graph initialization.
- **FR-004**: Runtime MUST manage Node lifecycle, Stream lifecycle, Node scheduling, and Stream data passing.
- **FR-005**: Graph Runtime MUST be provided as a Bazel `cc_library` consumable via `@graph_runtime//src/public:runtime`.
- **FR-006**: Config parser MUST implement `IGraphConfigParser` interface. Phase 1 defaults to JSON; additional formats MUST be addable without modifying Runtime.
- **FR-007**: Project MUST provide a complete String Pipeline example verifying config parsing, Graph building, Stream scheduling, and Node execution.
- **FR-008**: Public API MUST be exported via `GRAPH_RUNTIME_API` macro. All headers under `src/public/include/graph_runtime/`.
- **FR-009**: Build MUST use Bazel 6.5.x. Third-party deps in `graph_runtime_deps.bzl`. Platform definitions in `platforms/`.

### Non-Functional Requirements

- Library MUST be lightweight and modular.
- MUST be easily testable — unit tests on internal modules, integration tests on `//src/public:runtime`.
- MUST follow Google C++ Style Guide.
- Phase 1 MUST NOT include: visual editor, dynamic Graph, Graph optimizer, distributed execution.

## Key Entities

- **GraphConfig**: Configuration object produced by parser, consumed by builder. Platform-independent.
- **Graph**: Runtime representation of the pipeline — owns Nodes and Streams.
- **Node**: Computational unit with input/output Stream ports. Implements `Open()`, `Process()`, `Close()`.
- **Stream**: Data conduit between Nodes. Carries Packets.
- **Packet**: Atomic data unit flowing through Streams.
- **Scheduler**: Drives Node execution by managing work queues and timing.
- **CalculatorFactory**: Creates Node instances from config entries.

## Success Criteria

- **SC-001**: `bazel build //src/public:runtime` succeeds on macOS ARM64 and Linux x86_64.
- **SC-002**: `bazel test //src/tests/...` passes all unit and integration tests.
- **SC-003**: `bazel run //src/examples:string_pipeline` produces correct output.
- **SC-004**: External project can depend on `@graph_runtime//src/public:runtime` and build successfully.
- **SC-005**: Code coverage includes config parser, graph builder, scheduler, and stream/Packet modules.

## Assumptions

- Target platforms: macOS ARM64 (development), Linux x86_64 (deployment).
- C++17 standard.
- No Android/iOS support in Phase 1.
- All developers have Bazel 6.5.x installed.
- Reference implementations available at `/Users/moks/Develop/docker/ubuntu24/codes/mediapipe` and `/Users/moks/Develop/docker/ubuntu24/codes/atlas`.
