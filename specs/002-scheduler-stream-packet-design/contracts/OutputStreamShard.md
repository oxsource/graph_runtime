# Contract: OutputStreamShard

**File**: `graph_runtime/src/stream/output_stream_shard.h`

```cpp
namespace graph::runtime {

class OutputStreamManager;

class OutputStreamShard : public OutputStream {
 public:
  OutputStreamShard();

  // Called during each Process() — reset to manager's current state
  void Reset(Timestamp next_timestamp_bound, bool close);

  // From OutputStream:
  const std::string& Name() const final;
  void AddPacket(const Packet& packet) final;     // copy
  void AddPacket(Packet&& packet) final;           // move
  void SetNextTimestampBound(Timestamp bound) final;
  Timestamp NextTimestampBound() const final { return next_timestamp_bound_; }
  void Close() final;
  bool IsClosed() const final;
  void SetOffset(TimestampDiff offset) final;
  bool OffsetEnabled() const final;
  TimestampDiff Offset() const final;
  void SetHeader(const Packet& packet) final;
  const Packet& Header() const final;

  // Shard-specific
  bool IsEmpty() const { return output_queue_.empty(); }
  Timestamp LastAddedPacketTimestamp() const;
  std::list<Packet>& OutputQueue() { return output_queue_; }

 private:
  friend class OutputStreamManager;  // manager reads/pops the queue

  OutputStreamSpec* spec_ = nullptr;
  std::list<Packet> output_queue_;
  bool closed_ = false;
  Timestamp next_timestamp_bound_{Timestamp::Unset()};
  Timestamp updated_next_timestamp_bound_{Timestamp::Unset()};
};

}  // namespace graph::runtime
```

**Semantics**:
- Created per-invocation in `GraphContext`. Reset before each `Process()`, drained after `Process()` by `OutputStreamManager::PropagateUpdatesToMirrors()`.
- `AddPacket()` validates timestamp and type against `spec_`, pushes to `output_queue_`. Advances `next_timestamp_bound_` and `updated_next_timestamp_bound_`.
- `Reset()` clears the queue and sets initial bound and closed state from the manager. `updated_next_timestamp_bound_` is set to `Unset()` (so the framework can distinguish stale initial bounds from explicitly-set bounds).
- `Close()` marks closed and sets bound to `Timestamp::Done()`.
- `OutputStreamManager` (friend) reads `output_queue_` during propagation and moves packets to downstream `InputStreamManager` deques.
