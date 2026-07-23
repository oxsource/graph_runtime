# Research: Scheduler-Node-Stream-Packet Module Interaction Design

## Phase 0 — Unknowns Resolution

### 1. Stream Back-Pressure Policy

**Decision**: Use a push-based model with bounded queue — Stream::Push() returns false (or blocks) when the queue reaches max capacity, and the Scheduler checks queue state before activating upstream Nodes.

**Rationale**:
- Phase 1 is single-threaded, so blocking Push() would deadlock. Instead, Push() returns a status indicating back-pressure.
- The Scheduler tracks each Stream's fullness and avoids scheduling upstream Nodes when downstream Streams are full.
- This is simpler than pull-based models and sufficient for Phase 1 pipeline graphs.

**Alternatives considered**:
- Pull-based (downstream requests data): More flexible but adds complexity; deferred to Phase 2.
- Unbounded queue: Risk of OOM; rejected for production safety.

### 2. Scheduler Activation Model

**Decision**: Event-driven activation via Stream event observers. Stream::Push() triggers downstream readiness evaluation through InputStreamHandler, which may schedule the Node via Executor.

**Rationale**:
- No central polling loop — the system is purely reactive, matching MediaPipe's architecture.
- Schedule() is non-blocking: installs event observers and returns immediately. WaitUntilDone() blocks for completion.
- Scales naturally to multi-threaded execution (Phase 2) — events are just task schedules on an Executor.
- InputStreamHandler encapsulates the readiness policy, keeping the Scheduler focused on orchestration.

**Alternatives considered**:
- Central ready queue with polling loop (queue-driven): Rejected — creates a bottleneck, couples scheduling policy to Scheduler, and conflicts with multi-thread scalability.
- Topological walk on each event: O(N) per activation; rejected as too slow for 50+ Node graphs.

### 3. Node Lifecycle & Blocking Concerns

**Decision**: Node lifecycle methods MUST return promptly (non-blocking). Long-running work is the caller's responsibility to partition into multiple Process() calls.

**Rationale**:
- Single-threaded scheduler cannot preempt a blocking Node — the entire graph would stall.
- If a Node has long-running work, it should process one unit per Process() call and the Scheduler will re-invoke it when inputs are replenished.
- This is consistent with MediaPipe's Calculator model where each Process() call handles one packet.

**Alternatives considered**:
- Thread-per-Node: Rejected for Phase 1 (adds concurrency complexity, violates single-thread constraint).
- Async/coroutine model: Deferred to Phase 2.

### 4. Packet Ownership Model

**Decision**: Packet uses value semantics (copyable/movable) with a type-erased payload via `std::any`. Stream::Pop() returns the Packet by value, transferring ownership to the consuming Node.

**Rationale**:
- Value semantics avoid shared ownership complexity (no shared_ptr, no reference counting).
- For small payloads (< 64 bytes), stack allocation is efficient.
- For large payloads, callers can wrap in shared_ptr within the Packet payload.
- std::any provides type erasure without template bloat.

**Alternatives considered**:
- unique_ptr<Packet>: Adds heap allocation per packet; acceptable for Phase 1 but value semantics preferred.
- Zero-copy via shared memory: Over-engineering for single-process Phase 1; deferred to Phase 2.

### 5. Error Propagation Strategy

**Decision**: Fail-fast — any Node Process() error causes the Scheduler to abort the entire graph, call Close() on all opened Nodes, and propagate the error.

**Rationale**:
- Simplicity: No error recovery logic needed in Phase 1 Scheduler or Nodes.
- Predictability: Graph either succeeds or fails atomically.
- Errors are exceptional in the target use case (vision pipelines); retry logic can be added in Phase 2 if needed.

**Alternatives considered**:
- Skip-and-continue: Would require per-Node error configuration; deferred.
- Retry logic: Adds state complexity; not justified for Phase 1 failure rates.

### 6. Input Readiness Strategy

**Decision**: Pluggable `InputStreamHandler` interface with built-in strategies.

**Rationale**:
- Different pipeline topologies need different readiness semantics (all-inputs vs any-input vs barrier sync).
- Separating readiness from the Scheduler event loop follows MediaPipe's proven architecture and keeps the Scheduler focused on orchestration.
- Phase 1 default: `DefaultInputStreamHandler` (all inputs ready → process). Safe for most pipelines.

### 7. Task Execution Strategy

**Decision**: Pluggable `Executor` interface. Phase 1 default: `ApplicationThreadExecutor` (synchronous, single-threaded).

**Rationale**:
- Single-threaded execution is sufficient for Phase 1 scope (<50 Nodes, vision pipelines).
- The Executor abstraction allows future multi-threaded execution without changing Scheduler logic.
- ThreadPoolExecutor can be introduced in Phase 2 as a drop-in replacement.

### 8. Execution Order Guarantee

**Decision**: No explicit priority enumeration. Execution order is guaranteed by event timing — Open is only scheduled during initialization, Process is triggered by data arrival, Close is triggered by stream exhaustion. These three phases cannot collide because they occur in mutually exclusive time windows.

