# Tasks: Stream Input/Output API

**Input**: Design documents from `/specs/007-stream-io-implement/`

**Prerequisites**: spec.md (required), plan.md (required)

## Path Conventions

- Workspace root: `graph_runtime/graph_runtime/`
- Source paths below are relative to workspace root

---

## Phase 1: Foundational — Node & Stream Infrastructure

**Purpose**: Core stream plumbing that ALL user stories depend on. No user-facing API yet.

- [ ] T001 Add `InputStreamHandler*` and `OutputStreamHandler*` members to `Node` in `src/node/node.h`
- [ ] T002 [P] Implement mirror connections during node init using upstream index in `src/node/node.cc` / `src/public/graph_builder.cc`
- [ ] T003 Fix `OutputStreamManager::PropagateUpdatesToMirrors(Timestamp, OutputStreamShard*)` in `src/stream/output_stream_manager.cc`
- [ ] T004 [P] Fix `InputStreamHandler::FillInputSet` through SyncSet with `late_preparation_` in `src/scheduler/input_stream_handler.cc`
- [ ] T005 Add `Node::OpenNode()` / `Node::CloseNode()` / `Node::ProcessNode(CalculatorContext*)` with source/non-source paths in `src/node/node.cc`
- [ ] T006 Fix `OutputStreamHandler::PropagateOutputPackets` (sequential: direct; parallel: state machine) in `src/stream/output_stream_handler.cc`

**Checkpoint**: `bazel build //src/...` passes — stream infrastructure ready

---

## Phase 2: Foundational — Backpressure & Throttling

**Purpose**: Flow control that prevents unbounded buffering. Blocks until phase 1 complete.

- [x] T007 Add `full_input_streams_` map and `OnInputStreamFull`/`OnInputStreamNotFull` callback wiring in `src/public/graph_runtime.cc`
- [x] T008 Implement `GraphInputStreamAddMode` enum (ADD_IF_NOT_FULL / WAIT_TILL_NOT_FULL) — defined but full per-stream handling deferred to Phase 3+; backpressure callbacks active
- [x] T009 Implement `UnthrottleSources()` for deadlock recovery (doubles max queue size on full streams) in `src/public/graph_runtime.cc`
- [x] T010 Wire throttling into `HandleIdle` via `OnInputStreamNotFull` calling `scheduler_->HandleIdle()`

**Checkpoint**: `bazel build //src/...` passes — throttling integrated

---

## Phase 3: Foundational — Async Scheduler

**Purpose**: Non-blocking `Start()` + idle detection + source layer management. Required by ALL user stories.

- [ ] T011 Add full state machine (kNotStarted, kRunning, kPaused, kCancelling, kTerminated) to `Scheduler` in `src/scheduler/scheduler.h`
- [ ] T012 Implement `Scheduler::Start()` — non-blocking, set running, call HandleIdle in `src/scheduler/scheduler.cc`
- [ ] T013 Implement `HandleIdle()` with reentrancy, CleanupActiveSources, TryToScheduleNextSourceLayer in `src/scheduler/scheduler.cc`
- [ ] T014 Implement `TryToScheduleNextSourceLayer()` for source layer activation in `src/scheduler/scheduler.cc`
- [ ] T015 Implement `ScheduleNodeIfNotThrottled()` in `src/scheduler/scheduler.cc`
- [ ] T016 Update `SchedulerQueue::RunCalculatorNode()` — ProcessNode + StatusStop + EndScheduling in `src/scheduler/scheduler_queue.cc`
- [ ] T017 Implement `WaitUntilIdle()` / `WaitUntilDone()` in `src/scheduler/scheduler.cc`

**Checkpoint**: `bazel build //src/...` passes — async scheduler operational

---

## Phase 4: User Story 1 — AddPacketToInputStream (Priority: P1) 🎯 MVP

**Goal**: External consumers can push packets into a named input stream of a running graph.

**Independent Test**: Build a pipeline with a non-source node, call `AddPacketToInputStream("s", pkt)`, verify the node's `Process()` receives the packet.

