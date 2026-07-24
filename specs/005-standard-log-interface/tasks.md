# Tasks: Standard Log Interface

**Input**: Design documents from `/specs/005-standard-log-interface/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/

**Tests**: Not requested — test tasks excluded by default.

**Organization**: Tasks grouped by user story for independent implementation.

## Path Conventions

- Workspace root: `graph_runtime/graph_runtime/`
- Bazel package paths: `//src/public:log_interface`, `//src/public:runtime`
- Source in `graph_runtime/graph_runtime/src/` (relative paths below are from workspace root)

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Create logger module structure, Bazel targets, and internal headers

- [x] T001 Add `HookType` enum and `GraphHookEntity` struct to `graph_runtime/graph_runtime/src/public/graph_runtime.h`
- [x] T002 Create internal logger header at `graph_runtime/graph_runtime/src/public/logger.h` with `LogLevel` enum, `LogMessage` struct, and `Logger` class (singleton + `Log(LogLevel, tag, content)` + convenience methods)
- [x] T003 Create public logger header at `graph_runtime/graph_runtime/src/public/include/graph_runtime/logger.h` with `Logger` class exposing only 5 convenience methods (Debug, Info, Warn, Error, Fatal)
- [x] T004 [P] Update umbrella header `graph_runtime/graph_runtime/src/public/include/graph_runtime/graph_runtime.h` to include `graph_runtime/logger.h`
- [x] T005 Update `graph_runtime/graph_runtime/src/public/BUILD.bazel` — add `cc_library` target `log_interface`, add `log_interface` to `runtime_internal` and `runtime` deps; add `include/graph_runtime/logger.h` to `runtime` hdrs

**Checkpoint**: `bazel build //src/public:log_interface` and `bazel build //src/public:runtime` both pass ✅

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core logger implementation that all user stories depend on

- [x] T006 Implement `LogLevel` enum and `LogLevelToString` in `graph_runtime/graph_runtime/src/public/logger.h`/`logger.cc` with kFatal=0, kError=1, kWarn=2, kInfo=3, kDebug=4 (syslog order)
- [x] T007 Implement `LogMessage` struct in `graph_runtime/graph_runtime/src/public/logger.h` with fields: `LogLevel level`, `const char* tag`, `const char* message`, `int64_t timestamp_ms`
- [x] T008 Implement `Logger::Instance()` — Meyer singleton (C++11 function-local static) in `graph_runtime/graph_runtime/src/public/logger.cc`
- [x] T009 Implement timestamp formatting helper (ISO 8601 with milliseconds) in `graph_runtime/graph_runtime/src/public/logger.cc`
- [x] T010 Implement log line format assembly: `[TAG] [LEVEL] YYYY-MM-DD HH:MM:SS.mmm content\n` in `graph_runtime/graph_runtime/src/public/logger.cc`
- [x] T011 Implement `Logger::Log(LogLevel, tag, content)` — format the log line, write to stdout (kDebug/kInfo/kWarn) or stderr (kError/kFatal) protected by `absl::Mutex` in `graph_runtime/graph_runtime/src/public/logger.cc`
- [x] T012 [P] Implement 5 static convenience methods (Debug, Info, Warn, Error, Fatal) as thin wrappers calling Instance().Log() with the corresponding level in `graph_runtime/graph_runtime/src/public/logger.cc`

**Checkpoint**: `bazel build //src/public:runtime` passes ✅ — logger.cc compiled with LogLevelToString, singleton, timestamp, format assembly, mutex-protected output, and convenience wrappers

---

## Phase 3: User Story 1 - Default Console Logging (Priority: P1) 🎯 MVP

**Goal**: Logger works out-of-the-box — no hook setup needed. All internal modules can log via convenience methods.

**Independent Test**: Build and run any existing test that uses `std::cout` (e.g., scheduler_test.cc), then verify a new test that calls `Logger::Info(...)` produces expected output on stdout.

