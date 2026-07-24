# Feature Specification: Standard Log Interface

**Feature Branch**: `005-standard-log-interface`

**Created**: 2026-07-24

**Status**: Draft

**Input**: User description: "目前日志输出应该是标准控制台输出，设计标准日志接口，默认标准输出，外部可注入消费回调，返回true则终止默认行为，否则继续标准输出"

## Clarifications

### Session 2026-07-24

- **Q1: Function pointer table structure** → A: NULL-terminated sentinel array (hook type + function pointer pairs, terminated by a NULL entry), inspired by EGL extension patterns. No fixed slot limit, dynamically extensible by appending before the sentinel.
- **Q2: TAG naming convention** → A: Module-based with `graphrt::` prefix (e.g., `graphrt::scheduler`, `graphrt::stream`, `graphrt::node`, `graphrt::config`, `graphrt::public`).
- **Q3: Timestamp precision** → A: Millisecond precision, ISO 8601 format (e.g., `2026-07-24 12:00:00.123`).

## User Scenarios & Testing

### User Story 1 - Default Console Logging (Priority: P1)

As a developer using the graph_runtime library, I want the library to output log messages to stdout by default, so that I can see operational information without any additional setup.

**Why this priority**: The default behavior must work out-of-the-box to maintain backward compatibility and zero-config experience for existing users.

**Independent Test**: Can be fully tested by running any graph pipeline and verifying that log messages appear on stdout without registering any custom callback.

**Acceptance Scenarios**:

1. **Given** a default library configuration with no custom logger hook table registered, **When** the library produces a log message, **Then** the message is printed to stdout with the format `[TAG] [SEVERITY] timestamp content`
2. **Given** a default library configuration, **When** the library encounters an error, **Then** the error message is printed to stderr

---

### User Story 2 - Custom Callback Consumption (Priority: P1)

As a library consumer integrating graph_runtime into my application, I want to inject a custom log hook via a struct pointer table, so that I can route log output to my own logging system (e.g., file, network, structured logging).

**Why this priority**: The ability to intercept and redirect logs is the core new capability requested by the feature description.

**Independent Test**: Can be fully tested by registering a hook table and verifying that log messages are delivered to the registered hook instead of (or in addition to) stdout.

**Acceptance Scenarios**:

1. **Given** a custom logger hook table has been registered, **When** a log message is produced, **Then** the registered hook is invoked with the structured log message (TAG, timestamp, level, content)
2. **Given** a hook table registered via the public API and later replaced with a NULL/empty table, **When** a subsequent log message is produced, **Then** the hook is no longer invoked and output falls back to default stdout behavior

---

### User Story 3 - Hook-Controlled Default Output Suppression (Priority: P2)

As a library consumer, I want my hook to control whether the default stdout output occurs, so that I can suppress redundant console output when my hook has already handled the message.

**Why this priority**: This is explicitly requested in the feature description and is the key behavioral contract of the hook design.

**Independent Test**: Can be tested by registering a hook that returns true for certain messages and false for others, then verifying stdout suppression behavior matches the return value.

**Acceptance Scenarios**:

1. **Given** a registered hook that returns `true`, **When** a log message is produced, **Then** the message is delivered to the hook but **not** printed to stdout
2. **Given** a registered hook that returns `false`, **When** a log message is produced, **Then** the message is delivered to the hook **and** also printed to stdout as usual
3. **Given** a registered hook that conditionally returns `true` or `false` based on log level, **When** messages of different levels are produced, **Then** stdout output occurs only for messages where the hook returned `false`

---

### Edge Cases

- What happens when a registered hook function throws an exception? The logger should catch the exception, log a warning, and fall back to default stdout behavior for that message to avoid crashing the host application.
- How does the system behave with a NULL-initialized hook table (no entries or first entry already sentinel)? It is treated as "no hook registered" — all output goes to stdout/stderr by default.
- How are concurrent log calls handled from multiple threads? Log delivery to hooks and stdout must be thread-safe and not produce interleaved or corrupted output.
- What happens if the hook table is modified while a log message is being processed? The hook table pointer/registration should use thread-safe mechanisms (e.g., atomic pointer swap) to prevent dangling references.
- How does the system behave when the sentinel terminator is missing (malformed table)? The logger should detect absence of NULL sentinel within a reasonable limit and fall back to default output, logging a warning.

## Requirements

### Functional Requirements

