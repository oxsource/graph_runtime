# Data Model: Scheduler-Node-Stream-Packet

## Entities

### Timestamp

Timestamp class with special values encoded in the int64_t range extremes.

| Field | Type | Description |
|-------|------|-------------|
| `timestamp_` | `int64_t` | Raw timestamp value; special values at extremes |

**Special values**:

| Constant | Internal Value | Semantics |
|----------|---------------|-----------|
| `Unset()` | `INT64_MIN` | Default-constructed Packet, not valid for stream use |
| `Unstarted()` | `INT64_MIN + 1` | Input timestamp during `Open()` |
| `PreStream()` | `INT64_MIN + 2` | Header data (sole packet on stream) |
| `Min()` | `INT64_MIN + 3` | Minimum range timestamp |
| `Max()` | `INT64_MAX - 3` | Maximum range timestamp |
| `PostStream()` | `INT64_MAX - 2` | Stream summary (sole packet on stream) |
| `OneOverPostStream()` | `INT64_MAX - 1` | Internal use only |
| `Done()` | `INT64_MAX` | Input during `Close()`; end-of-stream signal |

**Validation rules**:
- `int64_t` constructor CHECK-fails if called with a special value.
- End-of-stream is detected by `timestamp() == Timestamp::Done()`, not by a separate boolean.
- `IsEmpty()` returns true only for `Unset()`.

---

### Packet

Atomic data unit flowing through the graph.

| Field | Type | Description |
|-------|------|-------------|
| `holder_` | `shared_ptr<HolderBase>` | Type-erased payload (custom Holder<T> or std::any) |
| `timestamp_` | `Timestamp` | Monotonic timestamp for ordering, plus lifecycle signalling |

**Validation rules**:
- Timestamp MUST be monotonically non-decreasing within a single Stream.
- Payload type is checked via `ValidateAsType<T>()` or `Get<T>()` at read time.
- End-of-stream is signaled by `Timestamp::Done()`, not by `is_empty`.
- `IsEmpty()` returns true when `holder_ == nullptr` (default-constructed or moved-from).

**Ownership**: Shallow-copy semantics — copy increments shared_ptr refcount (O(1)). Source is never deep-copied.

---

### Stream

Unidirectional data conduit between a single Node output port and a single Node input port.

| Field | Type | Description |
|-------|------|-------------|
| `name` | `std::string` | Unique identifier within the Graph |
| `queue_` | bounded queue of Packet | Internal packet buffer |
| `max_queue_size_` | `size_t` | Back-pressure threshold — Push fails when full |
| `is_closed_` | `bool` | Stream lifecycle state |

**States**:
```
Open ──► Active (first packet enqueued) ──► Closed (Close() called, Done() packet enqueued)
```

**Boundaries**:
- Owned by Graph, referenced by Node via pointer.
- One producer Node (output port), one consumer Node (input port).
- Stream does NOT know which Nodes are connected to it — that is Graph's responsibility.

**Validation rules**:
- `Push()` returns error status if queue is full (back-pressure).
- `Pop()` returns error status if queue is empty.
- `Close()` pushes a Packet with `Timestamp::Done()`; subsequent `Push()` calls fail.
- After `Close()`, `Pop()` continues until queue is drained, then returns `OutOfRangeError`.
- Consumer detects end-of-stream by checking `popped_packet.timestamp().IsDone()`. No separate boolean needed.

---

### Node

Computational unit with lifecycle, bound to input and output Streams.

| Field | Type | Description |
|-------|------|-------------|
| `name` | `std::string` | Unique identifier within the Graph |
| `input_ports_` | `map<string, Stream*>` | Named input Stream references |
| `output_ports_` | `map<string, Stream*>` | Named output Stream references |

**Lifecycle**:
```
Created ──Open()──► Opened ──Process()──► Processing ──► Opened (loop)
                         │                                       │
                         └──Close()──► Closed ◄──────────────────┘
```

**Boundaries**:
- Node MUST NOT reference other Nodes directly.
- Node MUST NOT own Streams — Streams are owned by Graph.
- Node contains NO scheduling logic — that is Scheduler's responsibility.
- Node contains NO configuration parsing — that is Config's responsibility.

