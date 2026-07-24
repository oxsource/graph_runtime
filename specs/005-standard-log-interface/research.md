# Research: Standard Log Interface

## Design Decisions

### Decision 1: Hook Table as NULL-Terminated Sentinel Array

- **Decision**: Use a NULL-terminated sentinel array of `{hook_type, hook_fn}` entries for the external interception interface.
- **Rationale**: EGL and Vulkan extension patterns use this approach — it provides ABI stability, zero wasted memory (no reserved slots), and dynamic extensibility by appending before the sentinel. No fixed slot limit.
- **Alternatives considered**: Fixed-slot struct (wasteful, ABI break on extension), std::function vector (not C-compatible, ABI unstable across shared library boundary), dynamic reallocation (complex, introduces allocation in hot path).
- **Source**: EGL_EXT_device_query, Vulkan VkBaseOutStructure pNext chains, GLib GSourceFuncs sentinel pattern.

### Decision 2: C Function Pointers for Public API (not std::function)

- **Decision**: Use plain C function pointers in the hook table struct for the cross-library public API boundary.
- **Rationale**: `std::function` is not ABI-stable across shared library boundaries. C function pointers guarantee binary compatibility. Internal implementation may still use `std::function` where appropriate.
- **Alternatives considered**: `std::function` (ABI unstable), virtual base class (requires RTTI/exception handling, more overhead), raw function pointers (selected).
- **Source**: Google C++ Style Guide permits C function pointers at ABI boundaries. Existing `ErrorCallback` in the project uses `std::function` for internal use only.

### Decision 3: Runtime-Level Global Hook Table Registry (not Logger-owned)

- **Decision**: The hook table is configured on the `GraphRuntime` instance (`SetGlobalHook`/`GetGlobalHook(int type)`), stored internally in a static atomic so Logger and other modules can read it by hook type without a runtime reference.
- **Rationale**: Decouples hook table ownership from any single module. Future features (metrics, tracing, security hooks) add new `HookType` values without modifying Logger or creating per-module setters. Atomic pointer provides lock-free read on all hot paths.
- **Alternatives considered**: `Logger::SetHookTable` (couples hook ownership to Logger), per-module setter functions (proliferates API surface, each needs own atomic).
- **Source**: C++11 guarantees thread-safe static initialization. Google style guide permits atomic globals for configuration state.

### Decision 4: Logger as Meyer Singleton with Output Mutex

- **Decision**: Thread-safe Meyer singleton (C++11 function-local `static`) for Logger instance; `absl::Mutex` guards default stdout/stderr output to prevent interleaved writes.
- **Rationale**: Logger is stateless aside from the output mutex — all hook dispatch state lives in the global hook table. Meyer singleton is zero-overhead until first use.
- **Alternatives considered**: Pass-around Logger instance (adds parameter to every function), dependency injection (over-engineered for v1).
- **Source**: C++11 guarantees thread-safe static initialization.

### Decision 5: Log Module Under `//src/public` with Bazel Visibility

- **Decision**: `HookType`, `GraphHookEntity`, `SetGlobalHook`, and `GetGlobalHook` live directly on `GraphRuntime` in `src/public/graph_runtime.h` (no separate hook_table header). Logger public header is under `src/public/include/graph_runtime/logger.h`. Implementation in `src/public/logger.cc`. New Bazel target `//src/public:log_interface` with `visibility = ["//visibility:public"]`. Aggregated into `//src/public:runtime`.
- **Rationale**: Follows established project pattern. Internal modules (scheduler, stream, etc.) depend on `@graph_runtime//src/public:log_interface`. External consumers get it via the umbrella `graph_runtime.h`.
- **Alternatives considered**: Separate `//src/log` module (adds unnecessary module boundary), header-only logger (cannot hide implementation details).
- **Source**: Existing `//src/public/BUILD.bazel` patterns, project_bootstrap.md conventions.

## Technology Choices

| Choice | Selected | Rationale |
|--------|----------|-----------|
| Log level enum (internal) | `enum class LogLevel { kFatal=0, kError=1, kWarn=2, kInfo=3, kDebug=4 }` | Syslog convention; not exposed — convenience methods select level implicitly |
| TAG format | `const char*` string `graphrt::module_name` | Lightweight, no allocation, simple comparison |
| Timestamp format | ISO 8601 with milliseconds | Human-readable, millisecond precision sufficient for debugging |
| Thread safety | `absl::Mutex` + `std::atomic` | Project already depends on absl; no new dependency needed |
| Log message struct (internal) | `struct LogMessage { LogLevel level; const char* tag; const char* content; int64_t timestamp_ms; }` | Internal struct for format assembly; hooks receive pre-formatted `const char*` string |
| Hook table entry | `struct GraphHookEntity { int type; bool (*hook_fn)(const void* data, int flag); }` | `type` discriminates at lookup time; `flag` reserved (pass 0); `data` cast by hook based on registered type |
