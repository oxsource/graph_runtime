# Research: Profiler Framework Technical Decisions

**Date**: 2026-07-28

## Clock Abstraction

- **Decision**: Abstract `Clock` interface with `RealClock` wrapping `std::chrono::steady_clock`
- **Rationale**: MediaPipe uses a `Clock`/`MonotonicClock` abstraction for testability. `steady_clock` is monotonic and not affected by system time adjustments. `absl::Time` is already used in the codebase (Logger), so `absl::FromUnixMicros`/`ToUnixMicros` in the boundary is natural.
- **Alternatives considered**:
  - Direct `steady_clock::now()` everywhere — simpler but non-testable
  - `absl::Time` directly — already in tree, but we want microsecond int64 for performance in hot Scope path

## Build-Time Enable/Disable Switch

- **Decision**: `config_setting` with `define_values = {"graph_runtime_profiler": "true"}` + `graph_runtime_select()`
- **Rationale**: MediaPipe uses the same pattern (`MEDIAPIPE_PROFILING=1`). The project already has `graph_runtime_select()` for platform selects. Using a `GRAPH_RUNTIME_PROFILER_ENABLED` define in the real target and omitting it in the stub gives clean conditionals.
- **Alternatives considered**:
  - Runtime `if` check only — simpler build but nonzero overhead even when disabled
  - Template policy-based — zero overhead but increases compile time and code complexity
  - Always compile real profiler — eliminates build complexity but adds code size even when unused

## TimeHistogram Implementation

- **Decision**: Fixed-size vector of buckets, pre-allocated on `Initialize()`, protected by `std::mutex`
- **Rationale**: Simple and sufficient for Phase 1. The histogram is per-node and updated only during Process calls; contention is low.
- **Alternatives considered**:
  - Lock-free atomic histogram — MediaPipe's approach with `CircularBuffer`, but over-engineered for Phase 1
  - Streaming percentile estimation (TDigest) — useful for skewed distributions but adds complexity without clear need

## Thread Safety Model

- **Decision**: `std::mutex` on histogram/bucket mutations, `std::atomic<bool>` for `is_profiling_` flag (read on every Scope entry/exit)
- **Rationale**: The atomic flag check is the hot path (every Open/Process/Close). Histogram updates are relatively rare (one per invocation) and mutex contention is negligible at Phase 1 scale.
- **Alternatives considered**:
  - Lock-free `CircularBuffer` — MediaPipe's approach, deferred to Phase 2 trace events
  - Per-node mutexes — finer-grained but adds complexity; single profiler mutex is fine for Phase 1

## ProfilerConfig Integration

- **Decision**: Optional `profiler_config` block in root of JSON config, mapped to `GraphConfig::profiler_config`
- **Rationale**: MediaPipe places `ProfilerConfig` in `CalculatorGraphConfig`. Our existing pattern has config structs with direct parsing. Adding a struct field is minimal and consistent.
- **Alternatives considered**:
  - Separate config file — over-engineered for bootstrapping
  - Programmatic API only — JSON users cannot enable profiling without code changes

## Public API Shape

- **Decision**: `GraphRuntime::profiler()` returns `ProfilingContext*` (dedicated handle) + convenience `GetNodeProfiles()` + `SetProfilerConfig()`
- **Rationale**: MediaPipe exposes `CalculatorGraph::profiler()` for direct access. The convenience method reduces boilerplate for common use. `SetProfilerConfig` allows programmatic config without JSON.
- **Alternatives considered**:
  - Only `GetNodeProfiles()` on `GraphRuntime` — simpler but less flexible for future tracing features
  - Static global profiler — MediaPipe considered but rejected for multi-graph safety; we follow the same reasoning

## Profile Serialization Format

- **Decision**: JSON, written by a lightweight hand-written JSON writer in `profile_writer.cc`
- **Rationale**: JSON is human-readable, portable, and requires no protobuf dependency. The existing `nlohmann/json` is available but lives in the `config/json` module; creating a dependency from profiler to config would violate module isolation. A lightweight writer that only handles the specific schema needed is ~100 lines and avoids coupling.
- **Alternatives considered**:
  - Protobuf / FlatBuffers — MediaPipe uses protobuf (`GraphProfile` proto), but we have no protobuf in the project; adding it just for profiling is overkill
  - CSV — simple but cannot represent nested histogram bucket data cleanly
  - Binary blob — not human-readable and requires a decoder for every analysis tool
  - Reuse `nlohmann/json` — already in the project but in a separate module; coupling profiler to the JSON parser module breaks isolation

## Reporter Design

- **Decision**: Simple in-memory aggregation (`Reporter` class), separate CLI tool (`print_profile`)
- **Rationale**: MediaPipe has a `Reporter` class in `mediapipe/framework/profiler/reporter/` with similar scope. Keeping the library (`Reporter`) separate from the CLI entry point (`print_profile`) allows programmatic use and testing.
- **Alternatives considered**:
  - Python post-processing script — simpler but adds a language dependency; C++ tool keeps everything in the same build system
  - SQLite-backed analysis — over-engineered for Phase 1
  - Embedded HTTP server — MediaPipe's web profiling direction but far beyond Phase 1 scope

## Comparison / Regression Detection

- **Decision**: Simple per-node delta calculation (absolute + percentage) between two reports
- **Rationale**: MediaPipe's `print_profile` does not support comparison. The delta feature adds significant value for CI regression detection without much complexity.
- **Alternatives considered**: Statistical tests (Mann-Whitney, t-test) — too heavy for Phase 1; requires multiple samples. Simple delta is useful enough.

## Reference: MediaPipe Profiler Architecture

Key components mapped from MediaPipe:

| MediaPipe | graph_runtime (Phase 1) | Notes |
|-----------|------------------------|-------|
| `ProfilingContext` / `GraphProfiler` | `ProfilingContext` / `GraphProfiler` | Same naming, simplified internals |
| `ProfilingContext::Scope` | `ProfilingContext::Scope` | Same RAII pattern |
| `GraphTracer` + `CircularBuffer<TraceEvent>` | Deferred to Phase 2 | Not needed for basic timing |
| `TimeHistogram` (proto) | `TimeHistogram` (C++ class) | Same concept, no protobuf |
| `CalculatorProfile` (proto) | `NodeProfile` (C++ struct) | Same fields, no protobuf |
| `ProfilerConfig` (proto) | `ProfilerConfig` (C++ struct) | Same fields, JSON-parsed |
| `Clock` / `MonotonicClock` | `Clock` / `RealClock` | Same abstraction |
| `MEDIAPIPE_PROFILING` define | `graph_runtime_profiler` Bazel config | Same concept |
| `GraphProfilerStub` | `GraphProfilerStub` | Same no-op pattern |
| `WriteProfile()` (proto disk serialization) | `WriteProfile()` (JSON disk serialization) | Same purpose, different format |
| `Reporter` + `print_profile` | `Reporter` + `print_profile` | Same name, similar scope; adds comparison feature |
