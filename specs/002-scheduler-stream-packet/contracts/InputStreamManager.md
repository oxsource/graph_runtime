# Contract: InputStreamManager

**File**: `graph_runtime/src/stream/input_stream_manager.h`

```cpp
namespace graph::runtime {

using QueueSizeCallback = std::function<void(InputStreamManager*, bool*)>;
using PacketArrivalCallback = std::function<void()>;

class InputStreamManager {
 public:
  explicit InputStreamManager(std::string name, int max_queue_size = -1);

  const std::string& name() const;

  // --- Producer interface (called by OutputStream::Send) ---

  // Add packets at the back of the deque. Copy variant.
  absl::Status AddPackets(const std::list<Packet>& packets, bool* notify);
  // Add packets at the back of the deque. Move variant (last mirror, zero-copy).
  absl::Status MovePackets(std::list<Packet>* packets, bool* notify);

  // Advance the timestamp bound without adding a packet.
  void SetNextTimestampBound(Timestamp bound);
  void Close();  // sets bound = Timestamp::Done()

  // --- Consumer interface (called by Scheduler task runner) ---

  // Pop all packets with timestamp <= requested timestamp.
  // Returns the exact match, or an empty packet at the timestamp bound.
  // *num_packets_dropped counts how many packets were popped (starts at -1).
  // *stream_is_done set to true when queue empty AND bound == Timestamp::Done().
  Packet PopPacketAtTimestamp(Timestamp timestamp,
                              int* num_packets_dropped,
                              bool* stream_is_done);

  // Pop the front of the queue (untimed consumption mode).
  Packet PopQueueHead(bool* stream_is_done);

  // Peek front without popping.
  Packet QueueHead() const;

  // --- State ---
  bool IsEmpty() const;               // queue_.empty()
  bool IsFull() const;                // max_queue_size_ != -1 && size >= threshold
  bool IsDone() const;                // queue_.empty() && bound_ == Timestamp::Done()
  int QueueSize() const;
  int MaxQueueSize() const;
  int64_t NumPacketsAdded() const;    // lifetime monotonic counter

  // Timestamp bound — used by InputStreamHandler::GetNodeReadiness()
  // Returns queue front's timestamp if non-empty, else next_timestamp_bound_.
  Timestamp MinTimestampOrBound(bool* is_empty) const;

  // --- Back-pressure callbacks (called by Scheduler during setup) ---
  void SetMaxQueueSize(int max_queue_size);
  void SetQueueSizeCallbacks(QueueSizeCallback full_cb,
                             QueueSizeCallback not_full_cb);

  // --- Arrival notification (called by Scheduler during setup) ---
  void SetArrivalCallback(PacketArrivalCallback cb);

 private:
  std::string name_;
  std::deque<Packet> queue_;
  int64_t num_packets_added_ = 0;
  Timestamp next_timestamp_bound_{Timestamp::PreStream()};
  Timestamp last_select_timestamp_{Timestamp::Unstarted()};
  bool closed_ = false;
  int max_queue_size_ = -1;

  PacketArrivalCallback arrival_callback_;
  QueueSizeCallback becomes_full_callback_;
  QueueSizeCallback becomes_not_full_callback_;
  bool last_reported_stream_full_ = false;
};

}  // namespace graph::runtime
```

**Semantics**:
- Manages a `std::deque<Packet>` that holds packets for exactly one downstream Node input port. **No intermediate Stream component exists** — OutputStream writes directly here.
- `AddPackets()` / `MovePackets()` enqueue at the back. `*notify` is set to true if the queue was previously empty (signals Scheduler to evaluate readiness). Fires `becomes_full_callback_` if queue crosses `max_queue_size_` threshold.
- `SetNextTimestampBound()` advances `next_timestamp_bound_` without adding a packet. Used for timestamp propagation without data.
- `Close()` sets `next_timestamp_bound_ = Timestamp::Done()`. After this, `IsDone()` returns true when the queue drains. **No sentinel Done packet is pushed.**
- `PopPacketAtTimestamp(ts)` pops all packets with timestamp <= ts from the front. Returns the exact match, or a synthetic empty packet at the bound if no exact match. This is the core consumption API for timestamp-synchronized inputs.
- `PopQueueHead()` pops the front regardless of timestamp (untimed mode for `ImmediateInputStreamHandler`).
- `MinTimestampOrBound()` returns `queue_.front().Timestamp()` if non-empty, or `next_timestamp_bound_` if empty. Used by `InputStreamHandler::GetNodeReadiness()` to compute the settled timestamp.
- `IsDone()` returns `queue_.empty() && next_timestamp_bound_ == Timestamp::Done()` — this is how the scheduler detects stream completion without a sentinel packet.
- Back-pressure callbacks (`becomes_full_callback_` / `becomes_not_full_callback_`) are registered by the Scheduler during `Schedule()`. They fire when queue size crosses `max_queue_size_`. The Scheduler uses them to throttle/unthrottle upstream sources.
- `arrival_callback_` is fired inside `AddPackets`/`MovePackets`/`SetNextTimestampBound` when the queue transitions from empty to non-empty, or when a bound advance may trigger readiness. It calls `InputStreamHandler::NotifyPacketArrival()`.
