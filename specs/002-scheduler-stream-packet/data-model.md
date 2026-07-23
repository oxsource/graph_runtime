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

### InputStreamShard

Per-input-port, per-invocation view of a single input Packet.

| Field | Type | Description |
|-------|------|-------------|
| `packet_queue_` | `std::queue<Packet>` | Holds the one Packet for this invocation (populated by FillInputSet) |
| `is_done_` | `bool` | Stream is closed and no more packets will arrive |

**Accessors**: `Value()` / `Get<T>()` / `IsEmpty()` / `IsDone()` / `Name()` / `Header()`

---

### InputStreamShardSet

Tag/index-addressable collection of all input shards.

| Method | Description |
|--------|-------------|
| `Get("TAG")` or `Get("TAG", index)` | Access by tag name and optional index |
| `Index(i)` | Access by flat index |
| `NumEntries()` | Total shards |
| Range iteration | `for (auto& shard : inputs)` |

---

### OutputStreamShard

Per-output-port, per-invocation write buffer.

| Field | Type | Description |
|-------|------|-------------|
| `output_queue_` | `std::list<Packet>` | Packets added during Process() |
| `closed_` | `bool` | Close() called on this shard |
| `next_timestamp_bound_` | `Timestamp` | Next valid output timestamp |

**Methods**: `AddPacket()` / `Add<T>()` / `SetNextTimestampBound()` / `SetOffset()` / `Close()` / `SetHeader()` / `Name()`

---

### OutputStreamShardSet

Tag/index-addressable collection of all output shards.

| Method | Description |
|--------|-------------|
| `Get("TAG")` / `Get("TAG", index)` | Access by tag name and optional index |
| `Index(i)` | Access by flat index |
| `NumEntries()` | Total shards |

---

### GraphContext

Per-invocation context passed to Node lifecycle methods. Aligns with MediaPipe's `CalculatorContext`.

| Field | Type | Description |
|-------|------|-------------|
| `node_name_` | `string` | Node's name in the graph |
| `node_id_` | `int` | Node's unique ID |
| `calculator_type_` | `string` | Registered type name (from NodeFactory) |
| `input_timestamp_` | `Timestamp` | Unstarted() for Open, scheduled ts for Process, Done() for Close |
| `inputs_` | `InputStreamShardSet` | Per-input-port shards with current packet |
| `outputs_` | `OutputStreamShardSet` | Per-output-port shards for writing |
| `options_` | `const NodeOptions*` | Node configuration |

**Lifecycle matrix**:

| Phase | InputTimestamp | Inputs state | Outputs state |
|-------|---------------|-------------|---------------|
| Open | `Unstarted()` | Header available | SetHeader/SetOffset allowed |
| Process | scheduled ts | Current batch packet per shard | AddPacket allowed |
| Close | `Done()` | All IsDone()=true | AddPacket + Close allowed |

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
| `active_sources_` | `set<Node*>` | Currently open source Nodes (for stopping_ cleanup) |
| `input_stream_handler_` | `unique_ptr<InputStreamHandler>` | Pluggable readiness policy |
| `default_queue_` | `SchedulerQueue` | Default queue bound to default executor |
| `non_default_queues_` | `map<string, unique_ptr<SchedulerQueue>>` | Named queues for heterogeneous executors |
| `all_queues_` | `vector<SchedulerQueue*>` | All queues for iteration |
| `default_executor_` | `shared_ptr<Executor>` | Default executor (ThreadPool or ApplicationThread) |
| `throttled_sources_` | `set<Node*>` | Source Nodes paused by back-pressure |
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

### SchedulerQueue

Per-executor priority queue implementing `TaskQueue`. One per executor. Bridges scheduling and execution.

| Field | Type | Description |
|-------|------|-------------|
| `queue_` | `std::priority_queue<Item>` | Nodes ordered by priority (Open > non-source > source) |
| `executor_` | `Executor*` | Bound executor — dispatches tasks via `AddTask(this)` |
| `num_pending_tasks_` | `int` | Tasks submitted to executor but not yet completed |
| `idle_callback_` | `function<void(bool)>` | Fires on idle state change → `Scheduler::QueueIdleStateChanged` |

