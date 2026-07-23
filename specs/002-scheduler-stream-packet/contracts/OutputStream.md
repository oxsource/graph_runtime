# Contract: OutputStream

**File**: `graph_runtime/src/stream/output_stream.h`

```cpp
namespace graph::runtime {

// Pure abstract interface — the only class that Calculator code sees.
// Implemented by OutputStreamShard (per-invocation buffer).
class OutputStream {
 public:
  virtual ~OutputStream() = default;

  virtual const std::string& Name() const = 0;

  // Add data packets
  virtual void AddPacket(const Packet& packet) = 0;
  virtual void AddPacket(Packet&& packet) = 0;

  // Timestamp bound
  virtual void SetNextTimestampBound(Timestamp timestamp) = 0;
  virtual Timestamp NextTimestampBound() const = 0;

  // Lifecycle
  virtual void Close() = 0;
  virtual bool IsClosed() const = 0;

  // Open() only — configure offset and header before any Process call
  virtual void SetOffset(TimestampDiff offset) = 0;
  virtual bool OffsetEnabled() const = 0;
  virtual TimestampDiff Offset() const = 0;
  virtual void SetHeader(const Packet& packet) = 0;
  virtual const Packet& Header() const = 0;
};

}  // namespace graph::runtime
```

**Semantics**:
- This is the **only interface calculators interact with** when writing output. The concrete implementation is `OutputStreamShard` (per-invocation buffer).
- `AddPacket()` enqueues a data packet. Timestamp and type are validated. On error, the error callback is triggered (graph termination).
- `SetNextTimestampBound(bound)` advances the output timestamp bound without sending data. This signals downstream that no packet will arrive before `bound`.
- `Close()` marks the stream as closed. Sets bound to `Timestamp::Done()`.
- `SetOffset(offset)` and `SetHeader(header)` are only valid during `Open()`. After `Open()`, the spec is locked via `LockIntroData()`.
