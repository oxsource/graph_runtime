# Feature Specification: Standard Log Interface

**Feature Branch**: `005-standard-log-interface`

**Created**: 2026-07-24

**Status**: Implemented

**Input**: User description: "目前日志输出应该是标准控制台输出，设计标准日志接口，默认标准输出，外部可注入消费回调，返回true则终止默认行为，否则继续标准输出"

## Clarifications

### Session 2026-07-24

- **Q1: Function pointer table structure → A**: NULL-terminated sentinel array → later simplified to HookFactory with type-based registration, one hook per type.
- **Q2: TAG naming convention → A**: Module-based `graphrt::` prefix, defined via `GRAPHRT_LOG_TAG` macro.
- **Q3: Timestamp precision → A**: Millisecond precision, ISO 8601 format.

## User Scenarios & Testing

### User Story 1 - Default Console Logging (Priority: P1)

As a developer using the graph_runtime library, I want the library to output log messages to stdout by default, without any additional setup.

**Why this priority**: Zero-config backward compatibility.

**Independent Test**: Build and run any pipeline; log messages appear on stdout with no hooks registered.

**Acceptance Scenarios**:
1. **Given** default configuration with no hooks, **When** a log message is produced, **Then** it's printed as `TAG L YYYY-MM-DD HH:MM:SS.mmm content`
2. **Given** default configuration, **When** an error is logged, **Then** it's printed to stderr

### User Story 2 - Hook Injection (Priority: P1)

As a library consumer, I want to inject a log hook via `GraphRuntime::SetHook()`, so that I can intercept log output.

**Why this priority**: Core extensibility mechanism.

**Independent Test**: Register a hook and verify it receives the formatted log line.

**Acceptance Scenarios**:
1. **Given** `runtime.SetHook(kTypeLog, myFn)` called, **When** a log is produced, **Then** `myFn` is invoked with the formatted line
2. **Given** a hook registered and later replaced, **When** a log is produced, **Then** only the new hook receives it

### User Story 3 - Suppression Control (Priority: P2)

As a developer, I want my hook's return value to control whether default stdout occurs.

**Why this priority**: Core behavioral contract.

**Independent Test**: Hook returning `true` → no stdout; returning `false` → stdout proceeds.

**Acceptance Scenarios**:
1. **Given** a hook returning `true`, **When** a log is produced, **Then** no stdout output
2. **Given** a hook returning `false`, **When** a log is produced, **Then** stdout output occurs
3. **Given** a hook conditionally returning `true`/`false`, **When** different messages are logged, **Then** suppression matches the return value

### Edge Cases

- Hook throwing an exception → caught, warning to stderr, fallback to default output
- No hook registered → all output goes to stdout/stderr normally
- Concurrent logging → thread-safe, no interleaved output
- Only one hook per type allowed; re-registering replaces the previous

## Requirements

### Functional Requirements

- **FR-001**: Default stdout/stderr output with no setup required
- **FR-002**: Public API via `GraphRuntime::SetHook(int type, HookFn fn)` for registering hooks by type
- **FR-003**: Hook function signature `bool (*)(const void* data, int flags)`; for log intercept, `data` is the formatted string
- **FR-004**: Hook return `true` suppresses default output, `false` allows it
- **FR-005**: Five log levels (kFatal..kDebug, syslog order) selected via convenience methods (Info, Debug, etc.)
- **FR-006**: `GRAPHRT_LOG_TAG` macro for module-level default TAG
- **FR-007**: `GRAPHRT_LOGD/LOGI/LOGW/LOGE/LOGF` macros for public API
- **FR-008**: Thread-safe, mutex-protected default output
- **FR-009**: Hook exception safety — catch, warn, fallback
- **FR-010**: Performance within 10% of raw std::cout with no hooks
- **FR-011**: TAG uses `graphrt::<module_name>` convention
- **FR-012**: Timestamp ISO 8601 with millisecond precision
- **FR-013**: Logger implementation in `src/framework/utils/` (internal); `src/framework/utils/hook.h` contains HookFactory

### Key Entities

- **LogLevel** (internal, `src/framework/utils/logger.h`): kFatal=0, kError=1, kWarn=2, kInfo=3, kDebug=4
- **LogMessage** (removed): Formatting is done inline; hooks receive `const char*`
- **HookFactory** (`src/framework/utils/hook.h`): Static class managing `vector<{type, fn}>`. `Register()`, `ForEachAccept()`
- **Logger** (internal, `src/framework/utils/`): Singleton, formats log lines, dispatches to HookFactory::ForEachAccept
- **HookFn** (`include/graph_runtime/hook.h`): `bool (*)(const void* data, int flags)`
- **kTypeLog** (`include/graph_runtime/hook.h`): Hook type constant for log interception (value 1)

## Success Criteria

- **SC-001**: `runtime.SetHook(kTypeLog, fn)` works with zero additional config
- **SC-002**: Hook return value correctly controls suppression (100% accuracy)
- **SC-003**: Logging overhead within 10% of `std::cout` at 100K messages
- **SC-004**: 8-thread concurrent stress test passes for 60s
- **SC-005**: Existing code migrated from `std::cout` to Logger without behavioral changes

## Assumptions

- Standard log levels sufficient; custom levels out of scope for v1
- Single globally-accessible logger instance (Meyer singleton)
- TAG defaults to `"graphrt"` when `GRAPHRT_LOG_TAG` not defined
- One hook per type; re-registration replaces
