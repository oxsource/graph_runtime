# Contract: Stream

**File**: `graph_runtime/src/stream/stream.h`

```cpp
namespace graph::runtime {

class Stream {
 public:
  explicit Stream(std::string name, size_t max_queue_size);

  const std::string& name() const;

  // Producer interface
  absl::Status Push(Packet packet);
  void Close();

  // Consumer interface
  absl::StatusOr<Packet> Pop();
  bool is_closed() const;
  size_t queue_size() const;
  size_t max_queue_size() const;

 private:
  std::string name_;
  std::queue<Packet> queue_;
  size_t max_queue_size_;
  bool is_closed_;
};

}  // namespace graph::runtime
```

**Semantics**:
- `Push()` enqueues a Packet. Returns `FailedPreconditionError` if stream is closed; `ResourceExhaustedError` if queue is full (back-pressure).
- `Pop()` dequeues a Packet. Returns `OutOfRangeError` if queue is empty AND stream is closed (drained).
- `Close()` marks end-of-stream. After close, remaining packets can still be popped until drained.
- `queue_size()` enables the Scheduler to check readiness without popping.
- Stream is single-producer, single-consumer in Phase 1 (no thread safety needed).

**Role distinction**:
- `Stream` is the **low-level 1:1 data pipe** between one output port and one input port.
- Fan-out (one output port → N input ports) is handled by `OutputStream`, which internally writes to multiple Streams.
- Notification on packet arrival is handled by `InputStreamManager`, which wraps Stream's consumer side and fires callbacks.
- Neither `Stream` nor the notification/manager layers know the graph topology — that is Graph's responsibility.