**Rationale**:
- In an event-driven model, lifecycle stages are naturally sequential: Init → Data flow → Teardown.
- Open, Process, and Close are never schedulable simultaneously — they depend on disjoint preconditions.
- MediaPipe does not expose a priority enum; its scheduler relies on the same event-timing guarantee.

### 9. Stream Management Layer — InputStreamManager / OutputStream / OutputStreamHandler

**Decision**: Split Stream management into four responsibilities:
- `Stream` — Low-level 1:1 data pipe (Push/Pop/Close, bounded queue, back-pressure).
- `InputStreamManager` — Per-input-port: notification callback on packet arrival + timestamp bound tracking.
- `OutputStream` — Per-output-port: fan-out to N downstream Streams via `Send()`.
- `OutputStreamHandler` — Per-Node: post-process orchestration (`PostProcess()` after each Process call, `Flush()` before Close).

**Rationale**:
- Matches MediaPipe's separation between `InputStreamManager` (data plane + notification) and `InputStreamHandler` (policy plane).
- Fan-out support is essential for non-trivial graph topologies — one Node output can feed multiple downstream Nodes.
- Separating `OutputStreamHandler` from the Scheduler event loop keeps both components testable in isolation.
- Timestamp bound tracking at the `InputStreamManager` level enables `ProcessTimestampBounds` mode (Phase 2) without modifying the Stream core.

**MediaPipe reference**:
- `InputStreamManager` wraps `InputStream` with notification callback and `MinTimestampOrBound()`.
- `OutputStreamHandler::PostProcess()` corresponds to `CalculatorNode::ProcessNode()`'s output shard processing.
- Fan-out is handled internally by `OutputStreamManager::PropagateUpdatesToMirrors()`.

### 10. Scheduler Completeness — State Machine, Stopping, Idle Tracking

**Decision**: Implement full state machine (5 states), `stopping_` flag for rapid pipeline teardown, `non_idle_queue_count_` for idle aggregation, and `handling_idle_` reentrancy guard.

**Rationale**:
- State machine enables pause/resume lifecycle, which is required for interactive applications and testing.
- `stopping_` flag mirrors MediaPipe's behavior: a single `StatusStop` from any non-source preemptively closes all sources, providing clean rapid shutdown.
- `non_idle_queue_count_` aggregation with reentrancy guard prevents both missed idle signals and stack overflow from recursive idle handlers.
- Source Nodes reuse a single default GraphContext — enforced by design since `max_in_flight` is always 1 for sources.

### 11. Source Layering (Deferred to Phase 2)

**Decision**: `source_layer` field on Source Nodes (default 0). All sources in layer N must close before layer N+1 sources activate. Not enforced in Phase 1 — all sources activate concurrently.

**Rationale**:
- Enables ordered source startup for multi-source pipelines (e.g., camera then audio sync).
- Design is reserved in the data model; Phase 1 simply ignores the field (all sources treated as layer 0).
- Phase 2 implementation: Scheduler tracks `active_layer_`, promotes on source Close, schedules next layer's Open.

**Phase 1 behavior**: `source_layer` field exists but is not consulted. All sources activate together.

### 10. Back-pressure: Auto-Unthrottle

**Decision**: Push failure triggers throttle; consumer Pop below `max_queue_size / 2` triggers auto-unthrottle. `HandleIdle()` detects deadlock and unthrottles all.

**Rationale**:
- Prevents unbounded memory growth without manual tuning.
- Auto-unthrottle on idle detection prevents deadlocks in cyclic/diamond topologies.
- Half-threshold provides hysteresis, preventing rapid throttle/unthrottle oscillation.

## Technology Choices Summary

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Stream back-pressure | Bounded queue, push-based, status return | Simple, safe for single-thread |
| Activation model | Event-driven: Stream events → InputStreamHandler → Executor | No central polling, reactive, scales to multi-thread |
| Schedule() mode | Non-blocking — installs event observers, returns | WaitUntilDone() blocks for completion |
| Node blocking | Non-blocking required | Single-thread constraint |
| Packet ownership | Value semantics via std::any | Simple ownership, no shared_ptr |
| Error propagation | Fail-fast, abort graph | Predictable, simple |
| Scheduler topology | Layer-based topological order | Matches Phase 1 simplicity |
| Input readiness | DefaultInputStreamHandler (all-inputs barrier) | Safe default; pluggable via interface |
| Task execution | ApplicationThreadExecutor (sync) | Phase 1 default; pluggable via interface |
| Execution order | Event-timed (Init → Data → Teardown) | Natural lifecycle sequencing, no priority enum |
| Stream notification | InputStreamMgr (arrival cb + bound) | Decouples data plane from readiness policy |
| Output fan-out | OutputStream (N downstream) + OutputStreamHandler (PostProcess) | Supports one-to-many graph topologies |
| Scheduler state | 5-state machine + stopping_ + non_idle_count_ + reentrancy guard | Pause/resume, clean shutdown, safe HandleIdle |
| Source context | Single default GraphContext reused | max_in_flight always 1 for sources |
| Source layering | source_layer field (Phase 2) | Ordered multi-source startup; Phase 1: all sources concurrent |
| Back-pressure recovery | Auto-unthrottle on Pop + HandleIdle deadlock break | Prevents deadlock without manual tuning |
