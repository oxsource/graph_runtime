# Public API Contract: Profiler Framework

## Consumer-Facing API

### Headers

```cpp
// Public umbrella header (already includes this)
#include "graph_runtime/graph_runtime.h"

// Standalone include if needed
#include "graph_runtime/profiler.h"
```

### Types

```cpp
namespace graph::runtime {

// Forward declaration — used only for handle access via GraphRuntime::profiler()
class ProfilingContext;

// Configuration structure
struct ProfilerConfig {
  bool enable_profiler = false;                    // Master enable/disable
  int64_t histogram_interval_size_usec = 1000000;  // Bucket width (microseconds)
  int num_histogram_intervals = 5;                 // Number of buckets
  std::string trace_log_path;                      // Default output dir; empty = require explicit path
};

// Per-node profile result
struct NodeProfile {
  std::string node_name;               // Node name
  int64_t open_runtime_usec = 0;             // Last Open() duration
  int64_t close_runtime_usec = 0;            // Last Close() duration
  int64_t process_count = 0;                 // Number of Process() calls
  int64_t process_time_total_usec = 0;       // Total Process() time
  double process_time_mean_usec = 0.0;       // Mean Process() time
};

}  // namespace graph::runtime
```

### GraphRuntime Methods

```cpp
namespace graph::runtime {

class GRAPH_RUNTIME_API GraphRuntime {
 public:
  // ── Profiler Configuration ──

  // Set profiler config before Initialize(). Overrides the config from
  // graph config file (any format) if both are provided.
  void SetProfilerConfig(const ProfilerConfig& config);

  // ── Profiler Access ──

  // Returns the raw ProfilingContext handle for direct access.
  // Returns non-null pointer regardless of build-time switch;
  // methods are no-ops when profiler is disabled.
  ProfilingContext* profiler();
  const ProfilingContext* profiler() const;

  // Convenience: returns per-node profiles after graph execution.
  // Empty vector when profiling is disabled.
  std::vector<NodeProfile> GetNodeProfiles() const;

  // ── Profile Persistence ──

  // Serialize all accumulated profile data to a JSON file at |path|.
  // Returns OkStatus on success, error status on file I/O failure.
  // No-op (returns OkStatus) when profiling is disabled.
  absl::Status WriteProfile(const std::string& path) const;
};

}  // namespace graph::runtime
```

## CLI Tool Contract

```bash
# Report from a single profile file
print_profile --files=/tmp/run1.json

# Report with node filter (glob pattern)
print_profile --files=/tmp/run1.json --node-filter="*source*"

# Compare two profile files (show deltas)
print_profile --files=/tmp/run1.json,/tmp/run2.json --compare

# CSV output for spreadsheet import
print_profile --files=/tmp/run1.json --format=csv

# Save to file instead of stdout
print_profile --files=/tmp/run1.json --output=/tmp/report.txt
```

## Config Contract (Format-Agnostic)

The profiler configuration is embedded in the graph config file under the `profiler_config` key. The exact syntax depends on the config format (JSON shown below; YAML, Protobuf, etc. follow their own syntax for the same fields):

```json
{
  "profiler_config": {
    "enable_profiler": true,
    "histogram_interval_size_usec": 2000000,
    "num_histogram_intervals": 5,
    "trace_log_path": "/var/log/graph_profiles"
  },
  "nodes": [...]
}
```

All fields in `profiler_config` are optional. When absent, defaults from `ProfilerConfig` apply.

## Build-Time Contract

```bash
# Enable profiler (real implementation)
bazel build //... --define graph_runtime_profiler=true

# Default (stub / no-op profiler)
bazel build //...
```

## Behavioral Contract

| Scenario | GetNodeProfiles() | WriteProfile(path) | Notes |
|----------|------------------------|--------------------|-------|
| Build: profiler disabled | `{}` (empty) | No-op, OkStatus | Compile-time zero overhead |
| Build: enabled, Config: enable_profiler=false | `{}` (empty) | No-op, OkStatus | Runtime no-op |
| Build: enabled, Config: enable_profiler=true, graph executed | Filled profiles | Valid JSON file | Real timing data |
| Called before graph execution | `{}` (empty) | Empty JSON (0 nodes) | No data yet |
| Called during graph execution | Partial data | Partial data in JSON | Best-effort snapshot |
| After Reset() | `{}` (empty) | Empty JSON (0 nodes) | Data cleared |
| Invalid path | N/A | Error status | No crash |