**Validation rules**:
- `Open()` MUST be called exactly once before any `Process()` call.
- `Process()` MUST NOT be called after `Close()`.
- `Close()` MUST be idempotent — calling again on an already-closed Node is a no-op (returns `OkStatus()`). [fix C]
- A Node with zero input Streams (source) is activated by the Scheduler on graph start.
- A Node with zero output Streams (sink) has its `Process()` result discarded.

---

### GraphContext

Per-invocation context passed to Node lifecycle methods.

| Field | Type | Description |
|-------|------|-------------|
| `inputs` | `map<string, Packet&>` | Named input Packets for this invocation |
| `outputs` | `map<string, PacketProducer>` | Named output producers for this invocation |
| `options` | `const NodeOptions&` | Node configuration from graph config |

**Boundaries**:
- Valid only during the scope of the current lifecycle call.
- Created by Scheduler before each Open/Process/Close call.
- Inputs are pre-populated by Scheduler from `Stream::Pop()`.
- Outputs are collected by Scheduler and pushed to output Streams after `Process()` returns.

---

### Scheduler

Execution engine with state machine, pluggable input policies, and task execution.

| Field | Type | Description |
|-------|------|-------------|
| `state_` | `atomic<SchedulerState>` | kNotStarted → kRunning ↔ kPaused → kCancelling → kTerminated |
| `stopping_` | `bool` | Set when a non-source returns StatusStop or error — sources skip Process and Close directly |
| `error_callback_` | `ErrorCallback` | Invoked when Node returns non-OK, non-Stop status; sets HasError and initiates kCancelling. HandleIdle waits for non_idle_queue_count_==0 before kTerminated |
| `non_idle_queue_count_` | `int` | Tracks active Executor queues; HandleIdle fires when reaches 0 |
| `handling_idle_` | `int` | Reentrancy guard counter for HandleIdle (int, not bool) |
| `active_sources_` | `set<Node*>` | Currently open, running source Nodes (for stopping_ cleanup) |
| `stream_to_input_mgr_` | `map<Stream*, InputStreamManager*>` | Pre-built mapping; task runner calls OnPacketEnqueued directly after Push — no callback on Stream |
| `input_stream_handler_` | `unique_ptr<InputStreamHandler>` | Pluggable readiness policy |
| `executor_` | `shared_ptr<Executor>` | Task execution strategy |
| `throttled_sources_` | `set<Node*>` | Source Nodes paused by back-pressure |
| `source_layers_` | `map<int, vector<Node*>>` | Source Nodes grouped by activation layer (Phase 2 — unused in Phase 1) |
| `active_layer_` | `int` | Currently active source layer index (Phase 2 — always 0 in Phase 1) |

**State machine**:
```
kNotStarted ──Schedule()──► kRunning ──Pause()──► kPaused
                                ▲                   │
                                └────Resume()───────┘
                                  │
                     Shutdown()/error──► kCancelling ──► kTerminated
                                  │
                 All closed ──────┴──────► kTerminated
```

**Boundaries**:
- Owned by Graph, which calls `Schedule()`, `Pause()`, `Resume()`, `WaitUntilDone()`, and `Shutdown()`.
- Scheduler does NOT own Nodes, Streams, InputStreamManagers, or OutputStreams — Graph does.
- Scheduler does NOT know about graph topology — that is Graph's responsibility.
- Readiness logic is delegated to `InputStreamHandler`; execution is delegated to `Executor`.
- Scheduler does NOT have a central polling loop — it reacts to Stream/Node events via registered observers.

**Validation rules**:
- `Schedule()` MUST be called after `Graph::Initialize()`. It is NON-BLOCKING — returns immediately after setting up event observers.
- `WaitUntilDone()` MUST be called after `Schedule()` to block until termination.
- `Pause()`/`Resume()` are only valid when state is `kRunning`. `Pause()` returns `FailedPreconditionError` if `stopping_ == true`.
- `Shutdown()` MUST be callable from any state.
- `AddNode()`/`RemoveNode()` return `UnimplementedError` in Phase 1.
- `InputStreamHandler`, `Executor`, and `ErrorCallback` MUST be set before `Schedule()` if custom implementations are used.
- Source Nodes reuse a single default `GraphContext` (max_in_flight is always 1 for sources).
- After setting `stopping_ = true`, all active sources in `active_sources_` are immediately scheduled for CloseNode.

