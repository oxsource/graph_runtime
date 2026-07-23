# Contract: Timestamp

**File**: `graph_runtime/src/stream/timestamp.h`

```cpp
namespace graph::runtime {

class Timestamp {
 public:
  // Special values — occupy extremes of int64_t range
  static Timestamp Unset();            // INT64_MIN         — default constructed Packet
  static Timestamp Unstarted();        // INT64_MIN + 1     — input during Open()
  static Timestamp PreStream();        // INT64_MIN + 2     — header data, sole packet on stream
  static Timestamp Min();              // INT64_MIN + 3     — minimum range timestamp
  static Timestamp Max();              // INT64_MAX - 3     — maximum range timestamp
  static Timestamp PostStream();       // INT64_MAX - 2     — stream summary, sole packet
  static Timestamp OneOverPostStream();// INT64_MAX - 1     — internal use only
  static Timestamp Done();             // INT64_MAX         — input during Close()

  // Construction from raw value
  explicit Timestamp(int64_t timestamp);  // CHECK-fails if IsSpecialValue()

  // Accessors
  int64_t Value() const;

  // Classification
  bool IsSpecialValue() const;  // Value() <= Min() || Value() >= Max()
  bool IsRangeValue() const;    // Value() >= Min() && Value() <= Max()
  bool IsAllowedInStream() const;  // Value() >= PreStream() && Value() <= PostStream()
  bool IsEmpty() const;            // Value() == Unset()

  // Stream timestamp arithmetic
  Timestamp NextAllowedInStream() const;
  Timestamp PreviousAllowedInStream() const;

  // Debug
  std::string DebugString() const;

 private:
  explicit Timestamp(int64_t value, bool /*construct_special*/);
  int64_t timestamp_;
};

// TimestampDiff for arithmetic
class TimestampDiff {
 public:
  static TimestampDiff Unset();
  explicit TimestampDiff(int64_t value);
  int64_t Value() const;
  bool IsEmpty() const;
  std::string DebugString() const;

 private:
  int64_t diff_;
};

// Arithmetic operators
TimestampDiff operator-(Timestamp t1, Timestamp t2);
Timestamp operator-(Timestamp t, TimestampDiff d);
Timestamp operator+(Timestamp t, TimestampDiff d);
TimestampDiff operator+(TimestampDiff d1, TimestampDiff d2);

}  // namespace graph::runtime
```

**Special values table**:

| Value | Name | Semantics |
|-------|------|-----------|
| `INT64_MIN` | `Unset()` | Default-constructed Packet, not valid for stream use |
| `INT64_MIN + 1` | `Unstarted()` | Input timestamp during `Open()` |
| `INT64_MIN + 2` | `PreStream()` | Header data, must be the only Packet on the Stream |
| `INT64_MIN + 3` | `Min()` | Minimum range timestamp |
| `INT64_MAX - 3` | `Max()` | Maximum range timestamp |
| `INT64_MAX - 2` | `PostStream()` | Entire-stream summary, must be the only Packet on the Stream |
| `INT64_MAX - 1` | `OneOverPostStream()` | Internal use only — immediately follows PostStream |
| `INT64_MAX` | `Done()` | Input timestamp during `Close()`; signals end-of-stream |

**Semantics**:
- `IsEmpty()` returns true only for `Unset()` — used to check if a Packet is default-constructed or moved-from.
- End-of-stream is signaled by `Timestamp::Done()`, not by a separate boolean. Stream::Pop() returning a Packet with `timestamp().IsDone()` tells the consumer the stream is exhausted.
- `Unstarted()` and `Done()` bracket the Open → Process → Close lifecycle.
- `PreStream()` and `PostStream()` allow single-packet header/summary data on a stream without breaking timestamp ordering.
- The `int64_t` constructor **CHECK-fails** if called with a special value — special values must be created through their named static methods.
- `NextAllowedInStream()` and `PreviousAllowedInStream()` support timestamp bound computation for synchronization policies.
