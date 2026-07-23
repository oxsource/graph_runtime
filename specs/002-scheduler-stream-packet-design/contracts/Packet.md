# Contract: Packet

**File**: `graph_runtime/src/stream/packet.h`

```cpp
namespace graph::runtime {

class Packet {
 public:
  Packet();
  explicit Packet(Timestamp timestamp);

  // Accessors
  Timestamp timestamp() const;
  bool is_empty() const;

  // End-of-stream marker
  static Packet EndOfStream();

 private:
  Timestamp timestamp_;
  bool is_empty_;
};

}  // namespace graph::runtime
```

**Semantics**:
- `Packet` is a value type — copyable and movable.
- `timestamp` provides ordering within a Stream; monotonically non-decreasing.
- `is_empty = true` signals end-of-stream; downstream Nodes must recognize this and prepare to close.
- The payload is stored via type erasure (implementation detail), accessible through the internal `Any` type.
