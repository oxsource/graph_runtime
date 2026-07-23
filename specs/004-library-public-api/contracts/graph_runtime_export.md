# Contract: graph_runtime_export.h

**File**: `graph_runtime/src/public/include/graph_runtime/graph_runtime_export.h`

```cpp
#if !defined(GRAPH_RUNTIME_EXPORT_H_)
#define GRAPH_RUNTIME_EXPORT_H_

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

#endif  // GRAPH_RUNTIME_EXPORT_H_
```

**Semantics**:
- Included by every public header.
- `GRAPH_RUNTIME_SHARED_LIBRARY` is defined via `copts` in `src/public/BUILD.bazel` when building the `runtime_shared` target.
- For static library builds (`cc_library`), `GRAPH_RUNTIME_SHARED_LIBRARY` is NOT defined, so `GRAPH_RUNTIME_API` expands to empty. Visibility is controlled by `-fvisibility=hidden`.
- Public classes: `class GRAPH_RUNTIME_API GraphRuntime { ... };`
- Public functions: `GRAPH_RUNTIME_API void SomeFunction();`
