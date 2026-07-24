# Implementation Plan: Stream Input/Output API

**Date**: 2026-07-24 | **Spec**: [spec.md](./spec.md)

## Summary

Implement `GraphRuntime::AddPacketToInputStream` and `CloseInputStream` for incremental packet injection. Add async scheduler mode. Fix stream propagation infrastructure. Reference: MediaPipe CalculatorGraph.

## Technical Context

**Language/Version**: C++17
**Primary Dependencies**: abseil-cpp (existing)
**Testing**: Google Test (existing)
**Reference**: MediaPipe at `/Users/moks/Develop/docker/ubuntu24/codes/mediapipe`

## Constitution Check

| Principle | Status |
|-----------|--------|
| I. Stream-Based Graph Architecture | ✅ PASS |
| III. Modularity & Extensibility | ✅ PASS |
| V. Build System Integrity | ✅ PASS |

## MediaPipe Alignment — Corrections to Initial Plan

Initial plan had 12 gaps vs MediaPipe reference. Corrected below:

### Stream Infrastructure

- **`PropagateUpdatesToMirrors`**: Signature is `(Timestamp next_timestamp_bound, OutputStreamShard* shard)`. Last mirror uses `MovePackets`, others `AddPackets`. Must update `next_timestamp_bound_` under mutex.
- **`FillInputSet`**: Through `SyncSet` abstraction. Supports `late_preparation_` flag. Also reports `num_packets_dropped` and `stream_is_done`.
- **`Node::ProcessNode`**: Only executes `Process()`. `OpenNode()` and `CloseNode()` are separate methods. Source vs non-source nodes have fundamentally different paths.
- **`PropagateOutputPackets`**: Default (sequential) mode has NO state machine. State machine only for parallel execution.
- **Mirror connection**: Done during node initialization (after flat manager arrays created), using upstream index from validated graph.

### Async Scheduler

- **State machine**: Full set is `kNotStarted`, `kRunning`, `kPaused`, `kCancelling`, `kTerminated`. `kIdle` is a condition (`non_idle_queue_count_ == 0`), NOT a state.
- **`HandleIdle()`**: Reentrancy-protected. Manages: `CleanupActiveSources()`, quit conditions, `TryToScheduleNextSourceLayer()`, `UnthrottleSources()`.
- **Source layer management**: Three tiers: `unopened_sources_` (not yet opened, sorted by `source_layer()`) → `sources_queue_` (opened but not activated) → `active_sources_` (currently running).
- **`TryToScheduleNextSourceLayer()`**: Pauses queue, adds layer sources atomically, resumes queue.

### GraphRuntime API

- **`GraphInputStream`**: Wraps `OutputStreamManager*` + reusable `OutputStreamShard`. Key: external packets are injected as if from an output stream.
- **`AddPacketToInputStream`**: Must check throttling first (`ADD_IF_NOT_FULL` / `WAIT_TILL_NOT_FULL` mode), use virtual node ID for throttle tracking, error-check after add, then `PropagateUpdatesToMirrors`, then notify scheduler via `AddedPacketToInputStream()`.
- **`CloseInputStream`**: Increments counter; when all closed, calls `scheduler_.ClosedAllGraphInputStreams()`.
- **Backpressure**: Full system: `full_input_streams_` vector, `UpdateThrottledNodes` callbacks, `GraphInputStreamAddMode`, `UnthrottleSources()` for deadlock recovery.

## Architecture

### Packet Flow (MediaPipe-aligned)

```
AddPacketToInputStream("s", pkt)
  → GraphInputStream::AddPacket (→ OutputStreamShard)
  → OutputStreamManager::PropagateUpdatesToMirrors(bound, &shard)
    → InputStreamHandler::AddPackets/MovePackets (per mirror)
      → InputStreamManager queue (thread-safe, arrival_callback)
        → InputStreamHandler::ScheduleInvocations
          → Node::Process(GraphContext)
            → OutputStreamShard (node output)
            → OutputStreamHandler::PropagateOutputPackets
              → OutputStreamManager::PropagateUpdatesToMirrors (downstream)
```

### Scheduler: Dual Mode

```
Current (Sync):
  Schedule() → open all → process loop → close all → return

New (Async):
  Start() → SetRunning → HandleIdle → return
  AddedPacketToInputStream → wake scheduler via AddedPacketToInputStream
  HandleIdle() → CleanupActiveSources → schedule layers → unthrottle → yield
  WaitUntilIdle() → block until non_idle_queue_count_ == 0
  WaitUntilDone() → block until state == kTerminated
```

## Phases (Corrected Order)

Dependencies dictate this order:

### Phase 1 — Node & Stream Infrastructure

1. Add `InputStreamHandler*` / `OutputStreamHandler*` members to `Node`
2. Implement mirror connections during node init (upstream index from validated graph)
3. Fix `OutputStreamManager::PropagateUpdatesToMirrors` (two params, MovePackets, bound)
4. Fix `InputStreamHandler::FillInputSet` through SyncSet (late_preparation_, PopPacketAtTimestamp)
5. Add `Node::OpenNode()` / `Node::CloseNode()` / `Node::ProcessNode(GraphContext*))` with source/non-source paths
6. Fix `OutputStreamHandler::PropagateOutputPackets` (sequential: direct; parallel: state machine)

### Phase 2 — Backpressure & Throttling

1. Add `full_input_streams_` vector and `UpdateThrottledNodes` callback wiring
2. Implement `GraphInputStreamAddMode` (ADD_IF_NOT_FULL / WAIT_TILL_NOT_FULL)
3. Implement `UnthrottleSources()` for deadlock recovery
4. Wire throttling into `HandleIdle`

### Phase 3 — Async Scheduler

1. State machine (kNotStarted, kRunning, kPaused, kCancelling, kTerminated)
2. `Scheduler::Start()` — non-blocking, set running, HandleIdle
3. `HandleIdle()` — reentrancy-protected, cleanup, schedule layers, unthrottle
4. `TryToScheduleNextSourceLayer()` — source layer activation
5. `ScheduleNodeIfNotThrottled()` — throttle check + enqueue
6. `SchedulerQueue::RunNode()` — ProcessNode + StatusStop handling
7. `WaitUntilIdle()` / `WaitUntilDone()` — condition variable + ApplicationThreadAwait

### Phase 4 — GraphInputStream & Public API

1. Add `GraphInputStream` map (stream_name → {OutputStreamManager*, OutputStreamShard})
2. Implement `AddPacketToInputStream(stream_name, packet)` with throttle check
3. Implement `CloseInputStream(stream_name)` with counter + scheduler notification
4. Update `GraphRuntime::Initialize()` for input stream mirror setup

### Phase 5 — Testing & Examples

1. Unit tests for each phase (see tasks.md)
2. New example: `add_packet_demo` — feeds packets to a running graph
3. Integration test: pipeline with external input → output

## Key Dependencies

```
Phase 1 (Node/Stream) ──→ Phase 2 (Throttle) ──→ Phase 3 (Scheduler) ──→ Phase 4 (API)
```

Each phase block the next. Testing in Phase 5 validates the full chain.
