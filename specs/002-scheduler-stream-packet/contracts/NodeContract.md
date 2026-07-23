# Contract: NodeContract

**File**: `graph_runtime/src/node/node_contract.h`

```cpp
// Uses CollectionItemId from types.h

namespace graph::runtime {

class PacketType {
 public:
  template <typename T>
  PacketType& Set();       // Accept specific type
  PacketType& SetAny();    // Accept any type
  PacketType& SetNone();   // Accept nothing
  PacketType& SetSameAs(const PacketType& other);  // Mirror another port
};

class PacketTypeSet {
 public:
  PacketType& Get(const std::string& port_name);
  PacketType& Get(CollectionItemId id);
  CollectionItemId BeginId() const;
  CollectionItemId EndId() const;
  int NumEntries() const;
};

class NodeContract {
 public:
  // Declare input/output stream port types
  PacketTypeSet& Inputs();
  PacketTypeSet& Outputs();

  // Declare input/output side packet types
  PacketTypeSet& InputSidePackets();
  PacketTypeSet& OutputSidePackets();

  // Access node options (from graph config)
  const NodeOptions& Options() const;
  template <typename T>
  const T& Options() const;

  // Optional settings
  void SetMaxInFlight(int n);
  void SetProcessTimestampBounds(bool enable);  // call Process() on timestamp bound advance

}  // namespace graph::runtime
```

**Semantics**:
- `NodeContract` is the compile-time port type declaration interface, analogous to MediaPipe's `CalculatorContract`. It is passed to the static `Node::GetContract()` method during graph validation.
- `PacketType::Set<T>()` declares that a port accepts/produces type `T`. `SetAny()` accepts any type. `SetNone()` accepts nothing.
- `PacketType::SetSameAs(other)` links a port's type to another port's type — used for pass-through nodes where output type mirrors input.
- `Inputs()` and `Outputs()` return `PacketTypeSet` collections indexed by port name.
- `InputSidePackets()` and `OutputSidePackets()` declare side packet types — graph-level constants that are validated and injected before graph start. Side packet type mismatches produce an error at build time.
- `Options()` provides typed access to node configuration from the graph config. The `Options<T>()` template uses `OptionsRegistry` to deserialize key-value pairs into a typed struct at runtime — providing compile-time field access without protobuf.
- `GetContract()` is called at graph construction time (before any Node instances exist). Type mismatches between connected ports produce an error at graph build time, not at runtime.
