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

### User Story 6 — Tag/Index Stream Support (Priority: P1)

As a library user, I want input and output streams to support MediaPipe-compatible `TAG:index` notation, so that I can address multiple streams of the same logical type by tag and index.

**Why this priority**: Required for interoperability with MediaPipe graph definitions and multi-stream calculators (e.g., stereo video, multi-channel audio).

**Independent Test**: A test configures a node with `{"VIDEO:0", "VIDEO:1"}` as input streams, injects packets to each index separately via `AddPacketToInputStream("VIDEO", 0, pkt)` and `AddPacketToInputStream("VIDEO:1", pkt)`, and verifies each index receives the correct packet.

**Acceptance Scenarios**:
1. **Given** a config where input_streams contains `"VIDEO:0"`, **When** the graph builds, **Then** a TagMap is created indexing streams by tag
2. **Given** `SetInputSidePacket("CONFIG:0", packet)` called before Start, **When** a node's Open() reads its side packet by `InputSidePackets().Get("CONFIG", 0)`, **Then** the packet is available
3. **Given** a graph with `output_streams: ["OUTPUT:0", "OUTPUT:1"]`, **When** a node writes to `ctx.Outputs().Get("OUTPUT", 1)`, **Then** the packet arrives on the correct indexed output
4. **Given** `GetGraphState()` exposed, **When** `AddPacketToInputStream("DATA", 5, pkt)` is called, **Then** the packet arrives at the correct index
5. **Given** `ParseTagIndexName("VIDEO:2:left_cam")`, **When** parsed, **Then** result is `{tag="VIDEO", index=2, name="left_cam"}`

---

### User Story 7 — Runtime Engine Hardening (Priority: P1)

As a library user, I want the runtime to properly handle InputStreamHandler strategies, MaxInFlight constraints, GraphContext pooling, batch scheduling, performance counters, and graceful cancellation, so that the graph execution matches MediaPipe's robustness and performance characteristics.

**Why this priority**: These are declared stubs or missing features that affect correctness and performance in production scenarios.

**Independent Test**: A test verifies that MaxInFlight limits concurrent node executions; a test verifies that GraphContexts are pooled and recycled; a test verifies that Cancel() drains pending work gracefully.

**Acceptance Scenarios**:

**InputStreamHandler Strategies:**
1. **Given** a node with `input_stream_handler: "immediate"`, **When** a packet arrives on any input, **Then** the node is scheduled immediately without waiting for other inputs
2. **Given** a node with `input_stream_handler: "sync_set"`, **When** packets arrive on multiple inputs, **Then** the node is scheduled only when all inputs have packets at the same timestamp
3. **Given** a node with `input_stream_handler: "fixed_size"` and max_queue_size set, **When** packets arrive, **Then** only up to max_queue_size packets are queued per input

**MaxInFlight:**
4. **Given** a node with `max_in_flight: 2`, **When** the node is scheduled twice concurrently, **Then** a third schedule attempt is deferred

**Context Pooling:**
5. **Given** a running graph, **When** a node's Process completes, **Then** its GraphContext is recycled via RecycleContext and reused

**Batch Scheduling:**
6. **Given** `ScheduleInvocations` with max_allowance, **When** multiple nodes are ready, **Then** up to max_allowance invocations are scheduled

**PerfCounters:**
7. **Given** a running graph, **When** nodes are processed, **Then** `tasks_submitted`, `tasks_completed`, `packets_processed`, `nodes_opened`, `nodes_closed` counters are non-zero

**Cancel:**
8. **Given** a running graph, **When** `Cancel()` is called, **Then** state transitions to `kCancelling`, pending work drains, and `WaitUntilDone()` returns

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
- **TagMap**: Tag/index mapping (new) in `src/framework/tool/tag_map.h` — maps `"VIDEO"` → indices `[0, 2)` for `VIDEO:0, VIDEO:1`

## Success Criteria

- **SC-001**: All 4 stub methods (OutputStreamCallback, InputSidePacket, OutputSidePacketCallback) implemented and tested
- **SC-002**: `WaitUntilDone()` works in async `Start()` path — demos use it instead of `Shutdown()`
- **SC-003**: `Pause()` / `Resume()` implemented and tested
- **SC-004**: `WaitForIdle()` / `HasGraphFinished()` / `GetGraphState()` exposed and tested
- **SC-005**: ConfigValidator detects connectivity and cycle errors
- **SC-006**: `bazel build //...` and `bazel test //...` pass with zero warnings
- **SC-007**: All existing tests continue to pass
- **SC-008**: Tag/index streams supported: ParseTagIndexName, TagMap, PacketTypeSet::Get(tag,index), AddPacketToInputStream(tag,index), AddPacketToInputStream("TAG:index")
- **SC-009**: Runtime hardened: Immediate/SyncSet/FixedSize InputStreamHandler, MaxInFlight enforcement, GraphContext pooling, ScheduleInvocations batch, PerfCounters wired, Cancel() implemented
