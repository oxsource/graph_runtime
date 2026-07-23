# Contract: Public API

**Public headers**: `src/public/include/graph_runtime/`

**Umbrella header**: `graph_runtime.h`

```cpp
#include "graph_runtime/graph_runtime_export.h"
#include "graph_runtime/graph.h"
#include "graph_runtime/packet.h"
#include "graph_runtime/node.h"
#include "graph_runtime/types.h"
```

## Export Macro

```cpp
// graph_runtime_export.h
#if defined(_WIN32)
  #if defined(GRAPH_RUNTIME_SHARED_LIBRARY)
    #define GRAPH_RUNTIME_API __declspec(dllexport)
  #else
    #define GRAPH_RUNTIME_API __declspec(dllimport)
  #endif
#else
  #if defined(GRAPH_RUNTIME_SHARED_LIBRARY)
    #define GRAPH_RUNTIME_API __attribute__((visibility("default")))
  #else
    #define GRAPH_RUNTIME_API
  #endif
#endif
```

## Named Types

```cpp
namespace graph::runtime {

// Error code enum
enum class GRAPH_RUNTIME_API ErrorCode {
  kOk = 0,
  kFileNotFound,
  kParseError,
  kGraphError,
  kRuntimeError,
};

// Type-erased data packet
class GRAPH_RUNTIME_API Packet { ... };

// Graph facade
class GRAPH_RUNTIME_API Graph { ... };

// Node interface (can be subclassed externally)
class GRAPH_RUNTIME_API Node { ... };

}  // namespace graph::runtime
```
