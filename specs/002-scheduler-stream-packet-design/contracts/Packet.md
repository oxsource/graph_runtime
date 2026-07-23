# Contract: Packet

**File**: `graph_runtime/src/stream/packet.h`

```cpp
namespace graph::runtime {

class Packet {
 public:
  // Default constructor — IsEmpty() == true, timestamp() == Timestamp::Unset()
  Packet();

  // Shallow copy — O(1) shared_ptr increment, shares underlying data
  Packet(const Packet&) = default;
  Packet& operator=(const Packet&) = default;

  // Move — transfers holder, source becomes empty
  Packet(Packet&&) = default;
  Packet& operator=(Packet&&) = default;

  // Factory — preferred creation
  template <typename T, typename... Args>
  static Packet MakePacket(Args&&... args);

  // Factory — adopt an already-owned pointer
  template <typename T>
  static Packet Adopt(const T* ptr);

  // Timestamp access and manipulation
  Timestamp timestamp() const;
  Packet At(Timestamp timestamp) const&;  // returns copy with new timestamp
  Packet At(Timestamp timestamp) &&;      // sets timestamp on *this then moves

  // State
  bool IsEmpty() const;  // holder_ == nullptr (default/moved/consumed)

  // Payload access — non-FATAL, returns Status on error
  template <typename T>
  absl::StatusOr<const T&> Get() const;

  // Type validation only — no data access
  template <typename T>
  absl::Status ValidateAsType() const;

  // Shared access — returns shared_ptr retaining the Packet's holder
  template <typename T>
  absl::StatusOr<std::shared_ptr<const T>> Share() const;

  // Equality — pointer equality (same Holder instance)
  friend bool operator==(const Packet& a, const Packet& b);
  friend bool operator!=(const Packet& a, const Packet& b);

  // Debug
  std::string DebugString() const;
  std::string DebugTypeName() const;

 private:
  // Type-erased storage via shared_ptr<HolderBase>
  // Phase 1: std::any internally; Phase 2: custom Holder<T> hierarchy with TypeId
  struct HolderBase {
    virtual ~HolderBase() = default;
    virtual const void* Ptr() const = 0;
    virtual const std::type_info& Type() const = 0;
  };

  template <typename T>
  struct Holder : HolderBase {
    explicit Holder(const T* ptr) : ptr_(ptr) {}
    ~Holder() override { delete ptr_; }
    const void* Ptr() const override { return ptr_; }
    const std::type_info& Type() const override { return typeid(T); }
    std::unique_ptr<const T> ptr_;
  };

  std::shared_ptr<const HolderBase> holder_;
  Timestamp timestamp_;
};

}  // namespace graph::runtime
```

**Semantics**:
- `Packet()` constructs an empty Packet: `IsEmpty() == true`, `timestamp() == Timestamp::Unset()`. Any `Get<T>()` call returns an error.
- Copy is **shallow** — both copies share the same `HolderBase` via `shared_ptr` (O(1), reference-counted). The underlying data is never deep-copied.
- Move transfers the `shared_ptr`, leaving the source empty (`IsEmpty() == true`, `timestamp() == Timestamp::Unset()`).
- `MakePacket<T>(args...)` allocates a new `T` on the heap, wraps it in `Holder<T>`, returns a Packet. Preferred factory.
- `Adopt<T>(ptr)` takes ownership of a raw `T*`, wrapping it in `Holder<T>`. The caller MUST have allocated `ptr` with `new`.
- `At(timestamp)` returns a Packet with the given timestamp. The const& version copies `*this`; the && version moves `*this`.
- `Get<T>()` checks `IsEmpty()` (error if empty) and type match (error if mismatch). On success returns `const T&`. Does **not** FATAL — returns `absl::StatusOr`.
- `ValidateAsType<T>()` is a lightweight type check without data access. Useful for pre-validation in `InputStreamHandler`.
- `Share<T>()` returns a `shared_ptr<const T>` that keeps the holder alive via `shared_ptr` aliasing.
- `operator==` compares holder pointers (same underlying data instance). Two Packets containing `int(5)` are **not** equal unless they share the same `Holder<int>`.
- End-of-stream is detected via `timestamp().IsDone()`, not via a separate empty flag. `IsEmpty()` only indicates no payload.
- Printable via `DebugString()`: `"Packet with timestamp: 12345 and type: int"`, or `"Packet with timestamp: Timestamp::Done() and type: int"`, or `"Packet with timestamp: Unset and no data"` when empty.
