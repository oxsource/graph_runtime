# Data Model: Standard Log Interface

## LogLevel (Internal)

Enumeration of log levels following syslog convention: lower value = more severe, higher value = more verbose. Not exposed in the public API — external consumers use named convenience methods (Info, Debug, etc.).

| Value | Name | Default Routing | Description |
|-------|------|----------------|-------------|
| 0 | `kFatal` | stderr | `F` | Severe errors that will likely lead to application termination |
| 1 | `kError` | stderr | `E` | Error events that might still allow the system to continue |
| 2 | `kWarn` | stdout | `W` | Potentially harmful situations that do not prevent operation |
| 3 | `kInfo` | stdout | `I` | General operational information about normal system behavior |
| 4 | `kDebug` | stdout | `D` | Detailed debugging information (typically filtered out in production) |

**Validation rules**: Must be in range [0, 4]. Values outside range are clamped to `kError` and a warning is emitted.

## LogMessage (Internal)

The structured log entry used internally by Logger to assemble the formatted string. Not exposed in the public API — hooks receive the pre-formatted `const char*` line.

| Field | Type | Description |
|-------|------|-------------|
| `level` | `LogLevel` | Log level (see above) |
| `tag` | `const char*` | Source module identifier, format `graphrt::module_name` (e.g., `graphrt::scheduler`) |
| `message` | `const char*` | Log message content text |
| `timestamp_ms` | `int64_t` | Epoch milliseconds (UTC) |

**Default output format**: `[TAG] [L] YYYY-MM-DD HH:MM:SS.mmm content\n`

- Example: `[graphrt::scheduler] [I] 2026-07-24 14:30:00.123 Pipeline started\n`
- Error/Fatal level routes to stderr instead of stdout.

## GraphHookEntity

A single entry in the NULL-terminated sentinel hook table.

| Field | Type | Description |
|-------|------|-------------|
| `type` | `int` | Hook type identifier. `0` = sentinel terminator. Type `1` = log interception hook. Future types > 1 reserved for extension. |
| `hook_fn` | `bool (*)(const void* data, int flag)` | Function pointer. `flag` is reserved (pass 0). `data` points to type-specific payload. For `kHookTypeLogIntercept`, `data` is a `const char*` (pre-formatted log line). Return `true` to suppress default behavior. May be NULL for placeholder entries. |

**Sentinel**: An entry with `type == 0` terminates the array. The table is considered empty if the first entry is already the sentinel.

**Iteration**: Walk entries from index 0. For each entry, if `type == 0`, stop. If `type` matches the desired hook type and `hook_fn != nullptr`, invoke `hook_fn(data, /*flag=*/0)` where `data` points to the type-specific payload. Continue to next entry regardless of return value (multiple hooks of the same type may process the same event).

## Logger (Singleton)

Central logging interface. Does not own the hook table — reads it from the `GraphRuntime`-level hook table (set via `SetGlobalHook`), filtering entries by `kHookTypeLogIntercept`.

| State | Type | Description |
|-------|------|-------------|
| `output_mutex_` | `absl::Mutex` | Guards default stdout/stderr output to prevent interleaved writes. |

**Lifecycle**:
- Initialized on first access (C++11 thread-safe function-local static).
- No explicit destruction — intentional leak to avoid static destruction order issues.

## GraphRuntime Hook Table

The hook table is owned and configured on the `GraphRuntime` instance. The table pointer is stored in a module-level static atomic (not a per-instance member) so that internal modules like Logger can read it without a runtime reference.

| State | Type | Description |
|-------|------|-------------|
| `hook_table_` | `std::atomic<const GraphHookEntity*>` | Active hook table pointer. Set via `GraphRuntime::SetGlobalHook()`, read via `GraphRuntime::GetGlobalHook(int type)`. |

**Lifecycle**:
- Initialized to `nullptr` (no hooks, pure default behavior).
- Set by the external consumer on the runtime instance: `runtime.SetGlobalHook(myTable)`.
- Cleared by `runtime.SetGlobalHook(nullptr)` or setting a sentinel-only table.
- Hook table lifetime managed by caller (caller registers, caller ensures table outlives all consumers).
- Thread-safe: atomic store-release / load-acquire for lock-free read on hot path.

## State Transitions

```
NULL (no hooks, pure defaults)
  ↓ runtime.SetGlobalHook(table)
Active (hooks registered)
  ↓ runtime.SetGlobalHook(NULL or sentinel-only table)
Inactive → defaults for all modules
  ↓ runtime.SetGlobalHook(table)
Active
```

Hook table swap is atomic and safe to call concurrently with any module's dispatch logic.
