# Quickstart: Profiler Framework Integration

This guide shows how library consumers use the profiler.

## Enable Profiling via Config File

The `profiler_config` block is embedded in the graph config. Example in JSON format:

```json
{
  "profiler_config": {
    "enable_profiler": true,
    "histogram_interval_size_usec": 2000000,
    "num_histogram_intervals": 5,
    "trace_log_path": "/tmp/profiles"
  },
  "nodes": [
    { "name": "source", "type": "StringProducer", "output_streams": ["source:out"] },
    { "name": "sink", "type": "StringConsumer", "input_streams": ["source:out"] }
  ]
}
```

Other config formats (YAML, Protobuf, etc.) populate the same `GraphConfig::profiler_config` fields with their own syntax.

## Enable Profiling Programmatically

```cpp
#include "graph_runtime/graph_runtime.h"

using namespace graph::runtime;

int main() {
  GraphRuntime runtime;

  // Option A: via programmatic config (overrides config file)
  ProfilerConfig pcfg;
  pcfg.enable_profiler = true;
  pcfg.histogram_interval_size_usec = 1000000;
  runtime.SetProfilerConfig(pcfg);

  // Option B: via config file (any supported format)
  GraphConfig config = ParseGraphConfig("pipeline.json");
  runtime.Initialize(config);

  // Execute graph
  runtime.Start();
  runtime.WaitUntilDone();

  // Read profiles (method 1: dedicated handle)
  auto* profiler = runtime.profiler();
  auto profiles1 = profiler->GetNodeProfiles();

  // Read profiles (method 2: convenience method)
  auto profiles2 = runtime.GetNodeProfiles();

  for (const auto& p : profiles2) {
    printf("Node: %s\n", p.node_name.c_str());
    printf("  Open:   %lld us\n", p.open_runtime_usec);
    printf("  Close:  %lld us\n", p.close_runtime_usec);
    printf("  Process calls: %lld\n", p.process_count);
    printf("  Process total: %lld us\n", p.process_time_total_usec);
    printf("  Process mean:  %.2f us\n", p.process_time_mean_usec);
  }

  // Save profile to JSON file for offline analysis
  auto status = runtime.WriteProfile("/tmp/profile.json");
  assert(status.ok());

  return 0;
}
```

## Analyze Profiles with CLI Tool

```bash
# Build the CLI tool
bazel build //src/framework/profiler/reporter/tools:print_profile --define graph_runtime_profiler=true

# Generate report from a profile file
./bazel-bin/src/framework/profiler/reporter/tools/print_profile \
  --files=/tmp/profile.json

# Compare two runs
./bazel-bin/src/framework/profiler/reporter/tools/print_profile \
  --files=/tmp/baseline.json,/tmp/experiment.json --compare

# Output as CSV for spreadsheet import
./bazel-bin/src/framework/profiler/reporter/tools/print_profile \
  --files=/tmp/profile.json --format=csv
```

## Build with Profiler Enabled

```bash
bazel build //... --define graph_runtime_profiler=true
```

## Build without Profiler (default, zero overhead)

```bash
bazel build //...
```

## Full Demo Example

A complete, runnable example is at `src/examples/profiler_demo.cc`. It demonstrates the full profiler workflow with a node that simulates ~5ms of compute work per Process call.

### Build & Run

```bash
# Build with real profiler enabled
bazel build //src/examples:profiler_demo --define graph_runtime_profiler=true

# Run
bazel run //src/examples:profiler_demo --define graph_runtime_profiler=true
```

### Expected Output

```
=== Profiler Demo ===

Config: 1 node(s), profiler enabled=true
Running graph (sync mode)...
Graph completed.

=== Profile Results ===
Node: work
  Process calls: 1
  Process total: 5038 us
  Process mean:  5038.00 us
  Open:          683 us
  Close:         10 us

Profile saved to: /tmp/profiler_demo_profile.json

=== CLI Analysis ===
Table output:
  print_profile --files=/tmp/profiler_demo_profile.json
```

### Analyze with CLI

```bash
bazel build //src/framework/profiler/reporter/tools:print_profile

# Table output
./bazel-bin/src/framework/profiler/reporter/tools/print_profile \
  --files=/tmp/profiler_demo_profile.json

# CSV output
./bazel-bin/src/framework/profiler/reporter/tools/print_profile \
  --files=/tmp/profiler_demo_profile.json --format=csv
```

### Stub Mode (Default Build)

Without `--define graph_runtime_profiler=true`, the profiler compiles as a no-op stub.
`GetNodeProfiles()` returns an empty vector and `WriteProfile()` returns `OkStatus`
without performing any I/O. This ensures zero overhead in production builds.
The demo binary still runs and shows `(No profile data ...)` — useful for
verifying the API compiles and doesn't crash in either mode.

```bash
bazel run //src/examples:profiler_demo
```
