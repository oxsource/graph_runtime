# Contract: GraphContext

**File**: `graph_runtime/src/node/graph_context.h`

```cpp
namespace graph::runtime {

class PacketProducer {
 public:
  explicit PacketProducer(OutputStream* stream);
  absl::Status Send(Packet packet);
  void Close();   // sets bound = Timestamp::Done() on the OutputStream

 private:
  OutputStream* output_stream_;
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
- `inputs` contains one Packet per input port (pre-popped from input `InputStreamManager` by the Scheduler task runner via `PopPacketAtTimestamp()`).
- `outputs` contains one `PacketProducer` per output port; Node calls `Send()` to write output.
- `PacketProducer::Send(packet)` calls `OutputStream::Send(packet)`, which writes directly to downstream `InputStreamManager`'s deques.
- `PacketProducer::Close()` calls `OutputStream::Close()` which propagates `Timestamp::Done()` as a timestamp bound to all downstream mirrors. **No sentinel Done packet is pushed.**
- After `Process()` returns, the Scheduler's task runner collects all sent Packets from `GraphContext::outputs` — output propagation is inlined, no separate `OutputStreamHandler`.
- `NodeOptions` is a generic key-value map — specific keys depend on the Node subclass.