---

### InputStreamManager

Per-input-port wrapper adding notification and timestamp tracking on top of Stream.

| Field | Type | Description |
|-------|------|-------------|
| `stream_` | `Stream*` | The underlying 1:1 data pipe (consumer side) |
| `arrival_callback_` | `PacketArrivalCallback` | Fires when a new Packet is enqueued |
| `min_timestamp_bound_` | `int64_t` | Smallest unsettled timestamp for readiness calculation |

**Boundaries**:
- One per Node input port, created during Scheduler initialization.
- `arrival_callback_` is set by the Scheduler and typically triggers `InputStreamHandler::NotifyPacketArrival()`.
- `MinTimestampOrBound()` is used by `InputStreamHandler::GetNodeReadiness()` to determine timestamp settlement.

---

### OutputStream

Output port abstraction supporting fan-out to multiple downstream Streams.

| Field | Type | Description |
|-------|------|-------------|
| `name_` | `std::string` | Port name matching the GraphConfig |
| `downstream_streams_` | `vector<Stream*>` | All downstream Streams this port writes to |

**Boundaries**:
- One per Node output port, created during Scheduler initialization.
- Downward connections are established by GraphBuilder during graph construction.
- `Send(Packet)` writes to every downstream Stream; first error is returned.
- Fan-in (multiple upstream → one downstream) is handled by separate OutputStreams writing to the same Stream.

---

### OutputStreamHandler

Per-Node handler for output propagation after Process completes.

| Method | Description |
|--------|-------------|
| `PostProcess(GraphContext&)` | Collect outputs, call OutputStream::Send(), propagate bounds |
| `Flush()` | Flush buffered outputs before Close |

**Boundaries**:
- One per Node, created during Scheduler initialization.
- Called by Scheduler's task runner after `Node::Process()`.
- Complements `InputStreamHandler` on the output side — both follow the same pluggability pattern.

---

### InputStreamHandler

Pluggable strategy determining when a Node is ready to execute.

| Method | Description |
|--------|-------------|
| `GetNodeReadiness(Node&) -> Readiness` | Returns kNotReady, kReadyForProcess, or kReadyForClose |
| `FillInputSet(Node&, GraphContext&)` | Pops input Packets into GraphContext.inputs |
| `NotifyPacketArrival(Node&)` | Called when new data arrives on an input Stream |

**Built-in strategies**:
- **DefaultInputStreamHandler**: Ready when all input Streams have ≥1 Packet. Pops one per stream. Default for Phase 1.
- **ImmediateInputStreamHandler**: Ready when any input Stream has a Packet. Non-monotonic timestamps possible.
- **BarrierInputStreamHandler**: Ready when all streams have a Packet at the same timestamp.

---

### Executor

Pluggable strategy for running scheduler tasks.

| Method | Description |
|--------|-------------|
| `ScheduleTask(function<void()>)` | Enqueue a task for execution |

**Built-in implementations**:
- **ApplicationThreadExecutor**: Synchronous, on-calling-thread execution. Default for Phase 1.
- **ThreadPoolExecutor**: Multi-threaded thread pool. Configurable thread count. Phase 2.

---

### NodeFactory

Registry for creating Node instances by type name.

| Method | Description |
|--------|-------------|
| `Register(type_name, factory_fn)` | Register a Node type for creation |
| `Create(type_name) -> unique_ptr<Node>` | Create a Node instance by type name |

**Boundaries**:
- Used by GraphBuilder during graph construction (not during execution).
- Registry is global — all Node types must be registered before graph construction.
- Default Node types (for the string pipeline example) are registered during library init.

---

## Relationships

