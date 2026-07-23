# Contract: SidePacket

**File**: `graph_runtime/src/public/side_packet.h`

```cpp
namespace graph::runtime {

// A set of side packets identified by tag/index.
// Side packets are graph-level constants — they exist for the lifetime of
// a graph run and are accessible across all Node::Open/Process/Close calls.
class PacketSet {
 public:
  const Packet& Get(const std::string& name) const;
  Packet& Get(const std::string& name);
  int NumEntries() const;
};

class OutputSidePacketSet {
 public:
  absl::Status Set(const std::string& name, const Packet& packet);
  absl::Status Set(const std::string& name, Packet&& packet);
  int NumEntries() const;
};

}  // namespace graph::runtime
```

**Semantics**:
- `PacketSet` is an immutable set of input side packets, indexed by tag name. Populated before `Start()` via `GraphRuntime::SetInputSidePacket()`. Available during the entire graph run.
- `OutputSidePacketSet` allows the Node to publish side packets during `Open()` or `Close()`. These are collected by the graph after `WaitUntilDone()` and can be consumed by the user via `GraphRuntime::SetOutputSidePacketCallback()`.
- Side packets are declared in `NodeContract` via `InputSidePackets()` / `OutputSidePackets()` and validated during graph construction.