**Priority ordering** (`Item::operator<`):
1. OpenNode tasks (highest — run before any ProcessNode).
2. Non-sources (scheduled before sources).
3. Sources (ordered by source_layer → node_id).

**Task dispatch path**:
```
Node ready → node->GetSchedulerQueue()->AddNode(node)
  → queue_.push(Item(node))
  → executor_->AddTask(this)
    → Schedule([this]{ RunNextTask(); })
      → thread picks up → RunNextTask()
        → pop Item → Node::Process/Open
        → if idle: idle_callback_(true) → non_idle_queue_count_--
```

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

Pure abstract interface seen by Calculator code. No mirrors or propagation logic.

| Method | Description |
|--------|-------------|
| `Name()` | Port name |
| `AddPacket(Packet)` | Enqueue a data packet |
| `SetNextTimestampBound(Timestamp)` | Advance bound without data |
| `Close()` | Mark stream closed, bound=Done |
| `SetOffset(TimestampDiff)` | Set output timestamp offset (Open only) |
| `SetHeader(Packet)` | Set stream header (Open only) |

---

### OutputStreamShard

Per-invocation write buffer. Implements `OutputStream`. Drained by `OutputStreamManager` after `Process()`.

| Field | Type | Description |
|-------|------|-------------|
| `output_queue_` | `std::list<Packet>` | Packets added during this Process() call |
| `next_timestamp_bound_` | `Timestamp` | Advanced on each AddPacket/SetNextBound |
| `updated_next_timestamp_bound_` | `Timestamp` | Only set by explicit SetNextBound (not by Reset) |
| `spec_` | `OutputStreamSpec*` | Shared metadata (offset, header, name) |

---

### OutputStreamManager

Persistent per-stream state. Owns mirrors and handles propagation.

| Field | Type | Description |
|-------|------|-------------|
| `spec_` | `OutputStreamSpec` | name, offset, header, locked_intro_data |
| `mirrors_` | `vector<Mirror>` | Downstream (InputStreamHandler*, CollectionItemId) |
| `next_timestamp_bound_` | `Timestamp` | Current output bound |
| `closed_` | `bool` | Stream closed |

**Methods**: `ComputeOutputTimestampBound()` / `PropagateUpdatesToMirrors()` / `ResetShard()` / `Close()` / `PropagateHeader()` / `LockIntroData()`

---

### OutputStreamHandler

Per-Node orchestrator for all output streams.

| Method | Description |
|--------|-------------|
| `InitializeOutputStreamManagers(flat*)` | Wire to flat manager array |
| `SetupOutputShards(shards*)` | Assign spec ptrs to shards |
| `Open(shards*)` | Propagate headers, lock intro data |
| `PrepareOutputs(ts, shards*)` | ResetShard for each manager |
| `PostProcess(ts, shards*)` | Compute bound + propagate to mirrors |
| `Close(shards*)` | Propagate remaining, close all managers |

**Default**: `InOrderOutputStreamHandler` — Phase 1 sequential path (direct PropagateOutputPackets).

---



### SyncSet

Group of input streams for coordinated readiness calculation. `DefaultInputStreamHandler` uses a single SyncSet; advanced handlers may split streams into multiple sets.

| Method | Description |
|--------|-------------|
| `GetReadiness(Timestamp*) -> Readiness` | Compare min_packet vs min_bound across all streams. Returns kReadyForProcess when timestamp is settled, kNotReady otherwise, kReadyForClose when all done. |
| `FillInputSet(Timestamp, GraphContext&)` | Pop one packet per stream at the given timestamp into InputStreamShards. |
| `FillInputBounds(GraphContext&)` | Pop timestamp-bound-only packets (ProcessTimestampBounds mode). |

**Readiness logic**:
- `min_stream_timestamp = min(min_packet, min_bound)` across all streams in set.
- If all streams have bound >= `Timestamp::Done()` → `kReadyForClose`.
- If `min_bound > min_packet` → `kReadyForProcess` (timestamp settled).
- Otherwise → `kNotReady`.

---

### InputStreamHandler

Pluggable strategy determining when a Node is ready to execute. Collaborates with `SyncSet`(s) for per-stream-set readiness.

