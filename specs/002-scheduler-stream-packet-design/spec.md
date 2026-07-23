# Feature Specification: Scheduler-Node-Stream-Packet Module Interaction Design

**Feature Branch**: `002-scheduler-stream-packet-design`

**Created**: 2026-07-23

**Status**: Draft

**Input**: "按照001架构设计方案中的计划，新建提案具体设计Scheduler ↔ Node ↔ Stream ↔ Packet，包含各个模块的设计思考及串联"

## User Scenarios & Testing

### User Story 1 - Define Module Interaction Contracts (Priority: P1)

As a module implementer, I want clearly defined interface contracts between Scheduler, Node, Stream, and Packet so that I can implement each module independently without coupling to other modules' internals.

**Why this priority**: Without clear contracts, modules cannot be developed or tested independently, creating coupling risks that defeat the architecture's modularity goal.

**Independent Test**: Each module's interface contract can be verified against its collaborators — e.g., a mock Node can be driven by the real Scheduler, and a mock Scheduler can drive the real Node, proving bidirectional substitution without modifying either module.

**Acceptance Scenarios**:

1. **Given** a Node implementation, **When** the Scheduler calls `Node::Open()` followed by `Node::Process()` with a valid context, **Then** the Node executes its lifecycle correctly and produces output packets via Stream.
2. **Given** a Stream with enqueued Packets, **When** a Node calls `Stream::Pop()`, **Then** it receives the next Packet in order without direct Node-to-Node coupling.
3. **Given** a Packet written to an output Stream, **When** the downstream Node reads from that Stream, **Then** it receives the same Packet content, proving Stream acts as a transparent conduit.

---

### User Story 2 - Data Flow Lifecycle & Scheduling Semantics (Priority: P1)

As a pipeline developer composing Nodes, I want a defined data flow sequence — from Scheduler activation through Node processing to Packet delivery — so that I can reason about when my Node code runs and how data moves between Nodes.

**Why this priority**: Data flow semantics determine correctness of pipeline behavior; without a clear sequence, developers cannot predict execution order or debug data loss/ordering issues.

**Independent Test**: A three-Node linear pipeline (Producer → Transformer → Consumer) can be constructed where each Node's execution order and data output is verified end-to-end — Producer writes Packet A, Transformer receives Packet A and writes Packet B, Consumer receives Packet B. All execution ordering and data integrity can be verified without a full runtime.

**Acceptance Scenarios**:

1. **Given** a graph with three Nodes in sequence (A → B → C), **When** the Scheduler activates Node A, **Then** A processes and its output Packet appears in the Stream connecting A→B, triggering B's activation.
2. **Given** a Node with multiple input Streams, **When** only some inputs have data available, **Then** the Scheduler does NOT activate the Node until all inputs are ready (back-pressure semantics).
3. **Given** a closed Stream (end-of-stream Packet), **When** the downstream Node reads it, **Then** the Node recognizes stream completion and enters its Close lifecycle.

---

### User Story 3 - Node Lifecycle Boundaries (Priority: P2)

As a Node implementer writing business logic, I want a predictable lifecycle — `Open` → `Process` (repeated) → `Close` — so that I know exactly when to allocate resources, transform data, and release resources.

**Why this priority**: Essential for correct Node implementation but can be designed alongside the core contracts in US1.

**Independent Test**: A single Node with lifecycle logging can be driven by a test harness that invokes Open, then multiple Process calls, then Close. The invocation order and count are verified without a full Scheduler or Graph.

**Acceptance Scenarios**:

1. **Given** a Node in Created state, **When** `Open()` is called, **Then** the Node transitions to Opened state and resources are initialized.
2. **Given** a Node in Opened state with all inputs ready, **When** `Process()` is called, **Then** the Node reads inputs, computes, and writes outputs, remaining in Opened state for subsequent calls.
3. **Given** a Node that has completed all processing, **When** `Close()` is called, **Then** the Node transitions to Closed state and resources are released.
4. **Given** a Node in Closed state, **When** `Process()` is called, **Then** it returns an error indicating the Node is no longer active.

---

### Edge Cases

