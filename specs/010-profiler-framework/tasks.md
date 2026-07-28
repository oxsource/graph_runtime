# Tasks: Profiler Framework

**Input**: Design documents from `specs/010-profiler-framework/`

**Prerequisites**: plan.md (required), spec.md (required for user stories), research.md, data-model.md, contracts/

**Tests**: Tests are included per user story as specified in plan.md §1.12.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

## Path Conventions

- **Repository root**: `graph_runtime/` (Bazel workspace root)
- **Source**: `src/framework/`
- **Tests**: `src/tests/`
- **Docs**: `specs/010-profiler-framework/`

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Create module directories and initial build files

- [X] T001 Create profiler module directory structure at `src/framework/profiler/` and `src/framework/profiler/reporter/tools/`
- [X] T002 [P] Create initial `src/framework/profiler/BUILD.bazel` with `config_setting(name = "profiler_enabled")`, `graph_profiler_stub` and `graph_profiler_real` targets, and `graph_runtime_select()` alias (stub only initially, real will add srcs as they are implemented)

**Checkpoint**: Module skeleton ready — no logic yet, but build files compile

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core infrastructure that MUST be complete before ANY user story can be implemented

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

- [X] T003 [P] Implement `Clock` abstract interface in `src/framework/profiler/clock.h` with `virtual int64_t TimeNowUsec() = 0`
- [X] T004 [P] Implement `RealClock` in `src/framework/profiler/clock.cc` wrapping `std::chrono::steady_clock`
- [X] T005 [P] Implement `ProfilerConfig` struct in `src/framework/profiler/profiler_config.h` with `enable_profiler`, `histogram_interval_size_usec`, `num_histogram_intervals`, `trace_log_path`
- [X] T006 [P] Implement `TimeHistogram` in `src/framework/profiler/time_histogram.h` and `src/framework/profiler/time_histogram.cc` — `Initialize()`, `AddSample()`, `Reset()`, read-only queries, `std::mutex` protection
- [X] T007 [P] Create public API header `src/framework/public/include/graph_runtime/profiler.h` with `ProfilingContext` forward declaration, `ProfilerConfig` struct, and `NodeProfile` struct (all decorated with `GRAPH_RUNTIME_API`)
- [X] T008 [P] Add `ProfilerConfig profiler_config` field to `GraphConfig` in `src/framework/config/graph_config.h`
- [X] T009 [P] Update umbrella header `src/framework/public/include/graph_runtime/graph_runtime.h` to include `"graph_runtime/profiler.h"`
- [X] T010 [P] Add `#include` for `profiler_config.h` and the `profiler` dep to `src/framework/config/BUILD.bazel` (for `graph_config` target)
- [X] T011 [P] Add `profiler_header` target to `src/framework/public/BUILD.bazel` that wraps the new public header, and add it as a dep to the `runtime` target
- [X] T012 Parse `profiler_config` block in `src/framework/config/json/json_parser.cc` to populate `GraphConfig::profiler_config`

**Checkpoint**: Foundation ready — Clock, ProfilerConfig, TimeHistogram, public header, and config parsing all functional

---

## Phase 3: User Story 1 — Configure and Enable Profiling (P1) 🎯 MVP

**Goal**: A library consumer can configure profiling (via config or programmatic API), run a graph, and query per-node Open/Process/Close timing data from `GetNodeProfiles()`.

**Independent Test**: Write a test graph with a node that sleeps 10ms in Process; enable profiling via config; call `GetNodeProfiles()` and verify the process time is ~10ms.

### Implementation for User Story 1

- [X] T013 [P] [US1] Implement `GraphProfiler` class (real) in `src/framework/profiler/graph_profiler.h` and `src/framework/profiler/graph_profiler.cc` — `Initialize()`, `Start()`, `Stop()`, `Pause()`, `Resume()`, `Reset()`, `SetClock()`, `GetNodeProfiles()`, `SetOpenRuntime()`, `AddProcessSample()`, `SetCloseRuntime()`, plus the `Scope` nested class with `EventType` enum
- [X] T014 [P] [US1] Implement `GraphProfilerStub` class (no-op) in the same `src/framework/profiler/graph_profiler.h` — all methods are empty bodies with the same API shape
- [X] T015 [P] [US1] Implement `ProfilingContext` as a class inheriting from either `GraphProfiler` or `GraphProfilerStub` depending on `GRAPH_RUNTIME_PROFILER_ENABLED` in `src/framework/profiler/graph_profiler.h`
- [X] T016 [US1] Add `ProfilingContext* profiler_` member, `SetProfiler()`, and `GetProfiler()` to `src/framework/scheduler/scheduler.h` and `src/framework/scheduler/scheduler.cc`
- [X] T017 [US1] Add `ProfilingContext* profiler_` member and `SetProfiler()` to `src/framework/scheduler/scheduler_queue.h` and `src/framework/scheduler/scheduler_queue.cc`
- [X] T018 [US1] Instrument `Node::Open` and `Node::Close` in `Scheduler::Schedule()` (sync path) and `Scheduler::Start()` (async path) with `ProfilingContext::Scope` wrappers in `src/framework/scheduler/scheduler.cc`
- [X] T019 [US1] Instrument `Node::Open` and `Node::Process` in `SchedulerQueue::RunNode()` with `ProfilingContext::Scope` wrappers in `src/framework/scheduler/scheduler_queue.cc`
- [X] T020 [US1] Add `std::unique_ptr<ProfilingContext> profiler_` member, `profiler()` accessor, `SetProfilerConfig()`, and `GetNodeProfiles()` to `GraphRuntime` in `src/framework/public/graph_runtime.h` and `src/framework/public/graph_runtime.cc`; wire profiler initialization and pass to scheduler during `Initialize()`
- [X] T021 [US1] Update `src/framework/scheduler/BUILD.bazel` to add `@graph_runtime//src/framework/profiler:graph_profiler` dep on `scheduler_queue` and `scheduler` targets

