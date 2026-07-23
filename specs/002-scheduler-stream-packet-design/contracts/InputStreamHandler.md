# Contract: InputStreamHandler

**File**: `graph_runtime/src/scheduler/input_stream_handler.h`

```cpp
namespace graph::runtime {

class Node;
class GraphContext;

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

  // Determines whether a Node is ready to execute.
  virtual Readiness GetNodeReadiness(const Node& node) = 0;

  // Populates GraphContext.inputs by popping from ready input Streams.
  virtual void FillInputSet(Node& node, GraphContext& context) = 0;

  // Called when a new Packet arrives on one of the Node's input Streams.
  // The handler MUST call GetNodeReadiness() and, if ready, invoke
  // the ScheduleCallback to schedule execution.
  virtual void NotifyPacketArrival(Node& node) = 0;
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
- `GetNodeReadiness()` is called by the handler itself within `NotifyPacketArrival()` — no central polling loop. Returns `kReadyForProcess` when all input conditions are met, `kReadyForClose` when all input Streams have reached end-of-stream, or `kNotReady`.
- `FillInputSet()` is called by the Scheduler's task runner just before executing `Node::Process()`. It pops one Packet per input port via `InputStreamManager::Pop()` and places them in `GraphContext::inputs`.
- `NotifyPacketArrival()` is the **core event entry point**. Called by the `InputStreamManager`'s arrival callback every time a Packet is enqueued. The handler:
  1. Updates internal readiness state (tracks per-port packet availability).
  2. Calls `GetNodeReadiness()`.
  3. If `kReadyForProcess` → invokes `ScheduleCallback(node)`.
  4. If `kReadyForClose` → invokes `ScheduleCallback(node)`.

Execution order is guaranteed by event timing — Open only happens during initialization, Process is triggered by data arrival, Close is triggered by stream exhaustion. No priority enumeration is needed.

**Built-in strategies**:

| Handler | Behavior | Schedule Trigger |
|---------|----------|-----------------|
| `DefaultInputStreamHandler` | Ready when ALL input Streams have ≥1 Packet. Pops one per stream. Preserves timestamp order. | Last missing input arrives |
| `ImmediateInputStreamHandler` | Ready when ANY input Stream has a Packet. May produce non-monotonic timestamps. | Any packet arrival |
| `BarrierInputStreamHandler` | Ready when ALL streams have a Packet at the **same timestamp**. | Last stream reaches timestamp T |

Events are self-ordering by nature: Open at init, Process on data, Close on exhaustion. No priority is required.
