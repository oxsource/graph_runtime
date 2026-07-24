# Research: Standard Log Interface

## Design Decisions

### Decision 1: HookFactory with Type-Based Registration (replaces Sentinel Table)

- **Decision**: Use `HookFactory` (static class with `Register(type, fn)` / `ForEachAccept(type, data, flags)`) instead of a NULL-terminated sentinel array.
- **Rationale**: The sentinel table was generic but Logger was the only consumer. Simpler design with one hook per type reduces complexity, removes atomic iteration, and eliminates the `GraphHookEntity` struct.
- **Alternatives considered**: Sentinel array (removed — too generic for single consumer), C function pointers (retained).

### Decision 2: GRAPHRT_LOGD Macro API (replaces public Logger class)

- **Decision**: Public API is macros `GRAPHRT_LOGD`/`LOGI`/`LOGW`/`LOGE`/`LOGF`. The `Logger` class is fully internal.
- **Rationale**: Cleaner separation — external consumers use macros; internal modules use Logger directly.
- **Alternatives considered**: Exposing `Logger::Info()` publicly (removed — violates encapsulation).

### Decision 3: GRAPHRT_LOG_TAG Macro for Module-Level TAG

- **Decision**: Modules define `#define GRAPHRT_LOG_TAG "graphrt::scheduler"` before including logger.h, then call no-tag overloads.
- **Rationale**: Eliminates repetitive tag passing, keeps TAG declaration at module top.
- **Alternatives considered**: Per-call TAG parameter (verbose), class-level TAG (not thread-safe).

### Decision 4: Logger in src/framework/utils/ (internal module)

- **Decision**: Logger lives in `src/framework/utils/` (not `src/framework/public/`). Public headers only contain macros.
- **Rationale**: Aligns with constitutional principle V — only `//src/framework/public:runtime` exposed externally.
- **Alternatives considered**: Logger in `src/framework/public/` (removed — leaked internal API).

### Decision 5: GraphRuntime::SetHook for Public Registration

- **Decision**: External consumers call `runtime.SetHook(kTypeLog, myFn)`. No public `RegisterHook` function.
- **Rationale**: Runtime is the sole public entry point for configuration. HookFactory is an internal implementation detail.
- **Alternatives considered**: Public `RegisterHook()` free function (removed), `GRAPH_REGISTER_HOOK` macro (removed), sentinel table (removed).

## Technology Choices

| Choice | Selected | Rationale |
|--------|----------|-----------|
| Hook management | `HookFactory` static class + `vector<{type, fn}>` | Simple, thread-safe, one-per-type |
| Public log API | `GRAPHRT_LOGD`/`GRAPHRT_LOGI` macros | Encapsulates Logger, no class exposure |
| Module TAG | `#define GRAPHRT_LOG_TAG ...` before include | Declared once per module |
| Log level output | Single char (F/E/W/I/D) | Compact, readable |
| Timestamp | ISO 8601 with ms | Standard, sufficient precision |
| Thread safety | `absl::Mutex` on output; `std::atomic_flag` for hook init | Project already depends on absl |