| Method | Description |
|--------|-------------|
| `SetScheduleCallback(ScheduleCallback)` | Register callback to schedule Node execution |
| `ScheduleInvocations(max_allowance, input_bound*) -> bool` | Core scheduling loop — calls GetNodeReadiness up to max_allowance times. Each time: if ready → FillInputSet → ScheduleCallback. Returns true if any invocation scheduled. |
| `GetNodeReadiness(Timestamp*) -> Readiness` | Delegates to SyncSet::GetReadiness. Returns kReadyForProcess, kReadyForClose, or kNotReady. |
| `FillInputSet(Timestamp, GraphContext&)` | Delegates to SyncSet::FillInputSet. Pop one Packet per input port into InputStreamShards. |
| `NotifyPacketArrival()` | Called by InputStreamManager arrival callback. Triggers ScheduleInvocations. |
| `SetNextTimestampBound(CollectionItemId, Timestamp)` | Forward bound to specific InputStreamManager. Called when upstream closes or propagates bound. |
| `Close()` | Close all managed InputStreamManagers. |

**Built-in strategies**:

| Handler | Behavior | SyncSet | Schedule Trigger |
|---------|----------|---------|-----------------|
| `DefaultInputStreamHandler` | Ready when ALL input Streams have ≥1 Packet at a settled timestamp. Pops one per stream. | Single set (all streams) | Last missing input arrives |
| `ImmediateInputStreamHandler` | Ready when ANY input Stream has a Packet. May produce non-monotonic timestamps. | Single set | Any packet arrival |
| `BarrierInputStreamHandler` | Ready when ALL streams have a Packet at the **same timestamp**. | Single set | Last stream reaches timestamp T |

---

### Executor

Pluggable strategy for running scheduler tasks. Works with `SchedulerQueue` via `TaskQueue` interface.

| Method | Description |
|--------|-------------|
| `AddTask(TaskQueue*)` | Entry point from SchedulerQueue — schedules `RunNextTask()` |
| `Schedule(closure)` | Pure virtual — executes closure on executor's thread(s) |

**TaskQueue interface**:
- `RunNextTask()` — pure virtual, implemented by `SchedulerQueue`. Called by executor when a thread is available.

**Built-in implementations**:
| Executor | Threads | Description | Phase |
|----------|---------|-------------|-------|
| `ApplicationThreadExecutor` | 0 (uses app thread) | Tasks enqueued to `app_thread_tasks_`, drained in `WaitUntilDone()`. Implemented via `DelegatingExecutor`. | Phase 1 |
| `ThreadPoolExecutor` | N (`min(CPUs, nodes)`) | Distributes tasks across a fixed-size thread pool. `num_threads` configurable. | Phase 1 |

**Per-node assignment**:
- Nodes declare executor via `Node::SetExecutorName()` from config.
- Empty name → `default_queue_` → default executor.
- Non-empty name → looked up in `non_default_queues_` → named executor queue.

---

### NodeContract

Port type declaration interface. Passed to Node's static `GetContract()` for graph validation.

| Method | Description |
|--------|-------------|
| `Inputs().Get("name").Set<T>()` | Declare an input port of type T |
| `Inputs().Get("name").SetAny()` | Declare an input port accepting any type |
| `Inputs().Get("name").SetSameAs(other)` | Mirror another port's type |
| `Outputs()...` | Same as Inputs for output ports |
| `Options<T>()` | Typed access to node config options |
| `SetMaxInFlight(n)` | Limit concurrent Process invocations |

**Boundaries**:
- Used at graph construction time — called once per node type during `ValidatedGraphConfig::Initialize()`.
- No Node instance exists yet — only port contracts are validated.
- Type mismatch between connected ports produces an error at build time.
- All node types MUST implement `static GetContract(NodeContract*)` (enforced by static_assert).

---

### NodeFactory

Polymorphic factory for creating Node instances. Each registered node type has one factory.

| Method | Description |
|--------|-------------|
| `GetContract(NodeContract*)` | Declare port types — calls T::GetContract() |
| `CreateNode(name, options) -> unique_ptr<Node>` | Create a Node instance for a graph run |

**NodeFactoryFor<T>**:
- Template specialization that calls `T::GetContract(contract)` and `return make_unique<T>(name, options)`.
- static_assert enforces that T inherits from Node and has `GetContract`.

---

### NodeFactoryRegistry

Global singleton registry mapping type names to `unique_ptr<NodeFactory>`.

