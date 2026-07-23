# Data Model: Graph Runtime

## Entities

### Packet

Atomic data unit flowing through the graph.

| Field | Type | Description |
|-------|------|-------------|
| `holder_` | `shared_ptr<HolderBase>` | Type-erased payload via custom Holder<T> |
| `timestamp_` | `Timestamp` | Timestamp for ordering + lifecycle signalling |

**Validation rules**:
- Timestamp MUST be monotonically non-decreasing within a single Stream.
- End-of-stream is signaled by `Timestamp::Done()`, not by a boolean.
- Payload type checked at read time via `Get<T>()` or `ValidateAsType<T>()`.

**Ownership**: Shallow-copy semantics — copy increments shared_ptr refcount (O(1)). Source never deep-copied.

---

### Timestamp

Timestamp class with special values encoded in int64_t range extremes.

**Special values**: `Unset()` (INT64_MIN), `Unstarted()` (Open phase), `PreStream()` (header), `Min()`, `Max()`, `PostStream()` (summary), `OneOverPostStream()` (internal), `Done()` (Close phase, INT64_MAX).

**Validation**: Constructor CHECK-fails if called with special value. End-of-stream detected via `timestamp() == Timestamp::Done()`.

---

### InputStreamManager

Per-input-port queue + timestamp tracking. No intermediate Stream class — receives data directly from upstream OutputStreamManager mirrors.

| Field | Type | Description |
|-------|------|-------------|
| `queue_` | `std::deque<Packet>` | Packet buffer, written by upstream mirrors, read by FillInputSet |
| `next_timestamp_bound_` | `Timestamp` | Advances on AddPackets/SetNextTimestampBound/Close |
| `closed_` | `bool` | true after Close(), which sets bound = Timestamp::Done() |
| `max_queue_size_` | `int` | Back-pressure threshold (-1 = unlimited) |
| `arrival_callback_` | `PacketArrivalCallback` | Fires when queue transitions from empty to non-empty |
| `becomes_full/not_full_callback_` | `QueueSizeCallback` | Trigger Scheduler throttle/unthrottle |

**Key methods**: `AddPackets()` / `MovePackets()` (last mirror gets move, zero-copy), `PopPacketAtTimestamp(ts)`, `PopQueueHead()`, `MinTimestampOrBound()`, `IsDone()` (queue empty && bound == Timestamp::Done()).

---

### OutputStreamManager

Per-output-stream persistent state. Owns mirrors and handles propagation.

| Field | Type | Description |
|-------|------|-------------|
| `spec_` | `OutputStreamSpec` | name, offset, header, locked_intro_data |
| `mirrors_` | `vector<Mirror>` | Downstream (InputStreamHandler*, CollectionItemId) |
| `next_timestamp_bound_` | `Timestamp` | Current output bound |

**Key methods**: `ComputeOutputTimestampBound(shard, input_ts)`, `PropagateUpdatesToMirrors(bound, shard)` (last mirror: MovePackets, others: AddPackets), `ResetShard(shard)`.

---

### OutputStreamShard

Per-invocation write buffer. Implements `OutputStream`. Drained by `OutputStreamManager` after Process().

| Field | Type | Description |
|-------|------|-------------|
| `output_queue_` | `std::list<Packet>` | Packets added during Process() |
| `next_timestamp_bound_` | `Timestamp` | Advanced on each AddPacket |
| `updated_next_timestamp_bound_` | `Timestamp` | Only set by explicit SetNextBound |

---

### Node

Computational unit with lifecycle.

| Field | Type | Description |
|-------|------|-------------|
| `name` | `std::string` | Unique identifier within Graph |
| `input_port_managers_` | `map<string, InputStreamManager*>` | Named input ports (deque + bound owner) |
| `output_streams_` | `map<string, OutputStream*>` | Named output ports (fan-out + mirrors) |
| `executor_name_` | `std::string` | Named executor assignment |
| `scheduler_queue_` | `SchedulerQueue*` | Queue for task scheduling |
| `source_layer_` | `int` | Source activation order (Phase 2) |

**Lifecycle**: `Open(GraphContext&)` → `Process(GraphContext&)` (repeated) → `Close(GraphContext&)`.
**Required static method**: `static GetContract(NodeContract* contract)` declares port types.

