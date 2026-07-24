# Tasks: MediaPipe Gap Closure

**Input**: Design documents from `/specs/009-mediapipe-gap-closure/`

**Prerequisites**: plan.md (required), spec.md (required), research.md

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

## Path Conventions

- Workspace root: `graph_runtime/graph_runtime/`
- All paths relative to workspace root

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Verify clean baseline before any changes.

- [X] T001 Run `bazel build //...` and `bazel test //...` to confirm clean baseline (14+ existing tests pass)

**Checkpoint**: `bazel build //...` zero errors, `bazel test //...` all pass ✅

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Non-blocking for this project — codebase already established. No shared blocking infrastructure needed across user stories.

**⚠️ Note**: Each user story phase can start independently after Phase 1. No Phase 2 tasks required.

**Checkpoint**: Foundation verified — user story implementation can begin ✅

---

## Phase 3: User Story 1 — Side Packet & Output Callback Stub Closure (Priority: P1) 🎯 MVP

**Goal**: Implement the 4 stub methods so `SetOutputStreamCallback`, `ClearOutputStreamCallback`, `SetInputSidePacket`, and `SetOutputSidePacketCallback` actually work.

**Independent Test**: A dedicated test calls each method with a test callback and verifies the callback fires with expected data.

### Implementation for User Story 1

- [X] T002 [P] [US1] Add output callback storage map to `GraphRuntime` in `src/framework/public/graph_runtime.h` — `std::map<std::string, std::function<void(const Packet&)>> output_stream_callbacks_`
- [X] T003 [P] [US1] Add `side_packet_map_` to `GraphRuntime` in `src/framework/public/graph_runtime.h` — `std::map<std::string, Packet> side_packet_map_`
- [X] T004 [P] [US1] Implement `SetOutputStreamCallback` in `src/framework/public/graph_runtime.cc` — store callback in `output_stream_callbacks_`; wire into `OutputStreamHandler::PostProcess` in `src/framework/stream/output_stream_handler.cc`
- [X] T005 [P] [US1] Implement `ClearOutputStreamCallback` in `src/framework/public/graph_runtime.cc` — remove from `output_stream_callbacks_`
- [X] T006 [P] [US1] Implement `SetInputSidePacket` in `src/framework/public/graph_runtime.cc` — store in `side_packet_map_`; wire into `GraphContext` via `Scheduler::Start()` in `src/framework/scheduler/scheduler.cc`
- [X] T007 [US1] Implement `SetOutputSidePacketCallback` in `src/framework/public/graph_runtime.cc` — store callback; trigger during `Node::Close()` via `GraphContext::OutputSidePackets()` in `src/framework/node/node.cc`
- [X] T008 [P] [US1] Test: OutputStreamCallback registration/firing/clearing in `src/tests/scheduler_test.cc`
- [X] T009 [P] [US1] Test: InputSidePacket injection and reading in `src/tests/integration_test.cc`

**Checkpoint**: All 4 stub methods implemented and tested — `bazel test //src/tests:scheduler_test` and `bazel test //src/tests:integration_test` pass with callback verification ✅

---

## Phase 4: User Story 2 — Scheduler Async Path Fix (Priority: P1) 🎯 MVP

**Goal**: Fix `WaitUntilDone()` to work correctly when graph is started via `Start()` and fed packets via `AddPacketToInputStream`. Demos use `WaitUntilDone()` instead of `Shutdown()`.

**Independent Test**: `add_packet_demo.cc` modified to call `WaitUntilDone()` instead of `Shutdown()` and exits normally.

### Implementation for User Story 2

- [ ] T010 [US2] Fix `Start()` async path in `src/framework/scheduler/scheduler.cc` — build event-driven scheduling loop that drives nodes to completion rather than relying solely on `HandleIdle()`
- [ ] T011 [US2] Fix hardcoded `Timestamp(1)` in `src/framework/scheduler/scheduler_queue.cc:79` — use real/assigned timestamp from scheduler context
- [ ] T012 [US2] Fix `WaitUntilDone()` in `src/framework/scheduler/scheduler.cc` — ensure `cv_` is notified when async path reaches `kTerminated`
- [ ] T013 [US2] Update `add_packet_demo.cc` in `src/examples/add_packet_demo.cc` — replace `Shutdown()` with `WaitUntilDone()`
- [ ] T014 [US2] Update `async_pipeline_demo.cc` in `src/examples/async_pipeline_demo.cc` — replace `Shutdown()` with `WaitUntilDone()`