- **FR-001**: The system MUST provide a standard logging interface with default behavior that outputs log messages to stdout and error messages to stderr
- **FR-002**: The system MUST provide a hook table on the `GraphRuntime` instance (`SetGlobalHook`/`GetGlobalHook(int type)`) via a struct pointer table (NULL-terminated sentinel array of `GraphHook` entries), externally injectable at runtime
- **FR-003**: The hook function signature MUST be `bool hook_fn(const void* data, int flag)`, where `flag` is an extensibility parameter (default 0, reserved for future use). `data` is cast based on the hook's registered `type`. For `kHookTypeLogIntercept`, `data` points to a null-terminated `const char*` containing the fully formatted log line. The function returns `true` to suppress default stdout output or `false` to allow it.
- **FR-004**: The system MUST respect the hook's return value: `true` suppresses default stdout output for that message, `false` allows default stdout output to proceed
- **FR-005**: The system MUST support five log levels (internally `kFatal`..`kDebug` in syslog order: 0=most severe, 4=most verbose) with appropriate default routing (Error/Fatal to stderr, others to stdout). External consumers select level via named convenience methods (Debug, Info, Warn, Error, Fatal) without exposing the enum directly
- **FR-006**: The logging interface MUST be thread-safe, allowing concurrent log calls from multiple threads without data races or interleaved output
- **FR-007**: The system MUST handle hook exceptions gracefully by catching them, logging a warning, and falling back to default stdout for the affected message
- **FR-008**: The logging API MUST be accessible from all internal modules (scheduler, stream, node, graph, config) as well as the public API surface
- **FR-009**: The system MUST support registration of new hook types by appending entries before the NULL sentinel, enabling future extensibility without ABI changes
- **FR-010**: The system MUST maintain reasonable performance overhead when no hook is registered (close to direct `std::cout` cost)
- **FR-011**: Every log message MUST include a TAG field using the `graphrt::<module_name>` convention to identify the source module
- **FR-012**: The timestamp format MUST use ISO 8601 with millisecond precision (e.g., `2026-07-24 12:00:00.123`)

### Key Entities

- **LogLevel** (internal): Enumeration of log levels (kFatal=0, kError=1, kWarn=2, kInfo=3, kDebug=4) following syslog convention where lower = more severe. Used internally; external consumers use convenience methods (Debug, Info, Warn, Error, Fatal)
- **LogMessage** (internal): A structured log entry containing TAG, timestamp (ISO 8601 ms), log level, and content text. Used internally by the Logger to assemble the formatted string delivered to hooks. Not exposed in the public hook API — hooks receive the pre-formatted `const char*` line.
- **GraphHook**: A struct with `type` and `hook_fn` fields, forming a NULL-terminated sentinel array (`GraphHook[]`). Configured on the `GraphRuntime` instance via `SetGlobalHook`/`GetGlobalHook(int type)`. The initial table contains one log interception hook (type `kHookTypeLogIntercept`); future hook types can be appended before the sentinel without ABI changes
- **Logger**: The central logging interface that reads hook entries of type `kHookTypeLogIntercept` from the global hook table registry, dispatches log messages to them, and handles default stdout/stderr output based on each hook's return value

## Success Criteria

### Measurable Outcomes

- **SC-001**: Users can inject a custom logger hook table via a single public API call and observe log messages delivered to the registered hook with zero additional configuration
- **SC-002**: The hook's boolean return value correctly controls stdout suppression for every log message (100% accuracy in test scenarios)
- **SC-003**: Logging overhead with no callback registered is within 10% of the performance of direct `std::cout` calls, measured over 100,000 log messages
- **SC-004**: Concurrent logging from 8 threads produces no output corruption, no data races, and no crashes in a sustained 60-second stress test
- **SC-005**: Existing scheduler and example code can be migrated from raw `std::cout`/`std::cerr` to the new logging interface with no behavioral changes observable to end users

## Assumptions

- Standard log levels (kFatal, kError, kWarn, kInfo, kDebug in syslog order) are sufficient; custom levels are out of scope for v1
- Timestamp uses ISO 8601 with millisecond precision as clarified
- TAG uses `graphrt::<module_name>` format as clarified
- The hook table uses a NULL-terminated sentinel array pattern as clarified
- A single globally-accessible logger instance (singleton pattern) is acceptable; per-instance loggers can be considered in a future iteration
- Exceptions thrown by hook functions should never propagate to library internal code
- Source file and line number information is optional in the initial interface and can be added later
- The existing abseil dependency is suitable for use in the logger implementation (e.g., `absl::Mutex` for thread safety)
