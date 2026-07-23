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



### Node

Computational unit with lifecycle, served by InputStreamManager (input) and OutputStream (output).

| Field | Type | Description |
|-------|------|-------------|
| `name` | `std::string` | Unique identifier within the Graph |
| `input_port_managers_` | `map<string, InputStreamManager*>` | Named input port references (deque + bound owner) |
| `output_streams_` | `map<string, OutputStream*>` | Named output port references (fan-out + mirrors) |

**Lifecycle**:
```
Created ──Open()──► Opened ──Process()──► Processing ──► Opened (loop)
                         │                                       │
                         └──Close()──► Closed ◄──────────────────┘
```

**Boundaries**:
- Node MUST NOT reference other Nodes directly.
- Node MUST NOT own InputStreamManagers or OutputStreams — these are owned by Graph.
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
- Inputs are pre-populated by Scheduler from `InputStreamManager::PopPacketAtTimestamp()`.
- Outputs are collected by Scheduler's task runner after `Process()` returns and sent via `OutputStream::Send()` directly to downstream `InputStreamManager` deques — no intermediate Stream.

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
- Scheduler does NOT own Nodes, InputStreamManagers, or OutputStreams — Graph does.
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

Owns the `std::deque<Packet>` for one downstream Node input port. Data is written directly by upstream `OutputStream::Send()` — no intermediate Stream component.

| Field | Type | Description |
|-------|------|-------------|
| `queue_` | `std::deque<Packet>` | Packet buffer, written by OutputStream mirrors, read by FillInputSet |
| `next_timestamp_bound_` | `Timestamp` | Smallest timestamp at which a packet may arrive; advances on Close() to Done |
| `closed_` | `bool` | true after Close(), which sets bound = Timestamp::Done() |
| `max_queue_size_` | `int` | Back-pressure threshold (-1 = unlimited) |
| `arrival_callback_` | `PacketArrivalCallback` | Fires when queue transitions from empty to non-empty |
| `becomes_full_callback_` / `becomes_not_full_callback_` | `QueueSizeCallback` | Trigger Scheduler throttle/unthrottle |

**Boundaries**:
- One per Node input port, created during Scheduler initialization.
- `arrival_callback_` is set by the Scheduler and triggers `InputStreamHandler::NotifyPacketArrival()`.
- `MinTimestampOrBound()` used by `InputStreamHandler::GetNodeReadiness()` to compute settled timestamp.
- `PopPacketAtTimestamp(ts)` is the primary consumer API — pops all packets <= ts.
- `IsDone()` returns true when `queue_.empty() && next_timestamp_bound_ == Timestamp::Done()`. No sentinel Done packet is pushed.
- Back-pressure: callbacks fire when queue crosses `max_queue_size_`, not by rejecting packets.

---

### OutputStream

Output port abstraction. Writes directly to downstream `InputStreamManager` deques via mirrors. No intermediate Stream.

| Field | Type | Description |
|-------|------|-------------|
| `name_` | `std::string` | Port name matching the GraphConfig |
| `mirrors_` | `vector<InputStreamManager*>` | All downstream managers this port writes to |

**Boundaries**:
- One per Node output port, created during Scheduler initialization.
- Mirrors are populated by GraphBuilder when wiring graph connectivity.
- `Send(Packet)` writes to each mirror via `InputStreamManager::AddPackets()` (copy) or `MovePackets()` (move, last mirror only).
- `Close()` sets `Timestamp::Done()` bound on all mirrors — no sentinel Done packet is pushed.
- Fan-in (multiple upstream → one downstream) is handled by multiple OutputStreams each writing to the same InputStreamManager.

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
GraphBuilder
  │  creates Nodes via NodeFactory
  │  creates InputStreamManager per input port (deque + bound + callbacks)
  │  creates OutputStream per output port (mirrors_ = list of downstream managers)
  │  wires OutputStream::mirrors_ → InputStreamManager* via graph topology
  ▼

Node (per instance)
  ├── input_ports_:  map<string, InputStreamManager*>
  │                     │
  │                     ├── queue_: std::deque<Packet>
  │                     ├── next_timestamp_bound_: Timestamp
  │                     ├── arrival_callback_ → NotifyPacketArrival
  │                     ├── becomes_full/not_full_callback_ → throttle/unthrottle
  │                     └── PopPacketAtTimestamp() / PopQueueHead()
  │
  └── output_ports_: map<string, OutputStream*>
                        │
                        └── mirrors_: vector<InputStreamManager*>  ← direct write
                            Send(packet) → AddPackets/MovePackets on each mirror
                            Close() → SetNextTimestampBound(Done) on each mirror

