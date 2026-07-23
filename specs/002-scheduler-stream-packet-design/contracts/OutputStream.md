# Contract: OutputStream

**File**: `graph_runtime/src/stream/output_stream.h`

```cpp
namespace graph::runtime {

class OutputStream {
 public:
  explicit OutputStream(std::string name);

  const std::string& name() const;

  // Connect downstream consumer (called by GraphBuilder)
  void AddDownstreamStream(Stream* stream);

  // Producer interface (called by Node via PacketProducer)
  absl::Status Send(Packet packet);
  void Close();

  // Timestamp bound propagation (called by OutputStreamHandler)
  void SetNextTimestampBound(int64_t timestamp);

 private:
  std::string name_;
  std::vector<Stream*> downstream_streams_;
};

}  // namespace graph::runtime
```

**Semantics**:
- Represents one output port of a Node. May fan-out to multiple downstream Streams.
- `AddDownstreamStream()` is called by GraphBuilder during initialization to register each downstream consumer.
- `Send(Packet)` writes the Packet to every downstream Stream via `Stream::Push()`. Returns the first error encountered (e.g., `ResourceExhaustedError` if any downstream Stream is full).
- `Close()` closes all downstream Streams (end-of-stream propagation).
- `SetNextTimestampBound()` broadcasts the timestamp bound to all downstream Streams, enabling timestamp-based synchronization.