### Tests for User Story 1

- [X] T022 [US1] Add `profiler_test.cc` target to `src/tests/BUILD.bazel` with dep on `@graph_runtime//src/framework/profiler:graph_profiler` and `@com_google_googletest//:gtest_main`
- [X] T023 [US1] Write `ProfilerDisabledReturnsEmptyProfiles` test: default config → `GetNodeProfiles()` returns empty vector
- [X] T024 [US1] Write `ProfilerEnabledRecordsRuntimes` test: single node with MockClock-simulated durations → verify measured runtimes match expectations
- [X] T025 [US1] Write `ProfilerResetClearsData` test: accumulate samples → `Reset()` → histograms are empty
- [X] T026 [US1] Write `ProfilerConfigFromJson` test: parse JSON with profiler_config block → verify `GraphConfig::profiler_config` fields match

**Checkpoint**: At this point, User Story 1 should be fully functional — profiling can be configured, data collected, and profiles retrieved in memory

---

## Phase 4: User Story 2 — Persist Profile Results for Offline Analysis (P1)

**Goal**: A performance engineer can save profile data to a JSON file via `WriteProfile()` and verify the output is valid.

**Independent Test**: Run graph with profiling enabled; call `WriteProfile("/tmp/test.json")`; verify the file exists, is valid JSON, and contains matching data to `GetNodeProfiles()`.

### Implementation for User Story 2

- [X] T027 [P] [US2] Create `src/framework/profiler/profile_writer.h` with `absl::Status WriteProfile(const std::string& path, const ProfilerConfig& config, const std::vector<NodeProfile>& profiles)` free function
- [X] T028 [US2] Implement JSON serialization in `src/framework/profiler/profile_writer.cc` — lightweight JSON writer that outputs the schema from plan.md §1.9 including `capture_time`, `node_count`, `profiler_config`, and `nodes` array with histogram buckets
- [X] T029 [US2] Add `WriteProfile(const std::string& path = "")` method to `GraphProfiler` in `src/framework/profiler/graph_profiler.h` and `src/framework/profiler/graph_profiler.cc` — delegates to `profile_writer`; when path is empty and `trace_log_path` is set, use `<trace_log_path>/profile_<timestamp>.json`
- [X] T030 [US2] Add `WriteProfile()` no-op to `GraphProfilerStub` in `src/framework/profiler/graph_profiler.h` (returns `OkStatus`)
- [X] T031 [US2] Add `WriteProfile(const std::string& path)` convenience method to `GraphRuntime` in `src/framework/public/graph_runtime.h` and `src/framework/public/graph_runtime.cc` that delegates to `profiler_->WriteProfile(path)`
- [X] T032 [US2] Add `profile_writer.cc` to the `srcs` of `graph_profiler_real` target in `src/framework/profiler/BUILD.bazel`

### Tests for User Story 2

- [X] T033 [US2] Write `ProfilerWriteProfileCreatesFile` test: enable profiler, run graph, `WriteProfile()` → verify JSON file exists and is valid
- [X] T034 [US2] Write `ProfilerWriteProfileReadableByReporter` test: `WriteProfile()` → read file back → verify JSON matches in-memory profiles

**Checkpoint**: Profile data can be persisted to disk as JSON and verified

---

## Phase 5: User Story 3 — Analyze Saved Profiles via CLI Tool (P2)

**Goal**: A developer can run a CLI tool against profile JSON files to produce human-readable reports with node filtering, comparison, and CSV output.

**Independent Test**: Manually create a profile JSON file; run `print_profile --files=<file>`; verify the output table contains correct columns and values.

### Implementation for User Story 3

- [X] T035 [P] [US3] Create `src/framework/profiler/reporter/reporter.h` and `src/framework/profiler/reporter/reporter.cc` with `Reporter` class — `Accumulate(json_path)`, `Report()`, `Compare(baseline)`, `Clear()`, plus `ProfileReport` and `Delta` structs as specified in plan.md §1.10
- [X] T036 [P] [US3] Create `src/framework/profiler/reporter/BUILD.bazel` with `cc_library` target for the `Reporter` library
- [X] T037 [P] [US3] Create `src/framework/profiler/reporter/tools/print_profile.cc` — CLI entry point using manual arg parsing for `--files`, `--node-filter`, `--compare`, `--format=(table|csv)`, `--output`
- [X] T038 [P] [US3] Create `src/framework/profiler/reporter/tools/BUILD.bazel` with `cc_binary` target for `print_profile` depending on the Reporter library