- [x] T013 [US1] Write a unit test at `graph_runtime/graph_runtime/src/tests/logger_test.cc` that calls `Logger::Info("graphrt::test", "hello")` and captures stdout to verify the formatted output matches `[graphrt::test] [INFO] ...`
- [x] T014 [US1] Write a unit test that calls `Logger::Error(...)` and verifies output goes to stderr
- [x] T015 [US1] Write a unit test that makes 10 concurrent `Logger::Info(...)` calls from 8 threads and verifies no interleaved/corrupted output lines

**Checkpoint**: `bazel test //src/tests:logger_test` passes ✅ — 6 tests covering LogLevelToString, all 5 levels, concurrent logging (8 threads × 50 iterations), and repeated calls (1000x)

---

## Phase 4: User Story 2 - Custom Hook Table (Priority: P1)

**Goal**: External consumers can inject a `GraphHookEntity[]` table via `GraphRuntime::SetGlobalHook()`. Logger dispatches to all `kHookTypeLogIntercept` hooks.

**Independent Test**: Register a hook table with two log intercept hooks, verify both receive the formatted string, and output falls back to stdout when hooks are cleared.

- [x] T016 [P] [US2] Implement `GraphRuntime::SetGlobalHook(const GraphHookEntity*)` — store pointer in a module-level `std::atomic<const GraphHookEntity*>` with release ordering in `graph_runtime/graph_runtime/src/public/graph_runtime.cc`
- [x] T017 [P] [US2] Implement `GraphRuntime::GetGlobalHook(int type) const` — scan atomically loaded table for first entry matching type, return pointer or nullptr in `graph_runtime/graph_runtime/src/public/graph_runtime.cc`
- [x] T018 [US2] Integrate hook dispatch into `Logger::Log()` — before writing to stdout/stderr, load the global hook table, iterate entries matching `kHookTypeLogIntercept`, call each `hook_fn(formatted_line, 0)`. If any returns `true`, skip default output in `graph_runtime/graph_runtime/src/public/logger.cc`
- [x] T019 [US2] Write a unit test that registers a hook table with one log intercept hook, logs a message, and verifies the hook receives the formatted string
- [x] T020 [US2] Write a unit test that clears the hook table (pass nullptr) after registration and verifies output falls back to stdout
- [x] T021 [US2] Write a unit test that registers a hook table with two log intercept hooks and verifies both are called in table order

**Checkpoint**: `bazel test //src/tests/...` passes ✅ — 13/13 tests. Hook table registration, dispatch, suppression, and cleanup all verified.

---

## Phase 5: User Story 3 - Hook-Controlled Suppression (Priority: P2)

**Goal**: Hook return value controls default output — `true`=suppress, `false`=allow. Edge cases: exception safety, NULL sentinel, concurrent hook swap.

**Independent Test**: Register a hook returning `true` and verify no stdout output; register a hook returning `false` and verify stdout output still occurs.

- [x] T022 [P] [US3] Implement return-value-based suppression in `Logger::Log()` — after iterating all matching hooks, if any hook returned `true`, skip default stdout/stderr write in `graph_runtime/graph_runtime/src/public/logger.cc`
- [x] T023 [US3] Implement exception safety — wrap each `hook_fn` call in try/catch, log warning to stderr on exception, fall back to default output in `graph_runtime/graph_runtime/src/public/logger.cc`
- [x] T024 [US3] Handle NULL-initialized/sentinel-only table (first entry type==0) — treat as "no hooks registered", write to stdout/stderr normally in `graph_runtime/graph_runtime/src/public/logger.cc`
- [x] T025 [US3] Write a unit test that registers a hook returning `true` and verifies no output appears on stdout
- [x] T026 [US3] Write a unit test that registers a hook returning `false` and verifies default output still appears on stdout
- [x] T027 [US3] Write a unit test where a hook throws an exception and verifies the exception is caught and output falls back to stdout
- [x] T028 [US3] Write a unit test that swaps the hook table atomically via SetGlobalHook while concurrent log calls are running (8 threads)

