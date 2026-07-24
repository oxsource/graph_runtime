# Quickstart: Standard Log Interface

## Default Usage (no hooks)

```cpp
#define GRAPHRT_LOG_TAG "graphrt::myapp"
#include "graph_runtime/graph_runtime.h"

int main() {
  GRAPHRT_LOGI("Pipeline started");
  GRAPHRT_LOGE("Error: something failed");
  GRAPHRT_LOGD("debug info");
}
```

**Default output**: `graphrt::myapp I 2026-07-24 14:30:00.123 Pipeline started`

## Internal Module Usage

Internal modules can use `Logger::Info(...)` directly:

```cpp
#define GRAPHRT_LOG_TAG "graphrt::scheduler"
#include "src/framework/utils/logger.h"

Logger::Info("Pipeline started");
Logger::Error("Error: something failed");
```

## Custom Hook (intercept logs)

```cpp
#include "graph_runtime/graph_runtime.h"

bool MyHook(const void* data, int /*flags*/) {
  fprintf(my_file, "%s\n", static_cast<const char*>(data));
  return false;  // let stdout output proceed
}

int main() {
  graph::runtime::GraphRuntime runtime;
  runtime.SetHook(graph::runtime::hook::kTypeLog, MyHook);

  GRAPHRT_LOGI("This goes to both my_file and stdout");
}
```

## Bazel Dependency

```python
deps = [
    "@graph_runtime//src/public:runtime",
]
```

The umbrella header `graph_runtime.h` includes all public types and macros.

## Migration from std::cout

| Before | After |
|--------|-------|
| `std::cout << "open " << name;` | `Logger::Info("open ...");` or `GRAPHRT_LOGI("open ...");` |
| `std::cerr << "error: " << msg;` | `Logger::Error("error: ...");` or `GRAPHRT_LOGE("error: ...");` |
| `std::cout << "debug: " << x;` | `Logger::Debug("debug: ...");` or `GRAPHRT_LOGD("debug: ...");` |

## Thread Safety

Logger is fully thread-safe. Multiple threads may log concurrently without interleaving or data races.
