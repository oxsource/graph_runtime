# Contract: InputStreamManager

**File**: `graph_runtime/src/stream/input_stream_manager.h`

```cpp
namespace graph::runtime {

using PacketArrivalCallback = std::function<void()>;

class InputStreamManager {
 public:
  explicit InputStreamManager(Stream* stream);

  const std::string& name() const;

  // Consumer interface (delegates to Stream)
  absl::StatusOr<Packet> Pop();
  bool IsEmpty() const;
  int64_t MinTimestampOrBound(bool* empty) const;

  // Notification registration (called by Scheduler during setup)
  void SetArrivalCallback(PacketArrivalCallback cb);

  // Called by Scheduler observer when upstream pushes a Packet
  void OnPacketEnqueued();

 private:
  Stream* stream_;
  PacketArrivalCallback arrival_callback_;
  int64_t min_timestamp_bound_;
};

}  // namespace graph::runtime
```

**Semantics**:
- Wraps a single `Stream` (consumer side) and adds notification and timestamp tracking on top.
- `SetArrivalCallback()` registers a callback that fires when a new Packet is enqueued in the underlying Stream (called from `OnPacketEnqueued()`). The callback typically triggers `InputStreamHandler::NotifyPacketArrival()` on the owning Node.
- `MinTimestampOrBound()` returns the minimum timestamp across available packets and the current timestamp bound. Used by `InputStreamHandler::GetNodeReadiness()` to determine if a timestamp is settled.
- `OnPacketEnqueued()` is called by the Scheduler's Stream event observer. If `arrival_callback_` is set, invokes it once — the callback may batch multiple packets.
- Multiple upstream producers (fan-in) are not supported in Phase 1 — each InputStreamManager wraps exactly one Stream.