**Checkpoint**: `bazel test //src/tests/...` passes ✅ — 13/13. Exception safety and concurrent swap verified.

---

## Phase 6: Migration & Polish

**Purpose**: Replace raw `std::cout`/`std::cerr` in existing modules with the new logger.

- [ ] T029 [P] Migrate `graph_runtime/graph_runtime/src/scheduler/scheduler.cc` — replace `std::cout`/`std::cerr` with `Logger::Xxx("graphrt::scheduler", ...)`
- [ ] T030 [P] Migrate `graph_runtime/graph_runtime/src/examples/string_pipeline.cc` — replace `std::cout`/`std::cerr` with `Logger::Xxx("graphrt::example", ...)`
- [ ] T031 [P] Migrate `graph_runtime/graph_runtime/src/examples/string_pipeline_json.cc` — replace `std::cout`/`std::cerr` with `Logger::Xxx("graphrt::example", ...)`
- [ ] T032 [P] Migrate `graph_runtime/graph_runtime/src/examples/custom_parser.cc` — replace `std::cout`/`std::cerr` with `Logger::Xxx("graphrt::example", ...)`
- [ ] T033 [P] Migrate `graph_runtime/graph_runtime/examples/consumer_demo/main.cc` — replace `std::cout` with `Logger::Xxx("graphrt::demo", ...)`
- [ ] T034 Run `bazel test //...` to verify all existing tests still pass after migration
- [ ] T035 Run logger performance benchmark — measure 100K log messages with no hook registered, verify overhead within 10% of raw `std::cout` (SC-003)

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — can start immediately
- **Foundational (Phase 2)**: Depends on Setup completion — BLOCKS all user stories
- **US1 (Phase 3)**: Depends on Foundational — No dependencies on other stories
- **US2 (Phase 4)**: Depends on Foundational — Independently testable from US1 but integrates into same `Logger::Log()` method
- **US3 (Phase 5)**: Depends on US2 (suppression logic builds on hook dispatch, but can be implemented with a stub dispatch if US2 not ready)
- **Migration (Phase 6)**: Depends on Foundational (needs logger working)

### User Story Dependencies

- **User Story 1 (P1)**: Can start after Phase 2 — fully independent MVP
- **User Story 2 (P1)**: Can start after Phase 2 — independently testable, adds hook table to GraphRuntime
- **User Story 3 (P2)**: Can start after Phase 4 — builds on hook dispatch but minimal coupling

### Parallel Opportunities

| Phase | [P] tasks |
|-------|-----------|
| Phase 1 | T004 (umbrella header update can happen alongside other Setup tasks) |
| Phase 2 | T012 (convenience methods are independent wrappers) |
| Phase 4 | T016, T017 (SetGlobalHook and GetGlobalHook are independent implementations) |
| Phase 5 | T022 (suppression logic), T023 (exception safety) |
| Phase 6 | T029–T033 (each file migration is fully independent) |

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup — Bazel target, headers, structs
2. Complete Phase 2: Foundational — Logger singleton, format assembly, stdout/stderr output
3. Complete Phase 3: US1 — verify default console logging works
4. **STOP and VALIDATE**: Logger outputs formatted messages to stdout/stderr with no hooks needed

### Incremental Delivery

1. Phase 1 + Phase 2 → Foundation ready (logger builds)
2. Add Phase 3 (US1) → Default console output working — **MVP achieved**
3. Add Phase 4 (US2) → Hook table injection working
4. Add Phase 5 (US3) → Suppression control + edge cases
5. Add Phase 6 → All existing code migrated, performance verified

### Parallel Team Strategy

Once Phase 1 + Phase 2 complete:
- Developer A: Phase 3 (US1) + Phase 4 (US2) sequentially (shared `Logger::Log()` method)
- Developer B: Phase 6 migrations (fully independent file changes)
- Developer C: Phase 5 (US3) — can start after Phase 4 `hook_fn` dispatch is stubbed