---

### GraphContext

Per-invocation context, analogous to MediaPipe's CalculatorContext.

| Field | Type | Description |
|-------|------|-------------|
| `node_name_` | `string` | Node's name |
| `input_timestamp_` | `Timestamp` | Unstarted()/scheduled_ts/Done() |
| `inputs_` | `InputStreamShardSet` | Per-input-port shards |
| `outputs_` | `OutputStreamShardSet` | Per-output-port shards |
| `input_side_packets_` | `PacketSet` | Graph-level constants |
| `output_side_packets_` | `OutputSidePacketSet` | Output side packets |

**Shard types**: `InputStreamShard` (Value/IsEmpty/IsDone/Header), `OutputStreamShard` (AddPacket/SetOffset/SetHeader/Close).

---

### Scheduler

Event-driven execution engine with 5-state machine.

| State | Meaning |
|-------|---------|
| `kNotStarted` | Before Schedule() |
| `kRunning` | Actively scheduling |
| `kPaused` | All queues stopped |
| `kCancelling` | Error recorded, draining |
| `kTerminated` | Final state |

**Key fields**: `stopping_` (StatusStop flag), `has_error_`, `non_idle_queue_count_` (idle aggregation), `handling_idle_` (reentrancy guard), `active_sources_` (set of open sources).

---

### SchedulerQueue

Per-executor priority queue implementing `TaskQueue`.

| Priority | Order |
|----------|-------|
| 1 (highest) | OpenNode tasks |
| 2 | Non-sources |
| 3 (lowest) | Sources (by layer → node_id) |

**Key methods**: `AddNode()`, `AddNodeForOpen()`, `RunNextTask()`, `SetExecutor()`, `SetIdleCallback()`.

---

### Executor / ThreadPoolExecutor

`Executor`: abstract base with `AddTask(TaskQueue*)` and `Schedule(closure)`.
`ThreadPoolExecutor`: multi-threaded, `num_threads = min(CPUs, nodes)`, mutex + condition variable.

---

### InputStreamHandler / SyncSet

`InputStreamHandler`: pluggable readiness policy with `ScheduleInvocations(max_allowance, input_bound)`.
`SyncSet`: inner class for readiness calculation — compares `min_packet` vs `min_bound`.
**Strategies**: `DefaultInputStreamHandler` (all-inputs barrier), `ImmediateInputStreamHandler`, `BarrierInputStreamHandler`.

---

### NodeFactory / NodeFactoryRegistry

`NodeFactory`: polymorphic base with `GetContract()` and `CreateNode()`.
`NodeFactoryFor<T>`: template implementation with `static_assert(HasGetContract<T>)`.
`NodeFactoryRegistry`: global singleton, `GRAPH_RUNTIME_REGISTER_NODE(type, class)` macro.
`NodeRegistrationToken`: RAII token for test cleanup.

---

### GraphConfig / GraphBuilder

`GraphConfig`: `NodeDef` (name, type, ports, options, executor, source_layer), `StreamDef` (connections), `ExecutorDef` (name, type, num_threads), global `max_queue_size`, `report_deadlock`.
`GraphBuilder::Build(config)`: validate contracts → create executors → create nodes → create managers → wire mirrors → return GraphRuntime.

---

## Relationships

```
GraphBuilder::Build(config)
  → NodeFactoryRegistry::CreateByName(type)
    → NodeFactory::CreateNode(name, options)
      → Node (input_port_managers_ + output_streams_)

OutputStreamManager::AddMirror(handler, id)  ← GraphBuilder wiring
  → writes directly to InputStreamManager::AddPackets/MovePackets
  → arrival_callback_ → NotifyPacketArrival
    → InputStreamHandler::ScheduleInvocations
      → SyncSet::GetReadiness
        → kReadyForProcess → SchedulerQueue::AddNode(node)
          → Executor::ScheduleTask
            → SchedulerQueue::RunNextTask
              → InputStreamHandler::FillInputSet → PopPacketAtTimestamp
              → Node::Process(context)
              → OutputStreamHandler::PostProcess
                → OutputStreamManager::PropagateUpdatesToMirrors
```

No intermediate Stream class. Data flows directly between OutputStreamManager and InputStreamManager.
