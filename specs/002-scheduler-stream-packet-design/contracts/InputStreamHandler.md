# Contract: InputStreamHandler

**File**: `graph_runtime/src/scheduler/input_stream_handler.h`

```cpp
namespace graph::runtime {

class Node;
class GraphContext;

class SyncSet {
 public:
  // Determine node readiness considering all streams in this set.
  // Returns kReadyForProcess, kReadyForClose, or kNotReady.
  // *min_stream_timestamp receives the minimum timestamp across all streams.
  Readiness GetReadiness(Timestamp* min_stream_timestamp);

  // Pop one packet per stream at the given timestamp into InputStreamShards.
  void FillInputSet(Timestamp timestamp, GraphContext& context);

  // Pop timestamp-bound-only packets into shards (for ProcessTimestampBounds mode).
  void FillInputBounds(GraphContext& context);
};

class InputStreamHandler {
 public:
  using ScheduleCallback = std::function<void(Node& node)>;

  enum class Readiness {
    kNotReady,         // Node cannot execute yet
    kReadyForProcess,  // All conditions met → run Process()
    kReadyForClose,    // All input streams closed → run Close()
  };

  virtual ~InputStreamHandler() = default;

  // Set the callback the handler uses to schedule work when readiness changes.
  virtual void SetScheduleCallback(ScheduleCallback cb) = 0;

  // --- Core scheduling interface ---

  // Main scheduling loop. Called after NotifyPacketArrival or after a Process
  // completes (rescheduling). Schedules up to max_allowance invocations.
  // *input_bound receives the minimum timestamp bound for downstream.
  // Returns true if any invocation was scheduled.
  virtual bool ScheduleInvocations(int max_allowance,
                                   Timestamp* input_bound) = 0;

  // Readiness check (may be called within ScheduleInvocations).
  virtual Readiness GetNodeReadiness(Timestamp* min_stream_timestamp) = 0;

  // Populate GraphContext.inputs via SyncSet::FillInputSet.
  virtual void FillInputSet(Timestamp timestamp, GraphContext& context) = 0;

  // --- Event handling ---

  // Called when a new Packet arrives on one of the Node's input Streams.
  // Delegates to ScheduleInvocations.
  virtual void NotifyPacketArrival() = 0;

  // Propagate timestamp bound to a specific input stream manager.
  // Called by the Scheduler's output propagation when upstream closes.
  virtual void SetNextTimestampBound(CollectionItemId id,
                                     Timestamp bound) = 0;

  // --- Lifecycle ---
  virtual void Close() = 0;
};

}  // namespace graph::runtime
```

**Collaboration with InputStreamManager**:
- Each Node input port has one `InputStreamManager` owning a `std::deque<Packet>` and timestamp bound.
- `InputStreamManager::AddPackets()` / `MovePackets()` fires `arrival_callback_` when queue transitions from empty to non-empty, which calls `NotifyPacketArrival()`.
- The handler reads each port's state via `InputStreamManager::MinTimestampOrBound()` and `IsEmpty()` to determine readiness.
- `FillInputSet()` calls `InputStreamManager::PopPacketAtTimestamp(ts)` to consume packets.

**Semantics**:
- `SetScheduleCallback()` provides the handler with a mechanism to schedule Node execution. Called once during Scheduler initialization.
- `ScheduleInvocations(max_allowance, input_bound)` is the **core scheduling loop** — analogous to MediaPipe's `ScheduleInvocations()`:
  1. While `invocations_scheduled < max_allowance`:
     - Call `GetNodeReadiness(&min_timestamp)`.
     - If `kReadyForProcess`:
       - Call `FillInputSet(min_timestamp, context)`.
       - Invoke `ScheduleCallback(node)`.
     - If `kReadyForClose`:
       - Invoke `ScheduleCallback(node)` (for CloseNode).
       - Break.
     - If `kNotReady`:
       - Set `*input_bound = min_timestamp` so downstream knows the bound.
       - Break.
  2. Returns true if any invocation was scheduled.
- `GetNodeReadiness(min_stream_timestamp)` is called within `ScheduleInvocations` — no central polling loop. Returns `kReadyForProcess` when all input conditions are met, `kReadyForClose` when all input Streams have reached end-of-stream, or `kNotReady`.
- `FillInputSet(ts, context)` is called just before scheduling `Process()`. It pops one Packet per input port via `InputStreamManager::PopPacketAtTimestamp(ts)` and places each into the corresponding `InputStreamShard` in `GraphContext::Inputs()`. If a port has no packet at the given timestamp, the shard is populated with an empty Packet (`IsEmpty() == true`).
- `NotifyPacketArrival()` is called by `InputStreamManager`'s arrival callback. It calls `ScheduleInvocations(max_allowance, &input_bound)` to determine if the node is now ready.
- `SetNextTimestampBound(id, bound)` forwards to the specified `InputStreamManager::SetNextTimestampBound(bound)`. Called by the Scheduler's event observer when upstream propagates a bound (e.g., on Close).

**SyncSet**:
- `SyncSet` groups a subset of input streams for coordinated readiness calculation.
- `DefaultInputStreamHandler` uses a single SyncSet containing all streams.
- Advanced handlers (e.g., `SyncSetInputStreamHandler`) may split streams into multiple sync sets, each synchronized independently.
- `GetReadiness(ts)` compares `min_packet_timestamp` vs `min_bound_timestamp` across all streams in the set:
  - If all streams have bound >= `Timestamp::Done()` → `kReadyForClose`.
  - If `min_bound_timestamp > min_packet_timestamp` → `kReadyForProcess` (the timestamp is settled).
  - Otherwise → `kNotReady`.
- `FillInputSet(ts, ctx)` pops all packets at `ts` from each stream's manager into the shard.

**Built-in strategies**:

| Handler | Behavior | Schedule Trigger |
|---------|----------|-----------------|
| `DefaultInputStreamHandler` | Ready when ALL input Streams have ≥1 Packet at a settled timestamp. Pops one per stream. Single SyncSet. | Last missing input arrives; timestamp settles |
| `ImmediateInputStreamHandler` | Ready when ANY input Stream has a Packet. May produce non-monotonic timestamps. | Any packet arrival |
| `BarrierInputStreamHandler` | Ready when ALL streams have a Packet at the **same timestamp**. | Last stream reaches timestamp T |

Execution order is guaranteed by event timing — Open only happens during initialization, Process is triggered by data arrival, Close is triggered by stream exhaustion. No priority enumeration is needed.
