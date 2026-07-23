# Quickstart: Scheduler-Node-Stream-Packet Module Interaction

This guide describes how the four core modules collaborate in the Graph Runtime execution engine.

## Module Overview

```
Packet                — Atomic data unit (value type, type-erased payload, timestamp)
Stream                — 1:1 bounded FIFO pipe between Node ports (push/pop, back-pressure)
InputStreamManager    — Per-input-port wrapper: arrival callback + timestamp bound tracking
OutputStream          — Per-output-port abstraction: fan-out to N downstream Streams
OutputStreamHandler   — Per-Node output propagation after Process (PostProcess/Flush)
Node                  — Computation unit with Open/Process/Close lifecycle
Scheduler             — State machine (5 states) + stopping_ flag + HandleIdle with reentrancy guard
InputStreamHandler    — Pluggable readiness policy (Default/Immediate/Barrier)
Executor              — Pluggable task execution (ApplicationThread sync / ThreadPool async)
GraphContext          — Per-invocation context passed to Node lifecycle methods
NodeFactory           — Registry for creating Node instances by type name
```

## Module Boundaries

| Module | Owns | Knows About | Does NOT Know |
|--------|------|-------------|---------------|
| Packet | Its payload | Nothing | Other modules |
| Stream | Packet queue | Its name, capacity | Which Nodes are connected |
| InputStreamManager | arrival_callback, bound | Underlying Stream | Scheduler, graph topology |
| OutputStream | downstream Stream list | Multiple Stream refs | Which Nodes consume them |
| OutputStreamHandler | PostProcess logic | Node's output ports | Scheduler internals |
| Node | Its logic | Input/output port refs | Scheduler, other Nodes |
| Scheduler | State machine, stopping_, non_idle_count | Stream/Node events | Business logic |
| InputStreamHandler | Readiness counters | InputStreamManagers | Executor internals |
| Executor | Task queue, thread pool | Function closures | Graph topology, Node state |
| NodeFactory | Registration registry | Type-name → creator mapping | Execution state |

## Data Flow (Event-Driven — No Central Loop)

Schedule() is non-blocking. It registers event observers and returns. All execution is a chain of event reactions:

```
                   ┌──────────────────────────────────────────────────────────────┐
                   │                Scheduler::Schedule(graph)                    │
                   │  (non-blocking — returns immediately)                        │
                   │                                                              │
                   │  1. Build topological layers                                 │
                   │  2. Group sources by source_layer (Phase 2)                  │
                   │  3. Create InputStreamManager per input port (arrival cb)    │
                   │  4. Create OutputStream per output port (fan-out)            │
                   │  5. Create OutputStreamHandler per Node (PostProcess)        │
                   │  6. Register event observers on:                             │
                   │     InputStreamManager::OnPacketEnqueued → NotifyArrival   │
                   │     Stream::Pop   ──► unthrottle check                     │
                   │     Stream::Close ──► mark input done                      │
                   │     Node::Close   ──► completion + [Ph2: layer]            │
                   │  7. Activate all sources                                    │
                   └──────────────────────┬───────────────────────────────────────┘
                                          │
         ┌────────────────────────────────┼────────────────────────────────┐
         ▼                                ▼                                ▼
  ┌──────────────┐                ┌──────────────┐                ┌──────────────┐
  │ Source Node  │                │ Transform    │                │ Sink Node    │
  │ (layer=0)    │                │ Node         │                │              │
  └──────┬───────┘                └──────┬───────┘                └──────┬───────┘
         │                               │                               │
         │  Process()                     │  Process()                    │  Process()
         ▼                               ▼                               ▼
  ┌──────────┐  Push(pkt)  ┌──────────┐  Push(pkt)  ┌──────────┐ Pop() ┌──────────┐
  │  Stream  │ ──────────► │  Stream  │ ──────────► │  Stream  │ ────► │  Stream  │
  │  (queue) │             │  (queue) │             │  (queue) │       │  (queue) │
  └──────────┘             └──────────┘             └──────────┘       └──────────┘
       │                        │                        │                  │
       │ EVENT: Push             │ EVENT: Push             │ EVENT: Pop       │
       ▼                        ▼                        ▼                  ▼
  ┌─────────────────────────────────────────────────────────────────────────────┐
  │                         EVENT REACTIONS                                      │
  │                                                                              │
  │  OpenNode task:                                                              │
  │    Node::Open() → OnNodeOpened(node):                                        │
  │      Source → add to active_sources_ + schedule initial ProcessNode          │
  │      Non-source → register InputStreamManagers → NotifyPacketArrival         │
  │                                                                              │
  │  ProcessNode task [fixes #1-#5]:                                             │
  │    [if stopping_ && Source → CloseNode, skip Process]                        │
  │    FillInputSet():                                                           │
  │      Pop from each InputStreamManager                                        │
  │      [side effect] if queue < max/2 → unthrottle upstream                   │
  │    status = Node::Process()                                                  │
  │    if error (!ok, !Stop): error_callback → kCancelling + stop all sources    │
  │    if Stop: stopping_=true → CloseNode on all active_sources_                │
  │    OutputStreamHandler::PostProcess():                                       │
  │      stream_to_input_mgr_ → direct OnPacketEnqueued() on downstream          │
  │      [no Stream callback needed]                                             │
  │    Source: if !stopping_ → reschedule ProcessNode (loop)                     │
  │                                                                              │
  │  Non-source StatusStop:                                                      │
  │    stopping_=true → CloseNode on all active_sources_                         │
  │                                                                              │
  │  Stream closed:                                                              │
  │    mark done → NotifyPacketArrival(consumer) → may trigger kReadyForClose    │
  │                                                                              │
  │  Node closed:                                                                │
  │    Source → remove from active_sources_                                      │
  │    [Ph2: advance layer]                                                      │
  │    if active_sources_.empty() + all closed + non_idle=0 → state_=kTerminated │
  │                                                                              │
  │  HandleIdle (Schedule/Resume/error/non_idle=0):                              │
  │    [handling_idle_++ guard]                                                  │
  │    if HasError && non_idle_queue_count_==0 → Quit → kTerminated  [fix A]     │
  │    if all closed + idle → Quit → kTerminated                                │
  │    if throttled sources → unthrottle (deadlock break)                        │
  │    [handling_idle_--]                                                        │
  └─────────────────────────────────────────────────────────────────────────────┘
```

