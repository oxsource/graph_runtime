<!-- SPECKIT START -->
For additional context about technologies to be used, project structure,
shell commands, and other important information, read the current plan at
`specs/011-stream-fanout/plan.md`, the feature specification at
`specs/011-stream-fanout/spec.md`, the MediaPipe fan-out analysis at
`specs/011-stream-fanout/research.md`, the data model at
`specs/011-stream-fanout/data-model.md`, the project bootstrap at
`graph_runtime/docs/project_bootstrap.md`, and prior design references at
`specs/002-scheduler-stream-packet-design/plan.md`,
`specs/003-config-architecture/plan.md`, `specs/004-library-public-api/plan.md`.

Active feature: 011-stream-fanout — one output stream consumed by many
downstream nodes (1→N fan-out). Input-side managers become per-consumer-edge
(distinct `InputStreamManager` per (consumer node, input edge), no more global
dedup by the full "port:stream" name); graph input streams are modeled as
virtual source `OutputStreamManager`s so `AddPacketToInputStream` fans out to
every consumer. Output-side `AddMirror` + `PropagateUpdatesToMirrors`
(copy N-1 + move last) is kept, matching MediaPipe. Fixes the deadlock that
media_record 004 (recorder + StreamPushNode both consuming `es_packets`)
hit, where the 2nd consumer was starved of an input queue and mis-scheduled
as a never-ending source.

Key architecture references:
- MediaPipe: /Users/moks/Develop/docker/ubuntu24/codes/mediapipe (validated graph / calculator_node / input & output stream managers / AddMirror fan-out)
- Atlas: /Users/moks/Develop/docker/ubuntu24/codes/atlas (Bazel build, public API export, platform config)

Dep prefix convention: ALL BUILD.bazel `deps` must use `@graph_runtime//` prefix (not `//`).
Framework modules live under `src/framework/` — all internal code is in `src/framework/`.
Public headers: `graph_runtime/src/framework/public/include/graph_runtime/` with `strip_include_prefix = "include"`.
Consumer include path: `#include "graph_runtime/graph_runtime.h"` (umbrella header).
Shared library: `bazel build //src/framework/public:runtime_shared` → `libruntime_shared.dylib`.
Consumer demo: `cd graph_runtime/examples/consumer_demo && bazel test //...`.
Build/test: `cd graph_runtime/graph_runtime && bazel build //... && bazel test //...` (workspace root; sync path `Schedule()`, async path `Start()`/`WaitUntilDone()`).
Wiring core: `src/framework/public/graph_runtime.cc` (`GraphRuntime::Initialize`); stream managers in `src/framework/stream/`; input handler in `src/framework/scheduler/input_stream_handler.*`.
<!-- SPECKIT END -->