| Method | Description |
|--------|-------------|
| `Register(type_name, factory) -> int` | Register a factory, returns registration ID |
| `Unregister(id) -> bool` | Remove a factory by ID (for testing) |
| `CreateByName(name, ...)` | Look up and create a Node by type name |
| `CreateByNameInNamespace(ns, name, ...)` | Look up with namespace prefix |
| `GetFactory(name)` | Look up factory without creating (for validation) |
| `IsRegistered(name)` | Check if a type is registered |

**Registration macro**:
```cpp
GRAPH_RUNTIME_REGISTER_NODE("TypeName", MyNode)
```
- File-scope static variable, runs before main().
- `GRAPH_RUNTIME_` prefix avoids symbol conflicts across projects.
- Creates a `NodeRegistrationToken` that auto-unregisters on destruction.

**Boundaries**:
- Registry is global — populated at program initialization via static registration.
- All node types must be registered before graph construction.
- Default node types are registered via `GRAPH_RUNTIME_REGISTER_NODE` in their respective .cc files.

---

## Relationships

```
GraphBuilder (validation phase)
  │  for each config node:
  │    factory = NodeFactoryRegistry::CreateByName(type_name)  // or CreateByNameInNamespace
  │    factory->GetContract(&contract)                          // validate port types
  │    store NodeContract in NodeTypeInfo
  ▼

GraphBuilder (construction phase)
  │  for each validated node:
  │    factory = NodeFactoryRegistry::GetFactory(type_name)
  │    node = factory->CreateNode(name, options)
  │    creates InputStreamManager per input port
  │    creates OutputStream per output port
  │    wires OutputStream::mirrors_ → InputStreamManager*
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

---

### GraphConfig

Configuration data for building a graph pipeline.

| Field | Type | Description |
|-------|------|-------------|
| `nodes` | `vector<NodeDef>` | Node definitions (name, type, ports, options, executor) |
| `streams` | `vector<StreamDef>` | Stream connections between node ports |
| `executors` | `vector<ExecutorDef>` | Named executor configurations |
| `input_streams` | `vector<string>` | External input stream names (creates GraphInputStreams) |
| `output_streams` | `vector<string>` | External output stream names (creates GraphOutputStreams) |
| `max_queue_size` | `int` | Default max queue depth (100, -1 = unlimited) |
| `report_deadlock` | `bool` | true = deadlock fails graph instead of auto-unthrottle |

**NodeDef extended fields**:
| Field | Type | Description |
|-------|------|-------------|
| `input_side_packets` | `vector<string>` | Side packet inputs (`"TAG:name"`) |
| `output_side_packets` | `vector<string>` | Side packet outputs (`"TAG:name"`) |
| `input_stream_handler` | `string` | Per-node input handler override |
| `max_in_flight` | `int` | Concurrent Process invocations (Phase 2) |

---

### GraphBuilder

Static utility that validates `GraphConfig`, creates instances, wires connections, and returns a `GraphRuntime`.

| Step | Description |
|------|-------------|
| ValidateContracts | Call NodeFactory::GetContract per node, check stream + side packet types |
| ValidateOptions | Check OptionsRegistry for each node's options type |
| CreateExecutors | Instantiate ThreadPoolExecutor per ExecutorDef |
| CreateNodes | NodeFactoryRegistry::CreateByName for each NodeDef |
| CreateStreamManagers | InputStreamManager + OutputStreamManager per port |
| WireMirrors | Populate OutputStreamManager::mirrors_ from StreamDef |
| CreateGraphInputStreams | Virtual source nodes for external input |
| CreateGraphOutputStreams | Virtual sink nodes for external output |
| CreateOutputStreamHandlers | One per node |
| CreateScheduler | With configured executors + InputStreamHandler |
| AssignNodesToQueues | Scheduler::AssignNodeToQueue per node |
| Return GraphRuntime | Fully initialized, ready for Start() |

---

### SidePacket / PacketSet / OutputSidePacketSet

Graph-level constants, validated and injected before Start().

| Class | Purpose |
|-------|---------|
| `PacketSet` | Immutable input side packet collection (`Get(name)`) |
| `OutputSidePacketSet` | Mutable output side packet collection (`Set(name, packet)`) |

**Boundaries**:
- Declared via `NodeContract::InputSidePackets()` / `OutputSidePackets()` during GetContract.
- Injected via `GraphRuntime::SetInputSidePacket(name, packet)` before Start().
- Access via `GraphContext::InputSidePackets()` / `OutputSidePackets()` during Open/Process/Close.
- Type mismatches caught at build time during graph validation.

---

### OptionsRegistry

Compile-time options deserialization without protobuf.

| Method | Description |
|--------|-------------|
| `Register<T>(type_name)` | Create registrar for type T |
| `Deserialize<T>(options) -> T` | Convert key-value map to typed struct |
| `Field(name, &T::member)` | Register a field for deserialization |

**Registration macro**:
```cpp
GRAPH_RUNTIME_REGISTER_OPTIONS("MyCalc", MyCalcOptions)
    .Field("threshold", &MyCalcOptions::threshold);
