# Contract: OutputStream

**File**: `graph_runtime/src/stream/output_stream.h`

```cpp
namespace graph::runtime {

class InputStreamManager;

class OutputStream {
 public:
  explicit OutputStream(std::string name);

  const std::string& name() const;

  // --- Graph construction (called by GraphBuilder) ---
  void AddMirror(InputStreamManager* downstream_mgr);

  // --- Producer interface (called by Node via PacketProducer) ---
  absl::Status Send(Packet packet);
  void Close();

  // --- Timestamp bound propagation ---
  void SetNextTimestampBound(Timestamp bound);

  // --- State ---
  bool IsClosed() const;

 private:
  std::string name_;
  bool closed_ = false;

  struct Mirror {
    InputStreamManager* manager;
  };
  std::vector<Mirror> mirrors_;
  Timestamp next_timestamp_bound_{Timestamp::Unset()};
};

}  // namespace graph::runtime
```

**Semantics**:
- Represents one output port of a Node. Packets are sent directly to downstream `InputStreamManager` instances via `mirrors_`. **No intermediate Stream component exists.**
- `AddMirror()` registers a downstream `InputStreamManager`. Called by `GraphBuilder` during initialization to wire the output port to its consumer(s).
- `Send(Packet)` writes the Packet to **all** downstream `InputStreamManager` instances:
  - The **last** mirror receives a **move** (`MovePackets` — zero-copy).
  - All earlier mirrors receive a **copy** (`AddPackets` — shared_ptr bump).
  - After each mirror's `AddPackets`/`MovePackets`, if `*notify` is true, the mirror's `arrival_callback_` fires (→ `InputStreamHandler::NotifyPacketArrival()`).
  - Returns the first error encountered. In Phase 1 errors are exceptional (queue not rejecting packets — back-pressure is managed via callbacks).
- `Close()` sets `closed_ = true` and calls `SetNextTimestampBound(Timestamp::Done())` on all mirrors. `Close()` does NOT push a sentinel Done packet — end-of-stream is signaled by bound propagation alone.
- `SetNextTimestampBound(bound)` propagates the bound to all mirrors via `InputStreamManager::SetNextTimestampBound()`. This is how timestamp bounds advance without sending data.
- The Scheduler's task runner calls `Send()` during output propagation (no separate `OutputStreamHandler` — post-process is inlined in the task runner).
