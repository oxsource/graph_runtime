# Data Model: Library Public API

## Entities

### Public Header Layer

The public API consists of a `src/public/include/graph_runtime/` directory with the following files:

| File | Content | Source |
|------|---------|--------|
| `graph_runtime_export.h` | `GRAPH_RUNTIME_API` macro | New |
| `types.h` | CollectionItemId, ErrorCallback, IsStopStatus, StatusStop | Moved from `src/public/types.h` |
| `timestamp.h` | Timestamp, TimestampDiff | Re-export of `src/stream/timestamp.h` |
| `packet.h` | Packet | Re-export of `src/stream/packet.h` |
| `graph_config.h` | GraphConfig, NodeDef, ExecutorDef | Re-export of `src/config/graph_config.h` |
| `side_packet.h` | PacketSet, OutputSidePacketSet | Re-export of `src/public/side_packet.h` |
| `graph_runtime.h` | Umbrella header including all above | New |

### strip_include_prefix Mechanism

```
src/public/
└── include/
    └── graph_runtime/
        ├── graph_runtime.h     ← #include "graph_runtime/graph_runtime.h"
        ├── graph_runtime_export.h
        ├── types.h
        ├── packet.h
        ├── timestamp.h
        ├── graph_config.h
        └── side_packet.h

BUILD.bazel:
  cc_library(
      name = "runtime",
      hdrs = glob(["include/graph_runtime/*.h"]),
      strip_include_prefix = "include",
      ...
  )
```

Consumer usage: `#include "graph_runtime/graph_runtime.h"` — the compiler sees the path as `include/graph_runtime/graph_runtime.h`, `strip_include_prefix = "include"` strips the `include/` prefix, leaving `graph_runtime/graph_runtime.h`.

### GRAPH_RUNTIME_API Macro

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
    #define GRAPH_RUNTIME_API  // empty — controlled by -fvisibility=hidden
  #endif
#endif
```

| Context | `GRAPH_RUNTIME_SHARED_LIBRARY` | `GRAPH_RUNTIME_API` expands to |
|---------|-------------------------------|-------------------------------|
| Building the lib (static) | Not defined | empty (but `-fvisibility=hidden` limits exports) |
| Building the shared lib | Defined | `__attribute__((visibility("default")))` |
| Consumer (header only) | Not defined | empty (no-op, uses default import) |

### Dep Reference Convention

All BUILD.bazel `deps` entries use `@graph_runtime//` prefix:

```python
# src/stream/BUILD.bazel
cc_library(
    name = "timestamp",
    ...
    deps = [
        "@graph_runtime//src/public:types",
        "@com_google_absl//absl/status:statusor",
    ],
)
```

### Linker Anchor

`graph_runtime_init.cc` ensures static registrations survive `linkshared=True` linking:

```cpp
// References static registry symbols to prevent linker stripping.
namespace { volatile int anchor = (DoSideEffect(), 0); }
```