## Execution Flow (Event-Driven)

1. **Graph::Initialize()**: Creates Nodes via NodeFactory, creates Streams, wires ports, creates Scheduler with `InputStreamHandler` + `Executor`.

2. **Graph::Start()** → calls `Scheduler::Schedule(graph)` — **non-blocking**, returns immediately:
   a. Build topological layers from Node/Stream wiring.
   b. Group Source Nodes by source_layer (Phase 2 — not used in Phase 1).
   c. Create InputStreamManager per input port, build `stream_to_input_mgr_` mapping.
   d. Create OutputStream (fan-out) + OutputStreamHandler per Node.
   e. Register event observers: InputStreamManager arrival + Stream::Close + Node::Close.
   f. Schedule Open tasks for all source Nodes via `Executor::ScheduleTask()`.
   g. Return.

3. **Graph::WaitUntilDone()** — blocks until completion signal.

4. **Events drive execution** (no central loop):
   - **OpenNode runs**: Node::Open() initializes. OnNodeOpened: Source → `active_sources_` + schedule initial ProcessNode. Non-source → register with InputStreamHandler.
   - **ProcessNode runs** (complete flow):
     * If stopping_ && Source → CloseNode directly (rapid teardown).
     * FillInputSet() → Pop from each InputStreamManager. Side effect: if queue drops below `max_queue_size/2`, unthrottle upstream source.
     * Execute Node::Process(). On error: error_callback → kCancelling + stop all. On StatusStop: stopping_=true + CloseNode on all active sources.
     * OutputStreamHandler::PostProcess() → uses `stream_to_input_mgr_` to call `OnPacketEnqueued()` directly on downstream (no Stream callback needed).
     * Source: if !stopping_, reschedule ProcessNode (loop).
   - **Stream exhausted → OnStreamClosed**: downstream InputStreamHandler may return kReadyForClose → CloseNode scheduled.
   - **Source closed → OnNodeClosed**: removed from `active_sources_`. Phase 2: advance layer. Check completion: all sources done + all non-sources closed + `non_idle_queue_count_ == 0` → state = kTerminated.
   - **Back-pressure**: Push failure → throttle upstream. Pop drain (inside FillInputSet) → unthrottle upstream.
   - **HandleIdle** (triggered from Schedule/Resume/error/non_idle=0): reentrancy-guarded. If HasError && non_idle_queue_count_==0 → Quit → kTerminated. If all done + idle → Quit → kTerminated. If throttled → unthrottle (deadlock break). [fix A]

5. **Graph completion**: All Nodes closed AND no pending tasks → signal fires → WaitUntilDone() unblocks.

6. **Shutdown**: `Scheduler::Shutdown()` sets `stopping_=true`, transitions to `kCancelling`, calls `HandleIdle()`. Tasks already on the Executor drain naturally. `HandleIdle` waits until `non_idle_queue_count_ == 0` before calling `Quit() → kTerminated`. `WaitUntilDone()` blocks transparently. [fix B]

## Key Design Rules

- **Event-driven**: No central polling loop. Execution is a chain of reactions to Stream/Node events.
- **Schedule() is non-blocking**: Sets up observers, returns immediately. `WaitUntilDone()` blocks.
- **No direct Node-to-Node coupling**: All communication goes through Streams.
- **Non-blocking Nodes**: Node::Process() must return promptly; long work must be split across multiple calls.
- **Push-based data flow**: Producer pushes to Stream; event propagates downstream.
- **Pluggable readiness**: InputStreamHandler decouples "when to run" from "how to run".
- **Pluggable execution**: Executor decouples "what to run" from "where to run" (sync/thread pool).
- **Auto-unthrottle**: Back-pressure is self-healing via Pop-triggered + deadlock-triggered unthrottle.

- **Fail-fast errors**: Any Node error aborts the entire graph.
- **CloseNode idempotent** [fix C]: Node::Close() is idempotent — double-close is safe if stopping_ fires concurrently with in-flight ProcessNode.

## Implementation Checklist

- [ ] Packet: value type with timestamp, end-of-stream marker, type-erased payload
- [ ] Stream: bounded queue with Push/Pop/Close, back-pressure via Push error return
- [ ] Node: base class with Open/Process/Close virtual methods, port binding API
- [ ] GraphContext: input accessors, output producers, options access
- [ ] Scheduler: state machine (5 states), `stream_to_input_mgr_` mapping, `active_sources_`, `stopping_` with proactive source closure, `error_callback_`, `HandleIdle` with reentrancy guard and 4 call sites, non-blocking Schedule(), blocking WaitUntilDone(), Pause()/Resume() with stopping guard
- [ ] InputStreamHandler: interface (GetNodeReadiness / FillInputSet / NotifyPacketArrival) + Default/Immediate/Barrier strategies
- [ ] Executor: interface (ScheduleTask) + ApplicationThreadExecutor (Phase 1)
- [ ] NodeFactory: global registry, type-name to creator mapping
