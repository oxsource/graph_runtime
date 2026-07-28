# Feature Specification: Profiler Framework

**Feature Branch**: `010-profiler-framework`

**Created**: 2026-07-28

**Status**: Draft

**Input**: User description: "当前项目参考对比mediapipe实现，可以考虑添加设计类似的profile机制，用于后续统计测试性能"

## User Scenarios & Testing

### User Story 1 - Configure and Enable Profiling (Priority: P1)

A library consumer enables profiling for a graph run to measure node-level performance. They configure the profiler either via the graph config file (any format supported by `IGraphConfigParser`) or programmatically through `SetProfilerConfig` before initialization. After graph execution, they query per-node timing data to identify bottlenecks.

**Why this priority**: Core use case that delivers immediate value — library consumers need the ability to measure node execution times to understand graph performance characteristics.

**Independent Test**: Can be fully tested by:
1. Writing a small graph with known delays (e.g., a node that sleeps for known durations in Open/Process/Close)
2. Enabling the profiler via config
3. Executing the graph
4. Querying `GetNodeProfiles()` and verifying the measured runtimes match expected delays within reasonable tolerance

**Acceptance Scenarios**:

1. **Given** a graph with a single node that sleeps 10ms in Process, **When** the graph runs with profiling enabled and `GetNodeProfiles()` is called, **Then** the returned profile for that node shows a Process runtime between 8ms and 12ms (accounting for scheduling overhead).

2. **Given** a multi-node graph with profiling enabled, **When** the graph completes execution, **Then** `GetNodeProfiles()` returns one entry per node, each containing Open, Process, and Close timing data.

3. **Given** a graph where profiling is disabled (default), **When** the graph completes execution, **Then** `GetNodeProfiles()` returns an empty vector.

---

### User Story 2 - Persist Profile Results for Offline Analysis (Priority: P1)

A performance engineer runs a graph with profiling enabled, then saves the per-node profile data to disk for later analysis. The saved data is written in a readable, portable format (JSON) that can be inspected manually, compared across runs, and fed into analysis tools.

**Why this priority**: Without persistence, profile data only exists in memory during the process lifetime, making it useless for CI pipelines, regression tracking, and offline analysis — which are the primary motivations for adding profiling.

**Independent Test**: Can be fully tested by:
1. Running a graph with profiling enabled
2. Calling `WriteProfile("/tmp/profile.json")`
3. Reading the file back and verifying it contains valid JSON with the same data as `GetNodeProfiles()`

**Acceptance Scenarios**:

1. **Given** a completed graph run with profiling enabled, **When** `WriteProfile("/tmp/profile.json")` is called, **Then** a JSON file is created with one entry per node, each containing `node_name`, `open_runtime_usec`, `close_runtime_usec`, `process_count`, `process_time_total_usec`, `process_time_mean_usec`, and the process runtime histogram buckets.

2. **Given** a profiler that was never started, **When** `WriteProfile("/tmp/profile.json")` is called, **Then** the file is created with an empty `nodes` array.

3. **Given** an invalid path for `WriteProfile`, **Then** the method returns an error status without crashing.

---

### User Story 3 - Analyze Saved Profiles via CLI Tool (Priority: P2)

A developer collects profile files from multiple graph runs (e.g., as part of a CI benchmark suite) and runs a command-line report tool to produce a human-readable summary showing per-node statistics: mean/median/p99 process times, open/close durations, and histogram distributions. The tool supports filtering by node name and comparing two runs to detect regressions.

**Why this priority**: The CLI tool unlocks the value of persistence — without it, users must manually parse JSON files. Lower priority than the core profiler and persistence because the Reporter can be built incrementally.

**Independent Test**: Can be fully tested by:
1. Creating a known profile JSON file manually
2. Running the CLI tool against it
3. Verifying the printed report matches expected values

**Acceptance Scenarios**:

1. **Given** a profile JSON file from a previous graph run, **When** the CLI tool processes it with default options, **Then** a formatted table is printed with columns for node name, process count, mean time, total time, open time, and close time.

2. **Given** a profile JSON file, **When** the CLI tool is run with `--node-filter "source"`, **Then** only nodes matching the filter pattern appear in the output.

3. **Given** two profile JSON files from different runs, **When** the CLI tool is run with a compare flag, **Then** the output shows per-node delta columns (e.g., "+5.2%" or "-3.1ms") between the two runs.

---

### User Story 4 - Build-Time Profiler Disabling for Production Deployment (Priority: P3)

A production deployer wants to eliminate all profiling overhead from a release build. They use a build-time flag to compile out the profiler entirely, ensuring zero runtime overhead and zero code footprint.

**Why this priority**: Important for production environments where every microsecond counts, but lower priority than the basic functional use case since the profiler already has a runtime enable/disable toggle.

**Independent Test**: Can be tested by:
1. Building the same test twice: once with the profiler enabled and once with it disabled
2. Verifying that the disabled build has no profiler symbols
3. Verifying that the disabled build runs with identical behavior but the profiler API returns empty/no-op results

**Acceptance Scenarios**:

1. **Given** the library is built with `graph_runtime_profiler=false` (or default), **When** a caller invokes `runtime.profiler()`, **Then** the returned pointer is non-null but all profiling methods are no-ops, and `GetNodeProfiles()` returns an empty list.

2. **Given** two builds of the same graph — one with profiling enabled and one disabled, **When** comparing binary sizes, **Then** the disabled build is measurably smaller.

---

### User Story 5 - Clock Abstraction for Testability (Priority: P4)

A test engineer wants to write deterministic unit tests for the profiler itself. They inject a mock clock that returns fixed timestamps, allowing precise verification of histogram ranges without relying on real wall-clock time.

**Why this priority**: Important for profiler correctness but only needed by developers of the profiler itself, not by library consumers.

**Independent Test**: Can be tested by:
1. Creating a `MockClock` that returns predetermined timestamps
2. Passing it to `ProfilingContext::SetClock()`
3. Executing a known sequence of Scope entries and exits
4. Verifying the resulting NodeProfile values match the expected calculations

**Acceptance Scenarios**:

1. **Given** a `MockClock` that returns [100, 200, 300] microseconds on successive calls, **When** a Scope of type `PROCESS` is created (reading 100us) and destroyed (reading 200us), then another of type `OPEN` (reading 200us to 300us), **Then** the process histogram contains one sample of 100us duration and the open runtime is 100us.

---

### Edge Cases

- What happens when profiling is enabled but no nodes exist in the graph? `GetNodeProfiles()` returns an empty vector.
- How does the system handle nodes with extremely fast Open/Process/Close (sub-microsecond)? The profiler records a runtime of 0 or 1 microsecond depending on clock resolution; this is acceptable behavior.
- What happens if `GetNodeProfiles()` is called while the graph is still running? The profiler returns partial data collected so far; no crash or undefined behavior.
- How does a reset behave mid-graph? After `Reset()`, all accumulated histogram data is cleared, but profiling continues accumulating new data.
- What happens if the clock returns non-monotonic values? The `RealClock` uses steady clock which is monotonic, so this cannot occur in practice.

## Requirements

### Functional Requirements

