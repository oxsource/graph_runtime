# Quickstart: Graph Runtime

## Prerequisites

- Bazel 6.5.x (install via [bazelisk](https://github.com/bazelbuild/bazelisk))
- C++17 compatible compiler (clang 14+ / gcc 11+)
- macOS ARM64 or Linux x86_64

All commands must be run from the `graph_runtime/` directory (Bazel workspace root).

## Build

```bash
# Build the library
bazel build //src/public:runtime

# Build the shared library
bazel build //src/public:runtime_shared

# Build all targets
bazel build //src/...
```

## Test

```bash
# Run all tests
bazel test //src/tests/...

# Run specific test
bazel test //src/tests:graph_builder_test --test_output=all
```

## Run Example

```bash
# String Pipeline MVP
bazel run //src/examples:string_pipeline
```

## External Dependency

In your external project's `WORKSPACE`:

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

In your `BUILD`:

```python
cc_binary(
    name = "my_pipeline",
    srcs = ["my_pipeline.cc"],
    deps = ["@graph_runtime//src/public:runtime"],
)
```

In your C++ code:

```cpp
#include "graph_runtime/graph_runtime.h"

using namespace graph::runtime;

// Parse config
auto parser = std::make_unique<JsonParser>();
GraphConfig config = parser->Parse("pipeline.json");

// Build and run graph
Graph graph;
graph.Initialize(config);
graph.Start();
graph.WaitUntilDone();
```

## Platform Selection

```bash
# Default host platform
bazel build //src/public:runtime

# Explicit platform
bazel build //src/public:runtime --config=linux_x86_64
bazel build //src/public:runtime --config=macos_arm64
```
