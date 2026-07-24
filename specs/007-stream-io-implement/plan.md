# Implementation Plan: Stream Input/Output API

**Date**: 2026-07-24 | **Spec**: [spec.md](./spec.md)

## Summary

Implement `GraphRuntime::AddPacketToInputStream` and `CloseInputStream` to support incremental packet injection into a running graph. Add async scheduling mode. Fix `OutputStreamManager::PropagateUpdatesToMirrors` to actually propagate packets. Wire stream mirrors in `GraphBuilder`.

## Technical Context

**Language/Version**: C++17
**Primary Dependencies**: abseil-cpp (existing)
**Testing**: Google Test (existing)
**Target Platform**: macOS, Linux — cross-platform C++
**Project Type**: C++ Library (Bazel)
**Reference Implementation**: MediaPipe (`/Users/moks/Develop/docker/ubuntu24/codes/mediapipe`)

## Constitution Check

| Principle | Status | Notes |
|-----------|--------|-------|
| I. Stream-Based Graph Architecture | ✅ PASS | This feature directly implements stream-based dataflow between nodes |
| III. Modularity & Extensibility | ✅ PASS | Extends existing scheduler, stream, node modules with async support |
| V. Build System Integrity | ✅ PASS | No new external dependencies |

## Project Structure

### Affected Files

```text
src/
├── public/
│   ├── graph_runtime.h    # + GraphInputStream map, async lifecycle
│   ├── graph_runtime.cc   # + AddPacketToInputStream, CloseInputStream
│   └── graph_builder.cc   # + stream mirror wiring
├── scheduler/
│   ├── scheduler.h        # + async state, HandleIdle, ScheduleNodeIfNotThrottled
│   ├── scheduler.cc       # + async Schedule path, source layer mgmt
│   ├── scheduler_queue.h  # + Item with context, source order
│   └── scheduler_queue.cc # + RunNode body
├── stream/
│   ├── output_stream_manager.cc  # + PropagateUpdatesToMirrors packet transfer
│   ├── output_stream_handler.cc  # + parallel execution state
│   └── input_stream_handler.cc   # + FillInputSet wiring
└── node/
    ├── node.h             # + InputStreamHandler*, OutputStreamHandler*
    └── node.cc            # + ProcessNode wrapper, scheduling state
```

## Architecture Changes

### Scheduler: Dual Mode

```
Current (Sync):
  Schedule() → open all → process loop → close all → return

New (Async):
  Start() → return immediately (worker threads process)
  AddedPacketToInputStream() → wake scheduler
  HandleIdle() → detect no work, yield
  WaitUntilDone() → block until all streams closed
  WaitUntilIdle() → block until no work available
```

### Packet Flow

```
AddPacketToInputStream("stream", pkt)
  → GraphInputStream → OutputStreamShard::AddPacket
  → OutputStreamManager::PropagateUpdatesToMirrors
    → InputStreamManager queue (downstream)
    → InputStreamHandler::ScheduleInvocations
      → Node::ProcessNode (runs calculator)
        → OutputStreamShard (node output)
        → OutputStreamHandler::PropagateOutputPackets
          → next InputStreamManager
```

## Phases

### Phase 1 — Stream Infrastructure Fixes

1. Fix `OutputStreamManager::PropagateUpdatesToMirrors` — move packets to mirrors
2. Fix `InputStreamHandler::FillInputSet` — pop from manager into shard
3. Fix `OutputStreamHandler::PropagateOutputPackets` — full state machine
4. Add `Node::ProcessNode()` — wrapper for Open/Process/Close
5. Wire stream mirrors in `GraphBuilder`

### Phase 2 — Async Scheduler

1. Add async state machine to Scheduler (states: kNotStarted, kRunning, kIdle, kTerminated)
2. Implement `HandleIdle()` — detect idle, schedule sources
3. Implement `ScheduleNodeIfNotThrottled()`
4. Implement source layer management
5. Implement `WaitUntilIdle()` / `WaitUntilDone()`
6. Update `SchedulerQueue::RunNode()` to call `Node::ProcessNode()`

### Phase 3 — GraphRuntime API

1. Add `GraphInputStream` map to `GraphRuntime`
2. Implement `AddPacketToInputStream(stream_name, packet)`
3. Implement `CloseInputStream(stream_name)`
4. Backpressure: throttle upstream when downstream queues full
5. Update `Initialize()` to set up input stream mirrors

### Phase 4 — Testing

1. Unit test: `AddPacketToInputStream` → node receives packet
2. Unit test: `CloseInputStream` → downstream gets StatusStop
3. Unit test: concurrent AddPacket calls (8 threads)
4. Unit test: unknown stream error, lifecycle order errors
5. Integration test: simple pipeline with external input

## Key Risks

- `SchedulerQueue::RunNode` currently has an empty body — the full implementation depends on `GraphContext` having correct input shards
- `GraphContext` may need to be extended to carry input/output shard sets
- The `Node` class doesn't currently have `InputStreamHandler`/`OutputStreamHandler` members — these need to be added and wired during graph construction
- Backpressure handling adds significant complexity — may be deferred to Phase 3 extension
