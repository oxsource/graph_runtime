# Research: Library Public API Design

## Phase 0 — Unknowns Resolution

### 1. Dep Reference Convention: `//` vs `@graph_runtime//`

**Decision**: Use `@graph_runtime//` prefix in all BUILD.bazel `deps` entries.

**Rationale**:
- Bazel resolves `//foo:bar` to `@graph_runtime//foo:bar` when building from within the `graph_runtime` workspace — functionally identical.
- Using `@graph_runtime//` makes the repository reference explicit, which is helpful when BUILD files are inspected, copied, or used as examples.
- External consumers write `deps = ["@graph_runtime//src/public:runtime"]` — internal deps should follow the same pattern for consistency.
- No functional difference in Bazel 6.5.x — both forms produce identical build graphs.

**Alternatives considered**:
- Keep `//` prefix: Shorter, conventional within single-repo projects. Rejected because Graph Runtime is designed as an external dependency, not a monorepo sub-project.
- Mixed convention (`//` for internal, `@graph_runtime//` for public targets): Adds mental overhead distinguishing cases. Rejected for consistency.

### 2. Public Header Selection

**Decision**: Expose only types that external pipeline developers directly interact with. Keep internals (Scheduler, Stream/Node managers, InputStreamHandler, etc.) hidden.

**Public**:
- `graph_runtime_export.h` — Export macro
- `types.h` — CollectionItemId, ErrorCallback, IsStopStatus, StatusStop
- `timestamp.h` — Timestamp, TimestampDiff
- `packet.h` — Packet
- `graph_config.h` — GraphConfig, NodeDef, ExecutorDef
- `side_packet.h` — PacketSet, OutputSidePacketSet
- `graph_runtime.h` — Umbrella header
- `graph_runtime_init.cc` — Linker anchor

**Internal (no public exposure)**:
- `input_stream.h`, `input_stream_manager.h`, `output_stream.h`, `output_stream_shard.h/.cc`, `output_stream_manager.h/.cc`, `output_stream_handler.h/.cc`
- `node.h`, `node_contract.h`, `node_factory.h`, `node_registry.h`, `node_options.h`, `options_registry.h`
- `graph_context.h`
- `scheduler.h`, `scheduler_queue.h`, `executor.h`, `thread_pool_executor.h`, `input_stream_handler.h`, `counters.h`
- `config_validator.h`, `parser_registry.h`, `i_graph_config_parser.h`
- `graph_builder.h`, `graph_runtime.cc` (implementation)

### 3. Re-export Header Pattern

**Decision**: Public headers are thin re-exports that include the internal header via `src/...` path and re-export the class. Public classes get the `GRAPH_RUNTIME_API` decoration.

**Pattern** (`packet.h`):
```cpp
#pragma once
#include "graph_runtime/graph_runtime_export.h"
#include "src/stream/packet.h"  // internal full definition

// Packet is already complete in the internal header.
// The public header re-exports it with GRAPH_RUNTIME_API context.
```

For headers that need `GRAPH_RUNTIME_API` on the class:
```cpp
#pragma once
#include "graph_runtime/graph_runtime_export.h"
#include "src/config/graph_config.h"  // internal definition

// Re-export with export macro context.
// The internal class definition is picked up via the include chain.
```

**Rationale**:
- Keeps a single source of truth for each class definition (no duplication).
- Public headers add only the `GRAPH_RUNTIME_API` decoration context.
- Internal includes are resolved at build time via Bazel's `-I` paths.
- Consumers see only `#include "graph_runtime/..."` paths.

### 4. `graph_runtime_init.cc` Linker Anchor

**Decision**: Create a single translation unit that references static registration symbols to prevent the linker from stripping them in `linkshared=True` builds.

**Content**:
```cpp
#include "src/node/node_registry.h"    // NodeFactoryRegistry symbols
#include "src/config/parser_registry.h" // ParserRegistry symbols

namespace graph::runtime {
namespace {

// References to force static registration symbols into the output.
volatile int kAnchorNode = (NodeFactoryRegistry::RegisteredTypes(), 0);
volatile int kAnchorParser = (ParserRegistry::RegisteredTypes(), 0);

}  // namespace
}  // namespace graph::runtime
```

**Rationale**: Without this, `cc_binary(linkshared=True, linkstatic=True)` strips unreferenced symbols from static archives, which removes static registration calls made at file scope.

### 5. External Consumption Flow

```
Consumer's WORKSPACE:
  http_archive(name = "graph_runtime", ...)
  load("@graph_runtime//:graph_runtime_deps.bzl", "graph_runtime_setup")
  graph_runtime_setup()

Consumer's BUILD:
  cc_binary(
      name = "my_app",
      srcs = ["main.cc"],
      deps = ["@graph_runtime//src/public:runtime"],
  )
```

## Technology Choices Summary

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Dep prefix | `@graph_runtime//` | Explicit repo reference, consistent with external consumption |
| Public header location | `src/public/include/graph_runtime/` | Atlas pattern, strip_include_prefix |
| Re-export pattern | Thin include-only wrappers | Single source of truth for class definitions |
| Export macro | `GRAPH_RUNTIME_API` in `graph_runtime_export.h` | Atlas ATLAS_API pattern |
| Visibility | `-fvisibility=hidden` + GRAPH_RUNTIME_API | Only public symbols exported |
| Linker anchor | `graph_runtime_init.cc` | Prevents static registration stripping |
