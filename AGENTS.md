<!-- SPECKIT START -->
For additional context about technologies to be used, project structure,
shell commands, and other important information, read the current plan at
`specs/010-profiler-framework/plan.md`, the module interaction design at
`specs/002-scheduler-stream-packet-design/plan.md`, the config architecture at
`specs/003-config-architecture/plan.md`, the library public API at
`specs/004-library-public-api/plan.md`, the project bootstrap at
`graph_runtime/docs/project_bootstrap.md`, and the feature specification at
`specs/010-profiler-framework/spec.md`.

Key architecture references:
- MediaPipe: /Users/moks/Develop/docker/ubuntu24/codes/mediapipe (stream scheduler, Calculator, Packet, GraphProfiler)
- Atlas: /Users/moks/Develop/docker/ubuntu24/codes/atlas (Bazel build, public API export, platform config)

Dep prefix convention: ALL BUILD.bazel `deps` must use `@graph_runtime//` prefix (not `//`).
Framework modules live under `src/framework/` — all internal code is in `src/framework/`.
Public headers: `graph_runtime/src/framework/public/include/graph_runtime/` with `strip_include_prefix = "include"`.
Consumer include path: `#include "graph_runtime/graph_runtime.h"` (umbrella header).
Shared library: `bazel build //src/framework/public:runtime_shared` → `libruntime_shared.dylib`.
Consumer demo: `cd graph_runtime/examples/consumer_demo && bazel test //...`.
Profiler build switch: `bazel build //... --define graph_runtime_profiler=true` enables real profiler; default uses no-op stub.
Profiler module: `src/framework/profiler/` — `GraphProfiler` (real), `GraphProfilerStub` (no-op), `TimeHistogram`, `Clock`.
<!-- SPECKIT END -->
