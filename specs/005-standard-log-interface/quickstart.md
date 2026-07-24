# Quickstart: Standard Log Interface

## Default Usage (no hooks)

The logger works out of the box — no setup required. All internal modules automatically log via the default stdout/stderr output.

```cpp
#include "graph_runtime/graph_runtime.h"

// In any internal module:
graph::runtime::Logger::Info("graphrt::scheduler", "Pipeline started");
graph::runtime::Logger::Error("graphrt::stream", "Packet dropped: timeout");

// Convenience:
Logger::Debug("graphrt::node", "Processing frame #42");
Logger::Warn("graphrt::config", "Using default config path");
Logger::Fatal("graphrt::runtime", "Unrecoverable error");
```

**Default output**: `[graphrt::scheduler] [I] 2026-07-24 14:30:00.123 Pipeline started`

## Custom Hook (intercept logs)

```cpp
#include "graph_runtime/graph_runtime.h"

// Define a hook function. The type is known from registration context —
// only data and flag are passed at runtime. flag is reserved, always 0.
// For kHookTypeLogIntercept, data points to a pre-formatted string.
bool MyLogHook(const void* data, int flag) {
  const char* line = static_cast<const char*>(data);
  fprintf(my_log_file, "%s\n", line);
  return false;  // also keep stdout output
}

bool SilentHook(const void* data, int flag) {
  const char* line = static_cast<const char*>(data);
  SendAlert("log", line);
  return true;  // suppress stdout entirely
}

// Register hooks via sentinel-terminated array on the runtime instance
static const graph::runtime::GraphHookEntity kMyHooks[] = {
  { graph::runtime::kHookTypeLogIntercept, MyLogHook },
  { graph::runtime::kHookTypeLogIntercept, SilentHook },
  { graph::runtime::kHookTypeSentinel, nullptr },
};

graph::runtime::GraphRuntime runtime;
runtime.SetGlobalHook(kMyHooks);

// To disable hooks and restore defaults:
runtime.SetGlobalHook(nullptr);

// Query a hook by type (returns first match or nullptr):
const graph::runtime::GraphHookEntity* hook = runtime.GetGlobalHook(
    graph::runtime::kHookTypeLogIntercept);
```

## Bazel Dependency

Add to your BUILD file:

```python
deps = [
    "@graph_runtime//src/public:runtime",  # includes logger in umbrella
]
```

Or for internal modules only needing logging:

```python
deps = [
    "@graph_runtime//src/public:log_interface",
]
```

The umbrella header `graph_runtime.h` automatically includes all logger types.

## Migration from std::cout

| Before | After |
|--------|-------|
| `std::cout << "Open: " << name << std::endl;` | `Logger::Info("graphrt::scheduler", "Open: ...");` |
| `std::cerr << "Error: " << msg << std::endl;` | `Logger::Error("graphrt::scheduler", msg);` |
| `std::cout << "Process: " << ... << std::endl;` | `Logger::Debug("graphrt::stream", "Process: ...");` |

## Thread Safety

The logger is fully thread-safe. Multiple threads may call `Log()` concurrently without data races or interleaved output.
