# Feature Specification: MediaPipe Gap Closure

**Feature Branch**: `009-mediapipe-gap-closure`

**Created**: 2026-07-24
**Status**: Draft

**Input**: 分析现有 graph_runtime 项目与 MediaPipe 之间的功能缺口，制定分阶段修复计划，消除所有已声明 stub 和功能缺陷。

## User Scenarios & Testing

### User Story 1 — Side Packet & Output Callback Stub Closure (Priority: P1)

As a library user, I want `SetOutputSidePacketCallback`, `SetInputSidePacket`, `SetOutputStreamCallback`, and `ClearOutputStreamCallback` to actually work, so that I can receive output data from and inject input data into a running graph.

**Why this priority**: These 4 methods are declared in the public API but are empty stubs — any caller relying on them gets silent no-ops.

**Independent Test**: A dedicated test calls each method with a test callback and verifies the callback fires with expected data.

**Acceptance Scenarios**:
1. **Given** a running graph with `SetOutputStreamCallback("output", cb)` registered, **When** a node produces output on that stream, **Then** `cb` is invoked with the output packet
2. **Given** `ClearOutputStreamCallback("output")` called, **When** output arrives on that stream, **Then** `cb` is not invoked
3. **Given** `SetInputSidePacket("tag", packet)` called before graph start, **When** a node's `Open()` reads its side packet, **Then** the packet is available
4. **Given** `SetOutputSidePacketCallback("tag", cb)` registered, **When** a node produces a side packet during `Close()`, **Then** `cb` fires

---

### User Story 2 — Scheduler Async Path Fix (Priority: P1)

As a library user, I want `WaitUntilDone()` to work correctly when the graph is started via `Start()` and fed packets via `AddPacketToInputStream`, so that I can await graph completion instead of calling `Shutdown()` as a workaround.

**Why this priority**: Currently `add_packet_demo.cc` and `async_pipeline_demo.cc` both use `Shutdown()` instead of `WaitUntilDone()` because the async scheduler path (`Start()` + `HandleIdle()`) does not properly drive the graph to `kTerminated`.

**Independent Test**: `add_packet_demo.cc` is modified to call `WaitUntilDone()` instead of `Shutdown()` and the test passes.

**Acceptance Scenarios**:
1. **Given** a graph started with `Start()` and packets added via `AddPacketToInputStream`, **When** all input streams are closed and all packets are consumed, **Then** `WaitUntilDone()` returns and the graph state is `kTerminated`
2. **Given** a graph with source nodes started via `Start()`, **When** all sources complete, **Then** `WaitUntilDone()` returns
3. **Given** a graph started via `Start()`, **When** `Shutdown()` is called externally, **Then** `WaitUntilDone()` returns promptly

---

### User Story 3 — Graph Lifecycle Queries (Priority: P2)

As a library user, I want `WaitForIdle()`, `HasGraphFinished()`, `GetGraphState()` to be available, so that I can monitor and react to graph progress without polling.

**Why this priority**: MediaPipe provides these for non-blocking graph monitoring. Currently graph_runtime only exposes `WaitUntilDone()` which is blocking.

**Independent Test**: A test starts a graph, waits briefly, calls `GetGraphState()` and `HasGraphFinished()`, adds more packets, and verifies state transitions.

**Acceptance Scenarios**:
1. **Given** a running graph, **When** `HasGraphFinished()` is called, **Then** it returns `false` while processing and `true` after completion
2. **Given** a graph between processing bursts, **When** `WaitForIdle()` is called, **Then** it returns when the graph has no pending work

---

### User Story 4 — Pause/Resume (Priority: P2)

As a library user, I want `Pause()` and `Resume()` to work, so that I can temporarily halt graph execution and resume it later.

**Why this priority**: These are declared but return `UnimplementedError`.

**Independent Test**: A test starts a graph, pauses it, verifies no processing occurs, resumes it, and verifies processing continues.

---

### User Story 5 — Config Validation Completeness (Priority: P2)

As a library user, I want `ConfigValidator` to detect more configuration errors, including stream connectivity, cycle detection, and type mismatches.

**Why this priority**: Currently only name/executor uniqueness is checked. Invalid configurations may fail at runtime with confusing errors.

**Independent Test**: A test provides a config with a missing input stream node and verifies the validator returns a clear error.

**Acceptance Scenarios**:
1. **Given** a config where `NodeDef.input_stream` references a stream from a non-existent node, **When** validated, **Then** a clear error is returned
2. **Given** a config with a cycle (A→B→C→A), **When** validated, **Then** a cycle error is returned
3. **Given** a config with mismatched input/output types, **When** validated, **Then** a type mismatch error is returned

---

## General Notes

- The grpah Runtime "WaitUntilDone" does not work because the scheduler's asynchronous path does not naturally drive the graph to termination state. The `WaitUntilDone` method is already functional in the synchronous path but not with multi-input processing. This is a specific defect to address.
- The spec 007 refers to the implementation of the scheduler but does not include the full feature gap between graph_runtime and MediaPipe.
- All currently `UnimplementedError` calls need to be tracked and resolved.

## Key Entities

- **GraphRuntime**: Main public API class in `src/framework/public/graph_runtime.h`
- **Scheduler**: Scheduling engine in `src/framework/scheduler/scheduler.h`
- **InputStreamHandler**: Stream policy implementations in `src/framework/scheduler/input_stream_handler.h`
- **GraphContext**: Per-invocation context passed to `Open()` / `Process()` / `Close()`
- **ConfigValidator**: Configuration validation in `src/framework/config/config_validator.h`
- **NodeContract**: Type contract declarations on each node

## Success Criteria

- **SC-001**: All 4 stub methods (OutputStreamCallback, InputSidePacket, OutputSidePacketCallback) implemented and tested
- **SC-002**: `WaitUntilDone()` works in async `Start()` path — demos use it instead of `Shutdown()`
- **SC-003**: `Pause()` / `Resume()` implemented and tested
- **SC-004**: `WaitForIdle()` / `HasGraphFinished()` / `GetGraphState()` exposed and tested
- **SC-005**: ConfigValidator detects connectivity and cycle errors
- **SC-006**: `bazel build //...` and `bazel test //...` pass with zero warnings
- **SC-007**: All existing tests continue to pass
