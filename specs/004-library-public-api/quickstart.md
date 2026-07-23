# Quickstart: Library Public API — Consuming Graph Runtime as a Bazel Library

## External Project Setup

### WORKSPACE

```python
http_archive(
    name = "graph_runtime",
    url = "https://github.com/<org>/graph_runtime/archive/v1.0.0.tar.gz",
    sha256 = "<sha256>",
    strip_prefix = "graph_runtime-1.0.0",
)

load("@graph_runtime//:graph_runtime_deps.bzl", "graph_runtime_setup")
graph_runtime_setup()
```

### BUILD

```python
cc_binary(
    name = "my_pipeline",
    srcs = ["main.cc"],
    deps = ["@graph_runtime//src/public:runtime"],
)
```

### C++ Code

```cpp
#include "graph_runtime/graph_runtime.h"

using namespace graph::runtime;

int main() {
  // Parse config
  GraphConfig config;
  config.nodes.push_back({"source", "SourceNode", {}, {"output:data"}, {}, {}, {}, "", "", 1, 0});

  // Build and run
  auto runtime = GraphBuilder::Build(config);
  runtime->Start();
  runtime->WaitUntilDone();
  return 0;
}
```

## Non-Bazel Consumption

```bash
# Build the shared library
bazel build //src/public:runtime_shared

# Link against it
clang++ -I$(bazel info workspace)/src/public/include \
    -L$(bazel info bazel-bin)/src/public \
    -lruntime_shared my_app.cc
```

## Public API Headers

| Header | Contents | Include Path |
|--------|----------|-------------|
| `graph_runtime_export.h` | `GRAPH_RUNTIME_API` macro | `graph_runtime/graph_runtime_export.h` |
| `types.h` | Error types, stop status | `graph_runtime/types.h` |
| `timestamp.h` | Timestamp, TimestampDiff | `graph_runtime/timestamp.h` |
| `packet.h` | Packet (MakePacket, Get, Share) | `graph_runtime/packet.h` |
| `graph_config.h` | GraphConfig, NodeDef, ExecutorDef | `graph_runtime/graph_config.h` |
| `side_packet.h` | PacketSet, OutputSidePacketSet | `graph_runtime/side_packet.h` |
| `graph_runtime.h` | Umbrella header (includes all above) | `graph_runtime/graph_runtime.h` |

## Dep Convention

All internal BUILD files use `@graph_runtime//` prefix:

```python
deps = [
    "@graph_runtime//src/stream:timestamp",
    "@com_google_absl//absl/status:statusor",
]
```

## Verifying the Public API

```bash
# Build everything
bazel build //src/...

# Run public API tests
bazel test //src/tests:public_api_test

# Verify exported symbols (macOS)
nm -gU bazel-bin/src/public/libruntime_shared.dylib
```