**Checkpoint**: Both demos use `WaitUntilDone()` and exit normally — `bazel test //...` all pass ✅

---

## Phase 5: User Story 3 — Graph Lifecycle Queries (Priority: P2)

**Goal**: Expose `WaitForIdle()`, `HasGraphFinished()`, `GetGraphState()` for non-blocking graph progress monitoring.

**Independent Test**: A test starts a graph, waits briefly, calls `GetGraphState()` and `HasGraphFinished()`, adds more packets, and verifies state transitions.

### Implementation for User Story 3

- [ ] T015 [P] [US3] Implement `WaitForIdle()` in `src/framework/scheduler/scheduler.cc` — condition variable triggered when `IsIdle()` returns true
- [ ] T016 [P] [US3] Implement `HasGraphFinished()` in `src/framework/scheduler/scheduler.cc` — combine `state()` with input stream close state
- [ ] T017 [US3] Expose `GetGraphState()` via `GraphRuntime` in `src/framework/public/graph_runtime.h` and `src/framework/public/graph_runtime.cc` — delegate to `Scheduler::state()`
- [ ] T018 [P] [US3] Test: Lifecycle query API state transitions in `src/tests/scheduler_test.cc`

**Checkpoint**: `WaitForIdle()`, `HasGraphFinished()`, `GetGraphState()` all functional — test verifies state transitions ✅

---

## Phase 6: User Story 4 — Pause/Resume (Priority: P2)

**Goal**: Implement `Pause()` and `Resume()` on `Scheduler` and expose through `GraphRuntime` — currently returning `UnimplementedError`.

**Independent Test**: A test starts a graph, pauses it, verifies no processing occurs, resumes it, and verifies processing continues.

### Implementation for User Story 4

- [ ] T019 [US4] Implement `Pause()` in `src/framework/scheduler/scheduler.cc` — set state to `kPaused`, block processing loop, notify via condition variable
- [ ] T020 [US4] Implement `Resume()` in `src/framework/scheduler/scheduler.cc` — set state to `kRunning`, notify waiting threads, trigger `HandleIdle()`
- [ ] T021 [P] [US4] Test: Pause/resume state machine and processing halt/resume in `src/tests/scheduler_test.cc`

**Checkpoint**: `Pause()` and `Resume()` work — test verifies no processing during pause and correct resume ✅

---

## Phase 7: User Story 5 — Config Validation Completeness (Priority: P2)

**Goal**: Add connectivity validation, cycle detection, and runtime type checking to `ConfigValidator` and scheduler.

**Independent Test**: A test provides a config with a missing input stream node and verifies the validator returns a clear error.

### Implementation for User Story 5

- [ ] T022 [P] [US5] Add connectivity validation in `src/framework/config/config_validator.cc` — each `NodeDef::input_stream` must trace to an upstream node's output stream
- [ ] T023 [P] [US5] Add cycle detection in `src/framework/config/config_validator.cc` — DFS-based directed cycle detection on node graph
- [ ] T024 [US5] Add runtime type checks in `src/framework/scheduler/scheduler.cc` — validate `Packet` type against `NodeContract` during `Process` invocation
- [ ] T025 [P] [US5] Test: Connectivity error, cycle error, and type mismatch detection in `src/tests/config_parser_test.cc`

**Checkpoint**: `ConfigValidator` detects connectivity/cycle errors — test verifies clear error messages ✅

---

## Phase 8: Polish & Cross-Cutting Concerns

**Purpose**: Build cleanliness and full validation.

- [ ] T026 [P] Run `bazel build //...` — zero errors, zero warnings
- [ ] T027 Run `bazel test //...` — all tests pass (both existing 14+ and new)
- [ ] T028 [P] Verify no stale `UnimplementedError` or empty stub bodies remain in `src/framework/`

**Checkpoint**: `bazel build //... && bazel test //...` zero errors, zero warnings ✅

---

## Dependencies & Execution Order

### Phase Dependencies

```
Phase 1 (Setup)
    │
    ├──▶ Phase 3 [US1] (Stub Closure)
    │
    └──▶ Phase 4 [US2] (Scheduler Fix) ──▶ Phase 5 [US3] (Lifecycle Query)
    │                                          │
    │                                          └──▶ Phase 6 [US4] (Pause/Resume)
    │
    └──▶ Phase 7 [US5] (Config Validation) ──▶ Phase 8 (Polish)
```