- **Stream back-pressure**: What happens when a Stream reaches maximum queue capacity? Downstream must signal the Scheduler to pause upstream Nodes to prevent memory growth.
- **Infinite stream Node**: How does the system handle a Node that never closes (e.g., a continuously producing source)? The Scheduler must support graceful shutdown via external signal without waiting for natural Node completion.
- **End-of-stream propagation**: When a Packet with end-of-stream marker is enqueued, downstream Nodes must recognize the signal and prepare to close their own output Streams.
- **Source/sink Nodes**: How does the system handle a Node with zero input Streams (source/producer) or zero output Streams (sink/consumer)? The lifecycle must support these degenerate topologies where `Process()` is triggered differently.
- **Node processing failure**: If a Node's processing method returns an error, the Scheduler must decide the failure policy — retry, skip and continue, or fail the entire graph.
- **Empty graph**: What happens when a Graph is initialized with no Nodes? This must be handled gracefully without crashes or hangs.
- **Single Node graph**: A graph with exactly one Node (both source and sink) must function correctly — it has no upstream or downstream dependencies.
- **Disconnected subgraphs**: Two independent subgraphs within the same Graph must execute independently without interference.

## Requirements

### Functional Requirements

- **FR-001**: Scheduler MUST activate a Node only when ALL input Streams have at least one Packet available.
- **FR-002**: Scheduler MUST activate Nodes in topological order — a downstream Node MUST NOT execute before its upstream producer.
- **FR-003**: Stream MUST deliver Packets in FIFO order — the order Packets are pushed MUST match the order they are popped.
- **FR-004**: Stream MUST enforce a configurable maximum queue depth to implement back-pressure.
- **FR-005**: Node MUST implement a three-phase lifecycle: `Open()` (one-time init), `Process()` (repeated execution), `Close()` (one-time teardown).
- **FR-006**: Packet MUST carry a timestamp for ordering and an end-of-stream marker for life cycle signaling.
- **FR-007**: Scheduler MUST support a `Shutdown()` signal that terminates all Node execution and returns control.
- **FR-008**: Scheduler MUST reserve extension points `AddNode(Node*)` and `RemoveNode(Node*)` returning `UnimplementedError` in Phase 1.
- **FR-009**: Node MUST NOT directly reference other Nodes — all inter-Node communication MUST go through Streams.

### Key Entities

- **Packet**: Atomic data unit flowing through Streams. Carries a type-erased payload (`std::any`), a monotonic timestamp, and an end-of-stream marker. Immutable after creation.
- **Stream**: Unidirectional bounded queue connecting a single output port of one Node to an input port of another Node. Maintains FIFO ordering and enforces back-pressure via max queue depth.
- **Node**: Computation unit with named input and output ports bound to Streams. Has a deterministic lifecycle: Created → Opened → Processing → Closed. Contains the business logic executed by the Scheduler.
- **GraphContext**: Per-invocation context passed to Node lifecycle methods. Provides read access to input Packets (by port name) and write access to output producers (by port name), plus Node configuration options.
- **Scheduler**: Execution engine that drives Node activation. Implements topological-layer ordering and single-threaded event loop. Responsible for detecting when a Node's inputs are ready and dispatching the appropriate lifecycle method.
- **NodeFactory**: Registry mapping type-name strings to Node subclass constructors. Enables configuration-driven Node instantiation without hardcoding Node types.

## Success Criteria

### Measurable Outcomes

- **SC-001**: A three-Node linear pipeline (Producer → Transformer → Consumer) processes 1000 Packets end-to-end with zero data loss and correct ordering.
- **SC-002**: Each module (Scheduler, Node, Stream, Packet) can be tested independently with mock collaborators — no integration test required to validate a single module's contract compliance.
- **SC-003**: A Node implementation can be written and verified knowing only the Node lifecycle contract and Stream/Packet interfaces — no knowledge of Scheduler internals required.
- **SC-004**: The Scheduler interface (Schedule, Shutdown, AddNode/RemoveNode stubs) can be implemented by an alternative scheduler without modifying Node, Stream, or Packet code.
- **SC-005**: Packet delivery from one Node's output to another Node's input (via Stream) completes in under 10 microseconds per hop in a single-threaded configuration.

## Assumptions

- All modules operate within a single process; no inter-process or distributed communication in Phase 1.
- Scheduler is single-threaded; no concurrent Node execution in Phase 1.
- Node lifecycle methods are non-blocking and return promptly; long-running work is not supported in Phase 1.
- Graph topology is static during execution — no Nodes or Streams are added or removed at runtime (Phase 2 feature).
- Error handling follows a fail-fast model: any Node failure causes the entire graph to shut down.
- Packet payload is type-erased; callers are responsible for type correctness when reading.
- Stream queue depth is configured per-Stream at graph construction time.
