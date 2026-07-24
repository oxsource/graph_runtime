# Contract: InputStream

**File**: `graph_runtime/src/stream/input_stream.h`

```cpp
namespace graph::runtime {

// Pure abstract interface — the class that Node code reads from.
// Implemented by InputStreamShard (per-invocation view).
class InputStream {
 public:
  virtual ~InputStream() = default;

  virtual const std::string& Name() const = 0;

  // Access the current packet
  virtual const Packet& Value() const = 0;
  virtual Packet& Value() = 0;  // mutable — allows moving out

  template <typename T>
  const T& Get() const {
    return Value().template Get<T>();
  }

  // State
  virtual bool IsEmpty() const = 0;   // Value().IsEmpty()
  virtual bool IsDone() const = 0;    // stream closed and no more packets

  // Metadata
  virtual Packet Header() const = 0;  // header set by upstream during Open()
};

}  // namespace graph::runtime
```

**Semantics**:
- This is the **only interface nodes interact with** when reading input. The concrete implementation is `InputStreamShard` (per-invocation view).
- `Value()` returns the current packet for this invocation. If no packet was sent at the current timestamp, returns an empty Packet (`IsEmpty() == true`).
- `IsDone()` returns true when the underlying stream is closed and all packets have been consumed. Guaranteed true during `Close()`.
- `Header()` returns the header packet set by the upstream node's `Open()`.
- `Get<T>()` is syntactic sugar for `Value().Get<T>()`.