- **FR-001**: The system MUST provide a `ProfilerConfig` struct that allows enabling/disabling profiling and configuring histogram parameters (interval size, number of intervals), accessible both from graph config (any supported format via `IGraphConfigParser`) and programmatically via `SetProfilerConfig()` on `GraphRuntime`.
- **FR-002**: The system MUST provide an abstract `Clock` interface for time measurement, with a real-time implementation using a monotonic clock source, and allow injection of custom clocks for testing.
- **FR-003**: The system MUST provide a `TimeHistogram` that collects runtime samples into fixed-width buckets, tracks total count and total duration, and supports `Initialize(interval_size, num_intervals)`, `AddSample(start_usec, end_usec)`, `Reset()`, and read-only queries (count, total, mean, buckets).
- **FR-004**: The system MUST provide a `GraphProfiler` class with a RAII `Scope` helper that records start time on construction and computes duration on destruction, dispatching timing data per node to the appropriate Open/Process/Close histogram based on the event type.
- **FR-005**: The system MUST support a build-time switch that selects between the real profiler implementation and a no-op stub, with zero runtime overhead when the stub is selected.
- **FR-006**: The system MUST provide `ProfilingContext` as a consistent class name that resolves to either `GraphProfiler` (real) or `GraphProfilerStub` (no-op) depending on the build-time switch, enabling uniform usage in framework code without conditional compilation at each call site.
- **FR-007**: The `GraphRuntime` MUST expose:
  - `ProfilingContext* profiler()` for direct profiler handle access
  - `void SetProfilerConfig(const ProfilerConfig& config)` for programmatic configuration
  - `std::vector<NodeProfile> GetNodeProfiles() const` for querying results
- **FR-008**: The system MUST instrument `Node::Open`, `Node::Process`, and `Node::Close` calls in both `Scheduler::Schedule()` (sync path) and `SchedulerQueue::RunNode()` (async path) with `ProfilingContext::Scope` RAII wrappers such that timing is automatically recorded when profiling is enabled.
- **FR-009**: The system MUST integrate `ProfilerConfig` into `GraphConfig` as an optional field, leveraging the existing `IGraphConfigParser` abstraction — any config format parser (JSON, future YAML/Protobuf) can populate the `profiler_config` field by filling the same struct.
- **FR-010**: The system MUST support writing accumulated profile data to a JSON file via `GraphRuntime::WriteProfile(const std::string& path)` or `ProfilingContext::WriteProfile(const std::string& path)`, producing a human-readable JSON document containing all `NodeProfile` entries with histogram data.
- **FR-011**: The system MUST provide a `Reporter` class that can read one or more profile JSON files and compute aggregate statistics per node, including mean, min, max, count, and total time for Process, plus per-node Open and Close times.
- **FR-012**: The system MUST provide a command-line tool (e.g., `print_profile`) that uses the `Reporter` to produce formatted text reports from profile JSON files, with support for:
  - Reading one or more input files
  - Filtering output by node name (glob pattern)
  - Comparing two profile files with delta columns
  - Outputting in both human-readable table and CSV formats
- **FR-013**: The system MUST support a `trace_log_path` field in `ProfilerConfig` that specifies a default output directory for profile JSON files. When set, `WriteProfile()` without a path argument writes to `<trace_log_path>/<graph_name>.json`. When empty, `WriteProfile()` requires an explicit path argument. Periodic auto-write may be supported in a future phase.

### Key Entities