```
GraphBuilder  ──uses NodeFactory──►  Node  ──served by──►  InputStreamManager (per input port)
                                                           │  wraps Stream (consumer side)
                                                           │  fires arrival_callback → NotifyPacketArrival
                                                           ▼
       ┌──────────────────────────────────────────┐
       │  Scheduler                               │
       │  ● state machine (kRunning/kPaused/...)   │
       │  ● stopping_ flag (StatusStop shortcut)   │
       │  ● HandleIdle with reentrancy guard       │
       │  ● non_idle_queue_count_ aggregation      │
       │  ● output_stream_handler_ + input_handler │
       └────┬──────────────────────────────┬───────┘
            │                              │
            ▼                              ▼
     InputStreamHandler              OutputStreamHandler
     ● GetNodeReadiness              ● PostProcess()
     ● FillInputSet                  ● Flush()
            │                              │
            ▼                              ▼
  InputStreamManager (N)           OutputStream (M)
       │  wraps Stream                    │  fan-out to 1..N Streams
       │  arrival_callback                │  Send() → Stream::Push()
       ▼                                  ▼
     Stream  ──carries──►  Packet      Stream
       (consumer side)                    (producer side)
```

## Data Flow Sequence (Event-Driven)

Schedule() is NON-BLOCKING — it sets up event observers and returns immediately. All execution is driven by data/state events that propagate through the system.

```
 1. Graph::Initialize()
    │  Creates Nodes via NodeFactory
    │  Creates Streams between Node ports
    │  Creates Scheduler with InputStreamHandler + Executor
    ▼
 2. Graph::Start()
    │
    ▼
 3. Scheduler::Schedule(graph) — NON-BLOCKING, returns immediately
    │  state_ → kRunning
    │
    ├── a. Build topological layers from Node/Stream wiring.
    │
    ├── b. Group Source Nodes by source_layer (Phase 2 — in Phase 1 all sources are layer 0 and activate together).
    │
    ├── c. For each Node input port: create InputStreamManager wrapping Stream consumer side.
    │    Set arrival_callback → InputStreamHandler::NotifyPacketArrival().
    │
    ├── d. Build stream_to_input_mgr_ mapping.
    │
    ├── e. For each Node output port: create OutputStream with downstream Stream list.
    │
    ├── f. For each Node: create OutputStreamHandler.
    │
    ├── g. Register event observers:
    │    • InputStreamManager::OnPacketEnqueued → NotifyPacketArrival
    │    • Stream::Close() → mark input done
    │    • Node::Close() → OnNodeClosed → completion check + [Ph2: layer advance]
    │
    └── h. Activate all Source Nodes:
         Executor::ScheduleTask([OpenNode])
    ▼
 4. [EVENT-DRIVEN — no central loop; reactions to events]

    ─── Event: OpenNode task runs ──────────────────────────────
    │  Node::Open(context)
    │  After Open → Scheduler fires OnNodeOpened(node):
    │    if Source:                                        [fix #2]
    │      add to active_sources_
    │      schedule initial ProcessNode via Executor.ScheduleTask  [fix #2]
    │    if non-source:
    │      register InputStreamManagers → NotifyPacketArrival if buffered
    │
    ─── Event: ProcessNode task runs ──────────────────────────
    │  [if stopping_=true AND node is Source]   [fix #3]
    │    → schedule CloseNode, skip Process, return
    │
    │  // Back-pressure unthrottle (side effect of FillInputSet)  [fix #5]
    │  InputStreamHandler::FillInputSet(node, context)
    │    → for each input: InputStreamManager::Pop()
    │    → if queue < max_queue_size/2 AND upstream throttled:
    │      → unthrottle upstream → NotifyPacketArrival(upstream)
    │
    │  absl::Status status = Node::Process(context)
    │
    │  // Error handling   [fix #4]
    │  if (!status.ok() && !IsStopStatus(status)):
    │    error_callback_(status)
    │    → HasError = true, stopping_ = true
    │    → schedule CloseNode for all active_sources_
    │    → HandleIdle()
    │    return
    │
    │  if (IsStopStatus(status)):   [stopping + source closure]
    │    stopping_ = true
    │    schedule CloseNode for all active_sources_   [fix #3]
    │    return
    │
    │  // Output propagation  [fix #1 — no Stream callback]
    │  OutputStreamHandler::PostProcess(context)
    │    for each output port:
    │      for each downstream Stream:
    │        status = OutputStream::Send(packet) → Stream::Push(packet)
    │        if (status.ok()):
    │          auto* input_mgr = stream_to_input_mgr_[stream]  // pre-built
    │          input_mgr->OnPacketEnqueued()   // direct call on downstream
    │          → arrival_callback_ → InputStreamHandler::NotifyPacketArrival(node)
    │        elif (status == ResourceExhausted):
    │          → throttle upstream source
    │
    │  // Source rescheduling (loop)
    │  if (node is Source && !stopping_):
    │    Executor::ScheduleTask([ProcessNode])
    │
    ─── Event: Non-source returns StatusStop ───────────────────
    │  stopping_ = true
    │  schedule CloseNode for all active_sources_    [fix #3]
    │
    ─── Event: Node::Close() completes ─────────────────────────
    │  Scheduler fires OnNodeClosed(node):
    │    if Source: remove from active_sources_
    │    [Phase 2: if Source → advance source_layer, activate next layer]
    │    Check graph completion:
    │      if active_sources_.empty()
    │         AND all non-sources closed
    │         AND non_idle_queue_count_ == 0:
    │        → state_ → kTerminated
    │        → signal WaitUntilDone()
    │
    ─── Event: Stream closed (end-of-stream) ─────────────────  [fix #5]
    │  Mark input port as done on consuming Node
    │  InputStreamHandler::NotifyPacketArrival(consumer)
    │    → GetNodeReadiness may return kReadyForClose
    │
    ─── Event: HandleIdle (called from Schedule/Resume/error/non_idle=0) ─  [fix #7]
    │  [handling_idle_++, reentrancy guard]
    │  if HasError && non_idle_queue_count_ == 0:   [fix A]
    │    Quit → state_ = kTerminated
    │  CleanupActiveSources()
    │  if all sources done AND all inputs closed AND no pending tasks:
    │    Quit → state_ = kTerminated
    │  if throttled active: unthrottle (deadlock break)
    │  [handling_idle_--]
    │
  5. Graph::WaitUntilDone() returns (state_ == kTerminated)
```

