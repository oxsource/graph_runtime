# Feature Specification: Stream Input/Output API

**Created**: 2026-07-24
**Status**: Draft

**Input**: 实现 `GraphRuntime::AddPacketToInputStream` 和 `CloseInputStream`，支持运行时向图投递数据包和关闭流

## User Scenarios & Testing

### User Story 1 - Add Packet to InputStream (Priority: P1)

As a library consumer, I want to push packets into a named input stream of a running graph, so that I can feed data into the pipeline incrementally (e.g., frame-by-frame from a camera).

**Why this priority**: This is the core missing feature — external data injection into a running graph.

**Independent Test**: Build a simple pipeline with a consumer node, call `AddPacketToInputStream` with a packet, verify the consumer receives it.

**Acceptance Scenarios**:
1. **Given** a graph with a non-source node that has an input stream, **When** `Start()` is called followed by `AddPacketToInputStream("stream", pkt)`, **Then** the node's `Process()` is invoked with the packet
2. **Given** a packet added to an input stream, **When** the node processes it, **Then** the node's output is propagated to downstream nodes
3. **Given** multiple packets added to the same stream, **When** processed, **Then** each packet is delivered in timestamp order

---

### User Story 2 - Close InputStream (Priority: P1)

As a library consumer, I want to signal that no more packets will be sent on a stream, so that the graph can drain and terminate gracefully.

**Why this priority**: Without close signals, the graph can't determine when to stop waiting for input.

**Independent Test**: Call `CloseInputStream`, verify downstream nodes receive `StatusStop()` and the graph terminates.

**Acceptance Scenarios**:
1. **Given** a running graph with an open input stream, **When** `CloseInputStream("stream")` is called, **Then** the downstream node receives `StatusStop()` after processing remaining packets
2. **Given** all input streams are closed, **When** the pipeline drains, **Then** `WaitUntilDone()` returns

---

### User Story 3 - Graph Lifecycle Management (Priority: P2)

As a library consumer, I want a clear lifecycle: `Initialize()` → `Start()` → `AddPacket/CloseInputStream` → `WaitUntilDone()` → `Shutdown()`, so that I can control the graph execution flow.

**Why this priority**: The current synchronous `Schedule()` doesn't support incremental packet injection.

**Independent Test**: Call lifecycle methods in order and verify each state transition succeeds.

**Acceptance Scenarios**:
1. **Given** a valid config, **When** `Initialize()` → `Start()` → `Shutdown()` is called, **Then** the graph transitions through states without error
2. **Given** a running graph, **When** `AddPacketToInputStream` is called before `Start()`, **Then** it returns `FailedPreconditionError`

---

### Edge Cases

- `AddPacketToInputStream` on unknown stream name → `NotFoundError`
- `AddPacketToInputStream` after `CloseInputStream` → `FailedPreconditionError`
- Concurrent `AddPacketToInputStream` calls from multiple threads → thread-safe, no data loss
- Backpressure: if downstream queues fill up, `AddPacketToInputStream` should block or return `UnavailableError`
- Packet with timestamp earlier than last processed → unordered error handling

## Requirements

### Functional Requirements

- **FR-001**: `GraphRuntime::AddPacketToInputStream(stream_name, packet)` MUST deliver the packet to the node owning the named input stream
- **FR-002**: `GraphRuntime::CloseInputStream(stream_name)` MUST signal end-of-stream, causing downstream nodes to receive `StatusStop()`
- **FR-003**: The scheduler MUST support an asynchronous mode with `Start()` + `WaitUntilDone()` + `WaitUntilIdle()` lifecycle
- **FR-004**: `OutputStreamManager::PropagateUpdatesToMirrors` MUST actually propagate packets to downstream `InputStreamManager` queues
- **FR-005**: `InputStreamHandler::FillInputSet` MUST correctly populate `InputStreamShard` from the input stream queue
- **FR-006**: `Node` MUST have a `ProcessNode(GraphContext&)` wrapper that calls `Open()`/`Process()`/`Close()` with proper input/output shard wiring
- **FR-007**: Backpressure MUST be supported — if a downstream queue exceeds capacity, upstream sources/graph-inputs are throttled
- **FR-008**: All stream operations MUST be thread-safe
- **FR-009**: `GraphBuilder` MUST wire stream mirrors during construction (connect output managers to downstream input handlers)
- **FR-010**: `bazel test //...` MUST pass after implementation

### Key Entities

- **GraphInputStream**: Virtual source that injects external packets into the graph, wrapped as `OutputStreamShard` + `OutputStreamManager`
- **InputStreamManager**: Thread-safe per-stream packet queue with arrival callback
- **OutputStreamManager**: Manages mirrors (downstream inputs); `PropagateUpdatesToMirrors` pushes packets
- **InputStreamHandler**: Determines node readiness; `FillInputSet` populates per-invocation shards
- **OutputStreamHandler**: Post-processes node output; calls `PropagateUpdatesToMirrors`
- **Scheduler (async)**: Manages source activation, idle detection, node throttling
- **GraphContext**: Per-invocation context carrying `InputStreamShardSet` and `OutputStreamShardSet`

## Success Criteria

- **SC-001**: `AddPacketToInputStream` delivers packet to the target node's `Process()`, confirmed by unit test
- **SC-002**: `CloseInputStream` propagates `StatusStop()` downstream, confirmed by integration test
- **SC-003**: `WaitUntilDone()` returns after all streams closed and packets processed
- **SC-004**: Concurrent `AddPacketToInputStream` from 8 threads produces no data races
- **SC-005**: `bazel test //...` passes (13 existing tests + new stream tests)
- **SC-006**: Backpressure works — excessive packets cause `UnavailableError` without crashing

## Assumptions

- MediaPipe's async scheduling model is the reference architecture
- The existing `InputStreamManager` implementation is correct and reusable
- The existing `SchedulerQueue` + `ThreadPoolExecutor` can be extended for async dispatch
- `GraphContext` can be extended to carry input/output shards
- Initial implementation targets a single-threaded async mode; multi-threaded executor comes later
