# Data Model: Graph Runtime

## Entities

### Packet

Atomic data unit flowing through the graph.

| Field | Type | Description |
|-------|------|-------------|
| `data` | `std::any` | Type-erased payload |
| `timestamp` | `int64_t` | Monotonic timestamp for ordering |
| `is_empty` | `bool` | Marker for end-of-stream / flush signals |

**Validation rules**:
- Timestamp MUST be monotonically non-decreasing within a Stream.
- Empty packets signal stream boundaries (open/close).

---

### Stream

Unidirectional data conduit between Node output and Node input.

| Field | Type | Description |
|-------|------|-------------|
| `name` | `std::string` | Unique identifier within Graph |
| `queue_` | `std::queue<Packet>` | Bounded packet buffer |
| `max_queue_size_` | `size_t` | Back-pressure threshold |
| `is_closed_` | `bool` | Stream lifecycle state |

**State transitions**:
- `Open` → `Active` (first packet enqueued) → `Closed` (end-of-stream packet received)

---

### Node

Computational unit executing on Stream data.

| Field | Type | Description |
|-------|------|-------------|
| `name` | `std::string` | Unique identifier within Graph |
| `input_streams_` | `std::vector<Stream*>` | Connected input streams |
| `output_streams_` | `std::vector<Stream*>` | Connected output streams |
| `calculator_` | `std::unique_ptr<Calculator>` | Business logic implementation |

**Lifecycle**:
1. `Open(CalculatorContext&)` — one-time init (read options, open resources)
2. `Process(CalculatorContext&)` — called when all inputs ready
3. `Close(CalculatorContext&)` — one-time teardown

**Validation rules**:
- Node MUST NOT reference other Nodes directly.
- Node MUST have at least one input or output stream.

---

### Calculator

Abstract interface for business logic executed by a Node.

| Method | Description |
|--------|-------------|
| `Open(CalculatorContext&) -> absl::Status` | Validate options, open resources |
| `Process(CalculatorContext&) -> absl::Status` | Read inputs, write outputs |
| `Close(CalculatorContext&) -> absl::Status` | Release resources |

---

### CalculatorContext

Per-invocation context passed to Calculator methods.

| Field | Type | Description |
|-------|------|-------------|
| `inputs` | `std::map<std::string, Packet&>` | Named input packets |
| `outputs` | `std::map<std::string, PacketProducer>` | Named output producers |
| `options` | `const NodeOptions&` | Node configuration from graph config |

---

### GraphConfig

Immutable configuration object produced by parsers, consumed by GraphBuilder.

| Field | Type | Description |
|-------|------|-------------|
| `nodes` | `std::vector<NodeDef>` | Node definitions with type + options |
| `streams` | `std::vector<StreamDef>` | Stream connections between nodes |
| `version` | `std::string` | Config schema version |

**Validation rules**:
- All referenced node names in StreamDef MUST exist in nodes.
- No duplicate node names.
- No duplicate stream names.

---

### Graph

Runtime representation of the pipeline.

| Field | Type | Description |
|-------|------|-------------|
| `nodes_` | `std::vector<std::unique_ptr<Node>>` | Owned nodes |
| `streams_` | `std::vector<std::unique_ptr<Stream>>` | Owned streams |
| `scheduler_` | `std::unique_ptr<Scheduler>` | Assigned scheduler |

**Lifecycle**:
1. `Initialize(const GraphConfig&)` → create nodes + streams, wire them up
2. `Start()` → scheduler begins processing
3. `WaitUntilDone()` → block until all nodes close
4. `Shutdown()` → stop scheduler, release resources

---

### Scheduler

Abstract interface driving Node execution.

| Method | Description |
|--------|-------------|
| `Schedule(Graph&)` | Begin scheduling all nodes |
| `AddNode(Node*)` | Reserved for Phase 2 dynamic graph |
| `RemoveNode(Node*)` | Reserved for Phase 2 dynamic graph |
| `Shutdown()` | Stop all scheduling activity |

**Phase 1 implementation**: Topological-layer scheduler that processes Nodes when all input streams have data.

---

## Relationships

```
GraphConfig  ──parsed-by──>  IGraphConfigParser
                                    │
                          GraphConfig (immutable)
                                    │
                          GraphBuilder::Build()
                                    │
                              RuntimeGraph
                              ├── Node* (N) ──has──> Calculator
                              ├── Stream* (M) ──connects──> Node pairs
                              └── Scheduler* (1)
                                      │
                              drives Node::Process()
                                      │
                              produces/consumes Packet via Stream
```

## State Diagram (Node)

```
  ┌─────────┐   Open()    ┌──────────┐
  │ Created ├────────────►│  Opened   │
  └─────────┘             └────┬─────┘
                               │
                    Process()  │  (all inputs ready)
                     ┌────────▼────────┐
                     │   Processing    │
                     └────────┬────────┘
                              │
                    Process() │  (more input available)
                     ┌────────▼────────┐
                     │   Processing    │  ◄── loop
                     └────────┬────────┘
                              │
                    Close()   │  (end-of-stream on all inputs)
                     ┌────────▼────────┐
                     │    Closed       │
                     └─────────────────┘
```
