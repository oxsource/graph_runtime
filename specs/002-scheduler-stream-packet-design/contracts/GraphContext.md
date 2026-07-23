# Contract: GraphContext

**File**: `graph_runtime/src/node/graph_context.h`

```cpp
namespace graph::runtime {

class PacketProducer {
 public:
  explicit PacketProducer(Stream* stream);
  absl::Status Send(Packet packet);
  void Close();

 private:
  Stream* stream_;
};

class GraphContext {
 public:
  GraphContext(
      std::map<std::string, Packet*> inputs,
      std::map<std::string, PacketProducer> outputs,
      const NodeOptions* options);

  Packet* GetInput(const std::string& port_name) const;
  PacketProducer* GetOutput(const std::string& port_name);
  const NodeOptions& options() const;

  const std::map<std::string, Packet*>& inputs() const;
  std::map<std::string, PacketProducer>& outputs();

 private:
  std::map<std::string, Packet*> inputs_;
  std::map<std::string, PacketProducer> outputs_;
  const NodeOptions* options_;
};

}  // namespace graph::runtime
```

**Semantics**:
- Created by Scheduler for each lifecycle invocation, valid only during the scope of that call.
- `inputs` contains one Packet per input port (pre-popped from input Streams by Scheduler).
- `outputs` contains one PacketProducer per output port; Node calls `Send()` to write output.
- After `Process()` returns, Scheduler collects all sent Packets and pushes them to output Streams.
- `NodeOptions` is a generic key-value map — specific keys depend on the Node subclass.
- `PacketProducer::Close()` sends a Packet with `Timestamp::Done()` on that output port, signaling end-of-stream to downstream consumers.
