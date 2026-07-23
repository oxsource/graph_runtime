# Research: Project Architecture Design

## Phase 0 — Unknowns Resolution

### 1. Stream-Based Scheduling Model (MediaPipe Reference)

**Decision**: Adopt MediaPipe's scheduler abstraction with simplification for Phase 1.

**Rationale**:
- MediaPipe's scheduler uses a layer-based topological order with input-ready signaling.
- Phase 1 only requires a single-threaded, non-preemptive scheduler that processes Nodes when all input Streams have data available.
- MediaPipe's `CalculatorGraph` uses `Packet`/`Stream`/`Node` as core primitives — the same model specified in project_bootstrap.md.

**Alternatives considered**:
- Full MediaPipe compatibility layer — rejected (over-engineered for Phase 1 goals).
- Actor model (Akka-style) — rejected (adds unnecessary concurrency complexity).

### 2. Public API Export Pattern (Atlas Reference)

**Decision**: Mirror Atlas `ATLAS_API` macro model exactly.

**Rationale**:
- Atlas's `atlas_export.h` provides a proven pattern: `-fvisibility=hidden` + `__attribute__((visibility("default")))` via macro.
- `cc_binary(linkshared=True, linkstatic=True)` with `alwayslink=1` matches Atlas's `src/public/BUILD`.

**Details**:
- `GRAPH_RUNTIME_API` macro in `graph_runtime_export.h`
- `-DGRAPH_RUNTIME_SHARED_LIBRARY` defined during shared lib build
- Umbrella header `graph_runtime.h` includes all sub-headers

### 3. Bazel Dependency Management (Atlas Reference)

**Decision**: Use `graph_runtime_deps.bzl` with `_deps()` internal function + `graph_runtime_setup()` external macro.

**Rationale**:
- Atlas's `atlas_deps.bzl` separates internal dep fetching (`_atlas_deps()`) from external setup (`atlas_setup()`).
- `native.existing_rule()` guard prevents duplicate declarations across workspace reloads.
- Platform definitions via `config_setting_and_platform()` macro keep BUILD files DRY.

### 4. Packet / Stream Design

**Decision**: Minimal Packet class wrapping `std::any` + timestamp; Stream as a thread-safe bounded queue.

**Rationale**:
- `std::any` provides type erasure without template bloat; adequate for Phase 1 in-process data flow.
- Bounded queue with back-pressure signal prevents unbounded memory growth.
- Timestamp enables future synchronization policies (sync/sync-set).

**Alternatives considered**:
- Template-based `Packet<T>` — rejected (complicates heterogeneous streams).
- Lock-free SPSC queue — deferred to Phase 2 if latency becomes critical.

### 5. JSON Config Parser

**Decision**: Use nlohmann/json as the sole JSON library, wrapped behind `IGraphConfigParser`.

**Rationale**:
- nlohmann/json is header-only, well-maintained, and already used in Atlas.
- The interface abstraction insulates Runtime from the parsing library.

### 6. Testing Strategy

**Decision**:
- **Unit tests**: GoogleTest, one `cc_test` per internal module, depending on the specific module.
- **Integration tests**: One `cc_test` depending on `//src/public:runtime`, testing end-to-end pipeline.

**Rationale**:
- Matches Atlas test structure (`tests/api/`, `tests/core/`, `tests/public/`).
- GoogleTest is the de facto standard for C++ Bazel projects.

## Technology Choices Summary

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Scheduler model | Layer-based topological, single-threaded | Matches Phase 1 simplicity goal |
| Public API export | `GRAPH_RUNTIME_API` macro | Atlas-proven pattern |
| Dep management | `graph_runtime_deps.bzl` | Atlas-proven pattern |
| Packet type erasure | `std::any` | Simple, sufficient for Phase 1 |
| Stream transport | Bounded queue + timestamp | Prevents memory blowup |
| JSON library | nlohmann/json | Header-only, widely used |
| Test framework | GoogleTest | Bazel-native, industry standard |
| Build system | Bazel 6.5.x | Specified in project_bootstrap.md |
| C++ standard | C++17 | Required for `std::any`, `std::variant` |