- **ProfilerConfig**: Configuration structure controlling profiling behavior — `enable_profiler` (bool), `histogram_interval_size_usec` (int64, default 1,000,000 = 1s), `num_histogram_intervals` (int, default 5), `trace_log_path` (string, default empty — if non-empty, `WriteProfile()` writes to `trace_log_path/<graph_name>.json` instead of requiring an explicit path argument).
- **TimeHistogram**: Bucket-based accumulator for runtime samples — stores configurable-width intervals, total count, total sum. Buckets are pre-allocated on `Initialize()`.
- **Clock**: Abstract interface for time measurement — provides monotonic microsecond timestamps. Supports both real-time and mock implementations.
- **NodeProfile**: Per-node profile result — contains `node_name`, `open_runtime_usec`, `close_runtime_usec`, and `process_runtime` (TimeHistogram).
- **GraphProfiler / GraphProfilerStub**: The real and no-op profiler implementations. The real version holds per-node `NodeProfile` data in a thread-safe map, manages the `Scope` RAII wrapper, and supports `Initialize`, `Start`, `Stop`, `Pause`, `Resume`, `Reset`, and `GetNodeProfiles`.
- **ProfilingContext**: Class alias that inherits from either `GraphProfiler` or `GraphProfilerStub` depending on the build-time switch. Owned by `GraphRuntime` and passed to `Scheduler`/`SchedulerQueue` for instrumentation.
- **Scope**: RAII helper nested inside `ProfilingContext` — records `start_time_usec` on construction, dispatches to `SetOpenRuntime`, `AddProcessSample`, or `SetCloseRuntime` on destruction based on `EventType` (OPEN, PROCESS, CLOSE).
- **Profile Json File**: On-disk representation of profile data — a JSON document with top-level keys: `graph_name` (string), `capture_time` (ISO 8601 string), `node_count` (int), and `nodes` (array of `NodeProfile` entries with histogram buckets). This file is the exchange format between the profiler (producer) and the Reporter (consumer).
- **Reporter**: Offline analysis component that ingests one or more profile JSON files, computes per-node statistics (mean, min, max, count, p50, p95, p99), and produces formatted reports. Supports filtering, comparison (delta between two runs), and multiple output formats.
- **CLI Tool (print_profile)**: Command-line executable wrapping the `Reporter`, accepting flags for input files, node filtering, comparison mode, and output format. Designed for use in CI pipelines and ad-hoc performance investigations.

## Success Criteria

### Measurable Outcomes

- **SC-001**: A library consumer can enable profiling via JSON config or programmatic API, run a graph, and obtain per-node Open/Process/Close timing data with microsecond precision in under 5 lines of consumer code.
- **SC-002**: When profiling is disabled (either at build time or runtime), the performance overhead is zero — no clock reads, no histogram updates, no memory allocation for profile data.
- **SC-003**: The profiler's `Scope` RAII wrapper adds less than 100 nanoseconds of overhead per call when profiling is enabled at runtime (measured as the difference between executing a node with and without the Scope wrapper, averaged over 1 million iterations).
- **SC-004**: A `TimeHistogram` with 5 buckets of 1-second intervals can accumulate 1 million samples in under 100 milliseconds.
- **SC-005**: The core profiling API can be used uniformly throughout the framework code without requiring conditional compilation directives at each call site.
- **SC-006**: A user can run a graph with profiling enabled, call `WriteProfile()` once, and a valid JSON file is produced that can be read back by `Reporter` to produce a formatted report — all without writing any glue code.
- **SC-007**: The CLI tool can process a profile JSON file containing 1000 nodes and produce a formatted summary report in under 1 second on a standard developer machine.

## Assumptions

- The profiler uses microsecond precision, which is sufficient for the expected use case of measuring node execution times in a graph runtime.
- A monotonic real-time clock is available on all target platforms.
- The build system's conditional compilation mechanism is sufficient for the build-time enable/disable switch.
- The profiler is single-graph-scoped: one `ProfilingContext` instance per `GraphRuntime`, not shared across graphs.
- Thread safety is provided via a mutex for histogram updates and atomic flags for enable/disable checks; the fine-grained lock-free approach (MediaPipe's `CircularBuffer`) is deferred to Phase 2.
- Node names in the graph are unique, so they can serve as keys for per-node profile data.
- The existing `IGraphConfigParser` abstraction sufficiently decouples config format from runtime; adding `profiler_config` support in any new format parser (YAML, Protobuf, etc.) is just a matter of populating the same `GraphConfig::profiler_config` struct fields.
- The existing JSON parser (`JsonParser`) serves as the initial implementation; future format parsers follow the same pattern.
- JSON is an acceptable serialization format for profile data — no protobuf or binary format is required for Phase 1.
- The `nlohmann/json` library already in the project can be reused for profile serialization (the profiler module may depend on the JSON parser, or implement its own lightweight JSON writer to avoid creating a dependency on an internal-only module).