Scheduler
  ● state machine (kRunning/kPaused/...)
  ● stopping_ flag + error_callback_
  ● HandleIdle with reentrancy guard + non_idle_queue_count_
  ● InputStreamHandler → GetNodeReadiness → FillInputSet → PopPacketAtTimestamp
  ● task runner: Process → output propagation (inline) → OutputStream::Send()
```

## Data Flow Sequence (Event-Driven)

Schedule() is NON-BLOCKING — it sets up event observers and returns immediately. All execution is driven by data/state events that propagate through the system.

```
 1. Graph::Initialize()
    │  Creates Nodes via NodeFactory
    │  Creates InputStreamManager per input port (deque + bound + callbacks)
    │  Creates OutputStream per output port (mirrors_ set by GraphBuilder)
    │  Creates Scheduler with InputStreamHandler + Executor
    ▼
 2. Graph::Start()
    │
    ▼
 3. Scheduler::Schedule(graph) — NON-BLOCKING, returns immediately
    │  state_ → kRunning
    │
    ├── a. Build topological layers from Node wiring.
    │
    ├── b. Group Source Nodes by source_layer (Phase 2 — Phase 1: all concurrent).
    │
    ├── c. For each InputStreamManager:
    │    • SetArrivalCallback → InputStreamHandler::NotifyPacketArrival()
    │    • SetQueueSizeCallbacks → Scheduler throttle/unthrottle
    │
    ├── d. Event observers:
    │    • InputStreamManager::arrival_callback_ → NotifyPacketArrival
    │    • InputStreamManager::Close() → bound = Done → downstream detects via IsDone()
    │    • Node::Close() → OnNodeClosed → completion check
    │
    └── e. Activate all Source Nodes:
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
    │  // FillInputSet — pops from InputStreamManager deques
    │  InputStreamHandler::FillInputSet(node, context)
    │    → for each input: InputStreamManager::PopPacketAtTimestamp(ts)
    │    → side effect: PopPacketAtTimestamp may fire becomes_not_full_callback_
    │      → Scheduler unthrottles upstream source
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
    │  // Output propagation — inline, no separate OutputStreamHandler
    │  for each output port (from GraphContext::outputs):
    │    OutputStream::Send(packet)
    │      → for each mirror(InputStreamManager*):
    │        last mirror: MovePackets(packets)   // zero-copy
    │        others:      AddPackets(packets)    // shared_ptr bump
    │        → if queue was empty: arrival_callback_ fires
    │          → NotifyPacketArrival(downstream Node)
    │        → if crosses max_queue_size: becomes_full_callback_
    │          → Scheduler throttles upstream source
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
    ─── Event: InputStreamManager::Close() (bound=Done) ──────
    │  OutputStream::Close() → SetNextTimestampBound(Done) on all mirrors
    │  Downstream InputStreamManager::IsDone() becomes true when queue drains
    │  InputStreamHandler detects via GetNodeReadiness → may return kReadyForClose
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

| Concern | Packet | InputStreamMgr | OutputStream | Node | GraphCtx | Scheduler | InputStreamHandler | Executor | NodeFactory |
|---------|--------|----------------|--------------|------|----------|-----------|-------------------|----------|-------------|
| Data transport | Carrier | deque + bound | Fan-out via mirrors | — | — | — | — | — | — |
| Notification | — | Arrival callback | — | — | — | — | — | — | — |
| Timestamp bound | — | Track + MinTimestampOrBound | Propagate bound to mirrors | — | — | — | Consume bound | — | — |
| Queue storage | — | `std::deque<Packet>` | — | — | — | — | — | — | — |
| Business logic | — | — | — | Execute | — | — | — | — | — |
| Activation scheduling | — | — | — | — | — | Orchestrate | — | Dispatch | — |
| Readiness policy | — | — | — | — | — | — | Decide | — | — |
| Back-pressure | — | becomes_full/not_full callbacks | — | — | — | Throttle/Unthrottle | — | — | — |
| Source layering (Ph2) | — | — | — | Declare layer | — | Sequence layers | — | — | — |
| Completion tracking | — | IsDone() | — | — | — | Detect all closed | Detect kReadyForClose | — | — |
| Type erasure | Provide | — | — | — | — | — | — | — | — |
| Lifecycle | — | Close()→bound=Done | Close()→mirror bound=Done | Open/Process/Close | Per-invoke bag | State machine + stopping | — | — | — |
| Node creation | — | — | — | — | — | — | — | — | Register/Create |
| Error propagation | — | — | — | Return error | — | Abort graph + stopping_ | — | — | — |