### Tests for User Story 3

- [X] T039 [US3] Write `ReporterAccumulateMultipleFiles` test: create two profile files with different data → `Accumulate` both → verify aggregated stats
- [X] T040 [US3] Write `ReporterCompareRuns` test: create two profile files with known deltas → `Compare()` → verify delta values match
- [X] T041 [US3] Write `PrintProfileCliBasic` test: invoke CLI binary with `--files` → verify stdout contains expected table output

**Checkpoint**: CLI tool produces formatted reports, supports filtering and comparison

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Documentation, build switch verification, agent context update, and final validation

- [X] T042 [P] Update `AGENTS.md` `<!-- SPECKIT START -->` block to reference `specs/010-profiler-framework/plan.md` and add profiler architecture notes (already partially done — verify and complete if needed)
- [X] T043 [P] Write `ProfilerBuildStubIsNoOp` test: verify that default build (without `--define graph_runtime_profiler=true`) produces empty profiles and no crashes
- [X] T044 [P] Write `ProfilerScopeRecordsCorrectly` test: use a `MockClock` implementation to verify Scope constructor/destructor timing dispatch is correct
- [X] T045 Verify all `bazel build //...` and `bazel test //...` pass with both default (`graph_profiler_stub`) and `--define graph_runtime_profiler=true` (`graph_profiler_real`) configurations
- [X] T046 Run `bazel build //src/framework/public:runtime_shared` to verify shared library builds correctly with profiler symbols exported

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — can start immediately
- **Foundational (Phase 2)**: Depends on Setup — BLOCKS all user stories
- **US1 (Phase 3)**: Depends on Foundational — core profiling MVP
- **US2 (Phase 4)**: Depends on US1 (needs `GraphProfiler` and `GetNodeProfiles()`)
- **US3 (Phase 5)**: Depends on US2 (needs `WriteProfile` JSON output)
- **Polish (Phase 6)**: Depends on all user stories

### User Story Dependencies

- **User Story 1 (P1)**: No dependencies on other stories — starts after Foundational
- **User Story 2 (P1)**: Depends on US1 — uses `GraphProfiler` data
- **User Story 3 (P2)**: Depends on US2 — reads profile JSON files
- **User Story 4 (P3)**: Cross-cutting, handled in Polish phase (T043)
- **User Story 5 (P4)**: Cross-cutting, handled in Polish phase (T044)

### Parallel Opportunities

- T002, T003, T004, T005, T006, T007, T008, T009, T010, T011 can all run in parallel
- T013/T014/T015 (GraphProfiler + Stub + ProfilingContext) can run in parallel
- T016 (Scheduler) and T017 (SchedulerQueue) can run in parallel
- Tests within each phase marked [P] can run in parallel
- T035/T036/T037/T038 (Reporter + CLI) can all run in parallel

---

## Parallel Example: User Story 1

```bash
# Launch all model/interface tasks for US1 together:
Task: "Implement GraphProfiler in src/framework/profiler/graph_profiler.h/cc"
Task: "Implement GraphProfilerStub in src/framework/profiler/graph_profiler.h"
Task: "Implement ProfilingContext in src/framework/profiler/graph_profiler.h"
```

```bash
# Launch all integration tasks for US1 together:
Task: "Wire SetProfiler in src/framework/scheduler/scheduler.h/cc"
Task: "Wire SetProfiler in src/framework/scheduler/scheduler_queue.h/cc"
Task: "Wire profiler in src/framework/public/graph_runtime.h/cc"
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup
2. Complete Phase 2: Foundational (CRITICAL — blocks all stories)
3. Complete Phase 3: User Story 1 (core profiling in memory)
4. **STOP and VALIDATE**: Verify `GetNodeProfiles()` returns correct timing data
5. Deploy/demo if ready

### Incremental Delivery

1. Setup + Foundational → Foundation ready
2. Add US1 (Core profiler, P1) → Test independently → working in-memory profiling
3. Add US2 (Persistence, P1) → Test independently → JSON file output
4. Add US3 (CLI tool, P2) → Test independently → full pipeline
5. Polish → Build switch verification, coverage tests

### Parallel Team Strategy

With multiple developers:

1. Developer A: Foundational (Clock, ProfilerConfig, TimeHistogram, public header)
2. Developer B: Foundational (BUILD.bazel, config parsing, GraphConfig integration)
3. Once foundational done:
   - Developer A: US1 (GraphProfiler + Scope)
   - Developer B: US1 (Scheduler/SchedulerQueue/GraphRuntime integration)
4. Developer C: US2 (WriteProfile) after US1 is functional
5. Developer D: US3 (Reporter + CLI) after US2 is functional