- [ ] T018 [US1] Add `GraphInputStream` map (stream_name → OutputStreamManager + OutputStreamShard) to `GraphRuntime` in `src/public/graph_runtime.h`
- [ ] T019 [US1] Implement `AddPacketToInputStream(stream_name, packet)` with throttle check in `src/public/graph_runtime.cc`
- [ ] T020 [US1] Update `GraphRuntime::Initialize()` for input stream mirror wiring in `src/public/graph_runtime.cc`
- [ ] T021 [US1] Write unit test: single packet delivered to target node in `src/tests/stream_io_test.cc`
- [ ] T022 [US1] Write unit test: multiple packets delivered in order in `src/tests/stream_io_test.cc`
- [ ] T023 [US1] Write unit test: concurrent AddPacket calls from 8 threads in `src/tests/stream_io_test.cc`

**Checkpoint**: `bazel test //src/tests:stream_io_test` passes — US1 functional

---

## Phase 5: User Story 2 — CloseInputStream (Priority: P1)

**Goal**: External consumers can signal end-of-stream; graph drains and terminates gracefully.

**Independent Test**: Call `CloseInputStream`, verify downstream nodes get `StatusStop()`, `WaitUntilDone()` returns.

- [ ] T024 [P] [US2] Implement `CloseInputStream(stream_name)` with counter + scheduler notification in `src/public/graph_runtime.cc`
- [ ] T025 [US2] Update `HandleIdle()` quit conditions to check `num_closed_graph_input_streams_` in `src/scheduler/scheduler.cc`
- [ ] T026 [US2] Write unit test: CloseInputStream propagates StatusStop downstream in `src/tests/stream_io_test.cc`
- [ ] T027 [US2] Write unit test: all streams closed → WaitUntilDone returns in `src/tests/stream_io_test.cc`

**Checkpoint**: `bazel test //src/tests:stream_io_test` passes — US2 functional

---

## Phase 6: User Story 3 — Graph Lifecycle Management (Priority: P2)

**Goal**: Clear lifecycle `Initialize() → Start() → AddPacket/Close → WaitUntilDone → Shutdown()`.

**Independent Test**: Call lifecycle methods in order, verify each state transition.

- [x] T028 [US3] Write unit test: lifecycle order (Init → Start → Shutdown) succeeds in `src/tests/stream_io_test.cc`
- [x] T029 [US3] Write unit test: AddPacket before Start returns error in `src/tests/stream_io_test.cc`
- [x] T030 [US3] Write unit test: AddPacket on unknown stream returns NotFoundError in `src/tests/stream_io_test.cc`
- [x] T031 [US3] Write unit test: CloseInputStream on unknown stream returns NotFoundError in `src/tests/stream_io_test.cc`

**Checkpoint**: `bazel test //src/tests:stream_io_test` passes ✅ — lifecycle edge cases covered

---

## Phase 7: Examples & Integration

**Purpose**: Polish and verification.

- [x] T032 [P] Create `add_packet_demo` example — feeds packets to a running graph via AddPacketToInputStream in `src/examples/add_packet_demo.cc`
- [x] T033 [P] Add `stream_io_test` and `add_packet_demo` BUILD targets to `src/tests/BUILD.bazel` and `src/examples/BUILD.bazel`
- [x] T034 Run `bazel test //...` — verify all existing tests (14) plus new stream tests pass

---

## Dependencies & Execution Order

### Phase Dependencies

```
Phase 1 (Stream Infrastructure) ──→ Phase 2 (Throttling) ──→ Phase 3 (Async Scheduler)
                                                                     ↓
                                              ┌────────────────────────┼────────────────────────┐
                                              ↓                        ↓                        ↓
                                        Phase 4 (US1)           Phase 5 (US2)           Phase 6 (US3)
                                              ↓                        ↓                        ↓
                                        └────────────────────────────────────────────────────────┘
                                                                     ↓
                                                               Phase 7 (Examples)
```

### User Story Dependencies

- **US1 (Phase 4, P1)**: Depends on Phases 1-3 — MVP
- **US2 (Phase 5, P1)**: Depends on US1 (AddPacketToInputStream must work first to test CloseInputStream)
- **US3 (Phase 6, P2)**: Depends on US1 + US2 (lifecycle spans both)

### Parallel Opportunities

| Phase | [P] tasks |
|-------|-----------|
| Phase 1 | T002 (mirror connect) + T004 (FillInputSet) — different files |
| Phase 7 | T032 (demo) + T033 (BUILD targets) — independent |

### MVP Scope

Phases 1-4 = **23 tasks** — AddPacketToInputStream working, no CloseInputStream, no lifecycle error tests.
