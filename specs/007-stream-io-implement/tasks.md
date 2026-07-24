# Tasks: Stream Input/Output API

## Phase 1 — Stream Infrastructure Fixes

- [ ] T001 Fix `OutputStreamManager::PropagateUpdatesToMirrors` to actually move packets to downstream InputStreamManagers in `src/stream/output_stream_manager.cc`
- [ ] T002 Fix `InputStreamHandler::FillInputSet` to pop packets from input queue and populate the shard in `src/scheduler/input_stream_handler.cc`
- [ ] T003 Fix `OutputStreamHandler::PropagateOutputPackets` to complete the propagation state machine in `src/stream/output_stream_handler.cc`
- [ ] T004 Add `Node::ProcessNode(GraphContext&)` that delegates to `Open()`/`Process()`/`Close()` based on node lifecycle state in `src/node/node.cc`
- [ ] T005 Add `InputStreamHandler*` and `OutputStreamHandler*` members to `Node` in `src/node/node.h`
- [ ] T006 Wire stream mirror connections in `GraphBuilder::Build()` in `src/public/graph_builder.cc`

## Phase 2 — Async Scheduler

- [ ] T007 Add async state machine (kNotStarted, kRunning, kIdle, kTerminated) to `Scheduler` in `src/scheduler/scheduler.h`
- [ ] T008 Implement `Scheduler::Start()` — non-blocking entry, activates source nodes in `src/scheduler/scheduler.cc`
- [ ] T009 Implement `Scheduler::HandleIdle()` — detect idle state, schedule sources/unthrottle in `src/scheduler/scheduler.cc`
- [ ] T010 Implement `Scheduler::ScheduleNodeIfNotThrottled()` — check throttling, enqueue node in `src/scheduler/scheduler.cc`
- [ ] T011 Implement `Scheduler::WaitUntilIdle()` / `WaitUntilDone()` in `src/scheduler/scheduler.cc`
- [ ] T012 Update `SchedulerQueue::AddNode` and `RunNextTask` to support `ProcessNode` items with `GraphContext` in `src/scheduler/scheduler_queue.cc`

## Phase 3 — GraphRuntime API

- [ ] T013 Add `GraphInputStream` map (stream_name → OutputStreamManager) to `GraphRuntime` in `src/public/graph_runtime.h`
- [ ] T014 Implement `GraphRuntime::AddPacketToInputStream(stream_name, packet)` in `src/public/graph_runtime.cc`
- [ ] T015 Implement `GraphRuntime::CloseInputStream(stream_name)` in `src/public/graph_runtime.cc`
- [ ] T016 Implement backpressure: if downstream queue exceeds limit, throttle upstream in `src/public/graph_runtime.cc`
- [ ] T017 Update `GraphRuntime::Initialize()` to set up input stream mirrors and map entries

## Phase 4 — Testing

- [ ] T018 Write unit test: `AddPacketToInputStream` delivers packet to target node in `src/tests/stream_io_test.cc`
- [ ] T019 Write unit test: `CloseInputStream` propagates `StatusStop()` in `src/tests/stream_io_test.cc`
- [ ] T020 Write unit test: concurrent `AddPacketToInputStream` from 8 threads in `src/tests/stream_io_test.cc`
- [ ] T021 Write unit test: error cases (unknown stream, lifecycle order) in `src/tests/stream_io_test.cc`
- [ ] T022 Write integration test: pipeline with external input → output in `src/tests/stream_io_test.cc`
- [ ] T023 Add `stream_io_test` target to `src/tests/BUILD.bazel`
- [ ] T024 Run `bazel test //...` — verify all existing tests still pass
