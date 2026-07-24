# Implementation Plan: Standard Log Interface

**Branch**: `005-standard-log-interface` | **Date**: 2026-07-24 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/005-standard-log-interface/spec.md`

## Summary

Replace raw `std::cout`/`std::cerr` in the graph_runtime library with a standard logging interface. The logger defaults to stdout/stderr output with structured format `[TAG] [LEVEL] timestamp content`. A hook table (`GraphHook[]`, NULL-terminated sentinel array) is configured on the `GraphRuntime` instance via `SetGlobalHook`/`GetGlobalHook(int type)`. The Logger reads the table internally, dispatching to entries of type `kHookTypeLogIntercept`; each hook's boolean return controls whether default stdout is suppressed (`true`) or proceeds (`false`). Future modules can add new hook types (e.g., metrics, tracing) without API changes. Design follows syslog ordering (kFatal=0, kError=1, kWarn=2, kInfo=3, kDebug=4) with module-based TAG (`graphrt::` prefix) and ISO 8601 millisecond timestamps.

## Technical Context

**Language/Version**: C++17

**Primary Dependencies**: `abseil-cpp` (`absl::Mutex` for thread safety, `absl::Status` for error reporting)

**Storage**: N/A (no persistence)

**Testing**: Google Test (existing project framework)

**Target Platform**: macOS (dev), Linux (server) — cross-platform C++

**Project Type**: Library (C++ static/shared library via Bazel)

**Performance Goals**: Logging overhead within 10% of raw `std::cout` at 100K messages; zero-cost when no hook registered

**Constraints**: Thread-safe concurrent logging from 8+ threads; public API must use `GRAPH_RUNTIME_API` macro; headers under `strip_include_prefix = "include"`; deps use `@graph_runtime//` prefix

**Scale/Scope**: ~20 log call sites in scheduler/stream/node/config; external consumers via public umbrella header

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|-----------|--------|-------|
| I. Stream-Based Graph Architecture | ✅ PASS | Logging is orthogonal to dataflow; no graph coupling |
| II. Configuration Driven | ✅ PASS | Logger is not graph topology; no config coupling |
| III. Modularity & Extensibility | ✅ PASS | Hook table sentinel-array pattern aligns perfectly with replaceable interfaces |
| IV. Google C++ Code Style | ✅ PASS | Will follow naming conventions, 80-char lines, `GRAPH_RUNTIME_API` visibility |
| V. Build System Integrity | ✅ PASS | New log targets under `//src/public`; deps use `@graph_runtime//` prefix |

**Design Constraints Check**:
- Public headers under `src/public/include/graph_runtime/` → ✅ logger headers placed there
- Umbrella header `graph_runtime.h` includes all public types → ✅ logger types added
- `strip_include_prefix = "include"` → ✅ consumer writes `#include "graph_runtime/logger.h"`
- Bazel-only build → ✅ new `cc_library` in `//src/public/BUILD.bazel`
- Reference implementations (MediaPipe, Atlas) → ✅ sentinel array pattern aligns with EGL/Atlas C API conventions

**Result**: GATE PASSED — no constitutional violations.

## Project Structure

### Documentation (this feature)

```text
specs/005-standard-log-interface/
├── plan.md              # This file
├── spec.md              # Feature specification
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/           # Phase 1 output
│   ├── graph_runtime_hook_api.h  # HookType + GraphHook + SetGlobalHook/GetGlobalHook
│   └── logger.h         # Public Logger API contract
└── checklists/
    └── requirements.md
```

### Source Code (repository root `graph_runtime/`)

```text
src/
├── public/
│   ├── include/graph_runtime/
│   │   ├── graph_runtime.h          # Umbrella header (existing, add logger.h)
│   │   ├── graph_runtime.h          # GraphRuntime class (existing, add HookType, GraphHook, SetGlobalHook/GetGlobalHook)
│   │   ├── logger.h                 # Public Logger API (convenience methods only)
│   │   └── graph_runtime_export.h   # (existing) GRAPH_RUNTIME_API macro
│   ├── logger.h                     # Internal: Logger class + LogLevel enum + LogMessage struct
│   ├── logger.cc                    # Logger implementation (singleton, format assembly, hook dispatch)
│   ├── logger.h                     # Internal Logger header (singleton accessor)
│   └── BUILD.bazel                  # Updated with log_interface cc_library
├── scheduler/                       # Migrate std::cout → logger calls
├── stream/                          # Migrate std::cout → logger calls
├── node/                            # Migrate std::cout → logger calls
├── config/                          # Migrate std::cout → logger calls
├── examples/                        # Migrate std::cout → logger calls
└── tests/
    ├── logger_test.cc               # New: logger unit tests
    └── public_api_test.cc           # Updated: verify logger in umbrella
```

**Structure Decision**: Single-project layout inside `graph_runtime/graph_runtime/src/`. Logger lives under `//src/public` as a new `cc_library` target, consistent with existing module layout. Public headers go in `include/graph_runtime/` with `strip_include_prefix`. Internal modules depend on it via `@graph_runtime//src/public:log_interface`.

## Complexity Tracking

No constitutional violations to justify.