```

---

### NodeOptions

Simple key-value options container with registry-backed typed deserialization.

| Method | Description |
|--------|-------------|
| `Set<T>(key, value)` | Store a typed option |
| `Get<T>(key) -> const T*` | Retrieve a typed option (nullptr if missing) |
| `Deserialize<T>() -> const T&` | Deserialize via OptionsRegistry into typed struct (cached) |

---

Project-wide type aliases and utility functions.

| Symbol | Definition | Purpose |
|--------|-----------|---------|
| `CollectionItemId` | `int` | Lightweight index handle for tag-indexed port collections |
| `ErrorCallback` | `function<void(Status)>` | Invoked on non-OK, non-Stop status |
| `IsStopStatus(status) -> bool` | `status.code() == kUnavailable` | Distinguish graceful stop from error |
| `StatusStop()` | `UnavailableError("Stop")` | Helper to create stop status |

---

| Concern | Packet | InputStreamMgr | OutputStreamMgr | OutputStream | Node | NodeContract | NodeFactoryReg | NodeFactory | GraphCtx | GraphCtxMgr | GraphRuntime | Scheduler | InputStreamHdlr | Executor |
|---------|--------|----------------|-----------------|--------------|------|-------------|---------------|------------|----------|-------------|-------------|-----------|----------------|----------|
| Data transport | Carrier | deque + bound | Fan-out mirrors | — | — | — | — | — | — | — | — | — | — | — |
| Notification | — | Arrival callback | — | — | — | — | — | — | — | — | — | — | — | — |
| Timestamp bound | — | Track bound | Propagate bound | — | — | — | — | — | — | — | — | — | Consume bound | — |
| Queue storage | — | deque | — | — | — | — | — | — | — | — | — | — | — | — |
| Business logic | — | — | — | — | Execute | — | — | — | — | — | — | — | — | — |
| Port declaration | — | — | — | — | static GetContract | Declare types | — | — | — | — | — | — | — | — |
| Port validation | — | — | — | — | — | Validate | Lookup factory | GetContract | — | — | — | — | — | — |
| Node instantiation | — | — | — | — | — | — | CreateByName | CreateNode | — | — | — | — | — | — |
| Activation scheduling | — | — | — | — | — | — | — | — | — | — | — | Orchestrate | — | Dispatch |
| Readiness policy | — | — | — | — | — | — | — | — | — | — | — | — | Decide | — |
| Back-pressure | — | Full/not_full cb | — | — | — | — | — | — | — | — | — | Throttle | — | — |
| Registration | — | — | — | — | — | — | Register macro | — | — | — | — | — | — | — |
| Source layering(Ph2) | — | — | — | — | Declare layer | — | — | — | — | — | — | Sequence layers | — | — |
| Completion tracking | — | IsDone() | — | — | — | — | — | — | — | — | — | Detect closed | Detect kReadyForClose | — |
| Type erasure | Provide | — | — | — | — | — | — | — | — | — | — | — | — | — |
| Context pool | — | — | — | — | — | — | — | — | — | Default/Prepare/Recycle | — | — | — | — |
| External I/O | — | — | — | — | — | — | — | — | — | — | AddPacket/SetCallback | — | — | — |
| Graph lifecycle | — | — | — | — | — | — | — | — | — | — | Init/Start/Wait/Shutdown | — | — | — |
| Error propagation | — | — | — | — | Return error | — | — | — | — | — | — | Abort + stopping_ | — | — |
