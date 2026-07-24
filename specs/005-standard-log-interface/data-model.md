# Data Model: Standard Log Interface

## LogLevel (Internal, `src/log/logger.h`)

Enumeration of log levels in syslog order: lower value = more severe.

| Value | Name | Default Routing | Output Abbreviation |
|-------|------|----------------|---------------------|
| 0 | `kFatal` | stderr | `F` |
| 1 | `kError` | stderr | `E` |
| 2 | `kWarn` | stdout | `W` |
| 3 | `kInfo` | stdout | `I` |
| 4 | `kDebug` | stdout | `D` |

**Validation**: Range [0, 4]; out-of-range clamped to `kError`.

## LogFormat

**Default output format**: `TAG L YYYY-MM-DD HH:MM:SS.mmm content\n`

Example: `graphrt::scheduler I 2026-07-24 14:30:00.123 Pipeline started`

## HookFactory (`src/hook/factory.h`)

Static class managing registered hooks. Storage is a `vector<{type, fn}>`.

| Method | Description |
|--------|-------------|
| `Register(int type, HookFn fn)` | Register a hook by type; replaces existing of same type |
| `ForEachAccept(int type, const void* data, int flags)` | Call the hook of given type, return its result |
| `ClearForTesting()` | Test only: clear all hooks |

## Logger (`src/log/logger.h`)

Central logging interface. Meyer singleton.

- Logs formatted via `std::snprintf` → `TAG L YYYY-MM-DD HH:MM:SS.mmm content`
- Before writing, calls `HookFactory::ForEachAccept(kTypeLog, line, 0)`
- If any hook returns `true`, default output suppressed
- Default output protected by `absl::Mutex` to prevent interleaving

## GraphRuntime API

```cpp
void SetHook(int type, HookFn fn);  // Register a hook by type
```

Delegates to `HookFactory::Register()` internally.
