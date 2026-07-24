# Tasks: Stream Input/Output API

## Phase 1 — Node & Stream Infrastructure

- [ ] T001 Add `InputStreamHandler*` and `OutputStreamHandler*` members to `Node` in `src/node/node.h`
- [ ] T002 Implement mirror connections during node initialization using upstream index from validated graph in `src/node/node.cc` / `src/public/graph_builder.cc`
- [ ] T003 Fix `OutputStreamManager::PropagateUpdatesToMirrors(Timestamp, OutputStreamShard*)` in `src/stream/output_stream_manager.cc` — move packets to mirrors, last mirror uses MovePackets
- [ ] T004 Fix `InputStreamHandler::FillInputSet` through SyncSet with `late_preparation_` support in `src/scheduler/input_stream_handler.cc`
- [ ] T005 Add `Node::OpenNode(CalculatorContext*)`, `Node::CloseNode()`, `Node::ProcessNode(CalculatorContext*)` with source/non-source paths in `src/node/node.cc`
- [ ] T006 Fix `OutputStreamHandler::PropagateOutputPackets` (sequential: direct; parallel: state machine) in `src/stream/output_stream_handler.cc`

## Phase 2 — Backpressure & Throttling

- [ ] T007 Add `full_input_streams_` vector and `UpdateThrottledNodes` callback wiring in `src/public/graph_runtime.cc`
- [ ] T008 Implement `GraphInputStreamAddMode` (ADD_IF_NOT_FULL / WAIT_TILL_NOT_FULL) in `src/public/graph_runtime.cc`
- [ ] T009 Implement `UnthrottleSources()` for deadlock recovery in `src/scheduler/scheduler.cc`
- [ ] T010 Wire throttling into `HandleIdle` in `src/scheduler/scheduler.cc`

## Phase 3 — Async Scheduler

- [ ] T011 Add full state machine (kNotStarted, kRunning, kPaused, kCancelling, kTerminated) to `Scheduler` in `src/scheduler/scheduler.h`
- [ ] T012 Implement `Scheduler::Start()` — non-blocking, set running, call HandleIdle in `src/scheduler/scheduler.cc`
- [ ] T013 Implement `HandleIdle()` with reentrancy protection, CleanupActiveSources, TryToScheduleNextSourceLayer in `src/scheduler/scheduler.cc`
- [ ] T014 Implement `TryToScheduleNextSourceLayer()` for source layer activation in `src/scheduler/scheduler.cc`
- [ ] T015 Implement `ScheduleNodeIfNotThrottled()` in `src/scheduler/scheduler.cc`
- [ ] T016 Update `SchedulerQueue::RunCalculatorNode()` — ProcessNode call + StatusStop handling + EndScheduling in `src/scheduler/scheduler_queue.cc`
- [ ] T017 Implement `WaitUntilIdle()` / `WaitUntilDone()` in `src/scheduler/scheduler.cc`

## Phase 4 — GraphInputStream & Public API

- [ ] T018 Add `GraphInputStream` map (stream_name → {OutputStreamManager*, OutputStreamShard}) to `GraphRuntime` in `src/public/graph_runtime.h`
- [ ] T019 Implement `AddPacketToInputStream(stream_name, packet)` with throttle check in `src/public/graph_runtime.cc`
- [ ] T020 Implement `CloseInputStream(stream_name)` with counter + scheduler notification in `src/public/graph_runtime.cc`
- [ ] T021 Update `GraphRuntime::Initialize()` for input stream mirror wiring in `src/public/graph_runtime.cc`

## Phase 5 — Testing & Examples

- [ ] T022 Create `add_packet_demo` example: feeds packets to a running graph via AddPacketToInputStream in `src/examples/add_packet_demo.cc`
- [ ] T023 Add unit test: AddPacketToInputStream delivers packet to target node in `src/tests/stream_io_test.cc`
- [ ] T024 Add unit test: CloseInputStream propagates StatusStop in `src/tests/stream_io_test.cc`
- [ ] T025 Add unit test: concurrent AddPacket calls from 8 threads in `src/tests/stream_io_test.cc`
- [ ] T026 Add unit test: error cases (unknown stream, lifecycle order) in `src/tests/stream_io_test.cc`
- [ ] T027 Add integration test: pipeline with external input → consumer output in `src/tests/stream_io_test.cc`
- [ ] T028 Add BUILD targets for stream_io_test and add_packet_demo
- [ ] T029 Run `bazel test //...` — verify all existing tests still pass