- **Phase 1 (Setup)**: No dependencies — start immediately
- **Phase 3 [US1]**: Depends on Phase 1 only — independent of other stories
- **Phase 4 [US2]**: Depends on Phase 1 only — independent of US1
- **Phase 5 [US3]**: Depends on Phase 4 (needs working `WaitUntilDone`)
- **Phase 6 [US4]**: Depends on Phase 4 (needs working scheduler async path)
- **Phase 7 [US5]**: Depends on Phase 1 only — fully independent
- **Phase 8 (Polish)**: Depends on all phases complete

### User Story Dependencies

- **US1 (P1)**: No dependencies on other stories — fully independent
- **US2 (P1)**: No dependencies on other stories — fully independent
- **US3 (P2)**: Depends on US2 (`WaitUntilDone` must work first)
- **US4 (P2)**: Depends on US2 (scheduler async path must work)
- **US5 (P2)**: No dependencies on other stories — fully independent

### Within Each User Story

- Implementation before phase checkpoint
- Story independent before moving to next priority

---

## Parallel Opportunities

| Phase | [P] Tasks | Rationale |
|-------|-----------|-----------|
| Phase 3 (US1) | T002, T003 (header fields) | Different fields in same file — can be done together |
| Phase 3 (US1) | T004, T005, T006 (methods) | Different methods, different files |
| Phase 3 (US1) | T008, T009 (tests) | Different test files |
| Phase 4 (US2) | T010, T011, T012 (scheduler core) | Different aspects of scheduler fix |
| Phase 5 (US3) | T015, T016 (different methods) | Different methods, same/different files |
| Phase 7 (US5) | T022, T023 (validation logic) | Different validation checks |
| Phase 8 | T026, T028 (verification) | Independent checks |

### Cross-Story Parallelism

US1, US2, and US5 can be implemented in parallel by different developers:

```text
Developer A: Phase 3 [US1] Stub Closure
Developer B: Phase 4 [US2] Scheduler Fix
Developer C: Phase 7 [US5] Config Validation
```

US3 and US4 must wait for US2 completion but can be parallelized together.

---

## Parallel Example: User Story 1

```bash
# Add header storage fields:
Task: T002 Add output_stream_callbacks_ to graph_runtime.h
Task: T003 Add side_packet_map_ to graph_runtime.h

# Implement all 4 stub methods:
Task: T004 Implement SetOutputStreamCallback in graph_runtime.cc + output_stream_handler.cc
Task: T005 Implement ClearOutputStreamCallback in graph_runtime.cc
Task: T006 Implement SetInputSidePacket in graph_runtime.cc + scheduler.cc
Task: T007 Implement SetOutputSidePacketCallback in graph_runtime.cc + node.cc

# Write tests in parallel:
Task: T008 Test OutputStreamCallback in scheduler_test.cc
Task: T009 Test InputSidePacket in integration_test.cc
```

---

## Implementation Strategy

### MVP First (US1 + US2 Only)

1. Complete Phase 1: Setup (baseline verification)
2. Complete Phase 3: [US1] Stub Closure (4 stub methods + tests)
3. Complete Phase 4: [US2] Scheduler Fix (WaitUntilDone + demos)
4. **STOP and VALIDATE**: All stubs work, `WaitUntilDone()` functional, demos exit normally
5. Deploy/demo if ready — this covers the P1 critical gaps

### Incremental Delivery

1. Phase 1 + Phase 3 (US1) → Stub closure complete → 4 stub methods work
2. Add Phase 4 (US2) → WaitUntilDone works → Demos use proper API → **MVP!**
3. Add Phase 5 (US3) → Lifecycle queries available → Non-blocking monitoring
4. Add Phase 6 (US4) → Pause/Resume works → Execution control
5. Add Phase 7 (US5) → Config validation complete → Defensive programming
6. Add Phase 8 → Build health → Zero warnings, all tests pass

### Parallel Team Strategy

With multiple developers:

1. Team completes Phase 1 together
2. Developer A: Phase 3 [US1] Stub Closure
3. Developer B: Phase 4 [US2] Scheduler Fix + Phase 5 [US3] Lifecycle
4. Developer C: Phase 7 [US5] Config Validation
5. Developer D (after US2): Phase 6 [US4] Pause/Resume
6. Team integrates and validates Phase 8 together

---

## Notes

- [P] tasks = different files, no dependencies
- [Story] label maps task to specific user story for traceability
- Each user story is independently completable and testable
- US1, US2, US5 are fully independent and can be done in parallel
- US3 depends on US2; US4 depends on US2
- Commit after each task or logical group
- Stop at any checkpoint to validate story independently