## Module Responsibility Matrix

| Concern | Packet | Stream | InputStreamMgr | OutputStream | OutputStreamHandler | Node | GraphCtx | Scheduler | InputStreamHandler | Executor | NodeFactory |
|---------|--------|--------|----------------|--------------|-------------------|------|----------|-----------|-------------------|----------|-------------|
| Data transport | Carrier | 1:1 pipe | — | Fan-out | — | — | — | — | — | — | — |
| Notification | — | — | Arrival callback | — | — | — | — | — | — | — | — |
| Timestamp bound | — | — | Track bound | Propagate bound | — | — | — | — | Consume bound | — | — |
| Business logic | — | — | — | — | — | Execute | — | — | — | — | — |
| Activation scheduling | — | — | — | — | — | — | — | Orchestrate | — | Dispatch | — |
| Readiness policy | — | — | — | — | — | — | — | — | Decide | — | — |
| Output post-process | — | — | — | — | PostProcess | — | — | — | — | — | — |
| Back-pressure | — | Enforce | — | — | — | — | — | Throttle/Unthrottle | — | — | — |
| Source layering (Ph2) | — | — | — | — | — | Declare layer | — | Sequence layers | — | — | — |
| Completion tracking | — | — | — | — | — | — | — | Detect all closed | — | — | — |
| Type erasure | Provide | — | — | — | — | — | — | — | — | — | — |
| Lifecycle | — | Closed signal | — | — | Flush | Open/Process/Close | Per-invoke bag | State machine + stopping | Detect kReadyForClose | — | — |
| Node creation | — | — | — | — | — | — | — | — | — | — | Register/Create |
| Error propagation | — | — | — | — | — | Return error | — | Abort graph + stopping_ | — | — | — |
