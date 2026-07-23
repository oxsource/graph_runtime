# Contract: OutputStreamHandler

**File**: `graph_runtime/src/stream/output_stream_handler.h`

```cpp
namespace graph::runtime {

class Node;
class GraphContext;

class OutputStreamHandler {
 public:
  explicit OutputStreamHandler(Node* node);

  // Called after Node::Process() completes
  void PostProcess(GraphContext& context);

  // Called when output streams should be flushed before Close
  void Flush();

 private:
  Node* node_;
};

}  // namespace graph::runtime
```

**Semantics**:
- One `OutputStreamHandler` per Node, created during Scheduler initialization.
- `PostProcess()` is called by the Scheduler's task runner after `Node::Process()` returns. It:
  1. Collects all sent Packets from `GraphContext::outputs`.
  2. For each output port's `PacketProducer`: calls `OutputStream::Send()` to fan-out to downstream streams.
  3. Propagates timestamp bounds from Process results.
  4. Handles end-of-stream markers from output producers.
- `Flush()` is called before `Node::Close()` to ensure all buffered outputs are written.
- Separating output handling from the Scheduler event loop follows the same pluggability pattern as `InputStreamHandler`.
