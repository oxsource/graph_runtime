# Data Model: Profiler Framework

## Entities

### ProfilerConfig

Configuration structure that controls profiling behavior.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `enable_profiler` | bool | `false` | Master switch; if false, all profiling is a no-op at runtime |
| `histogram_interval_size_usec` | int64 | `1000000` | Width of each histogram bucket in microseconds (1 second default) |
| `num_histogram_intervals` | int | `5` | Number of histogram buckets |
| `trace_log_path` | string | `""` | Default output directory for `WriteProfile()`; empty = require explicit path |

**Validation**: `histogram_interval_size_usec > 0`, `num_histogram_intervals > 0`

---

### TimeHistogram

Bucket-based accumulator for runtime duration samples.

| Field | Type | Description |
|-------|------|-------------|
| `interval_size_usec_` | int64 | Width of each bucket in microseconds |
| `num_intervals_` | int | Number of buckets (fixed at initialization) |
| `count_` | int64 | Total number of samples accumulated |
| `total_` | int64 | Sum of all sample durations in microseconds |
| `buckets_` | vector<int64> | Per-bucket counts; index = floor(duration / interval_size), capped at num_intervals-1 |
| `mutex_` | mutex | Protects non-atomic fields during concurrent access |

**Operations**:
- `Initialize(interval_size_usec, num_intervals)` — allocate buckets, reset counters
- `AddSample(start_usec, end_usec)` — compute `duration = end_usec - start_usec`, increment `count_`, add to `total_`, increment appropriate bucket
- `Reset()` — zero all counters and buckets
- `mean()` — `total_ / count_` (0 if count == 0)

---

### NodeProfile

Per-node profile result exposed through the public API.

| Field | Type | Description |
|-------|------|-------------|
| `node_name` | string | Node name as declared in the graph |
| `open_runtime_usec` | int64 | Duration of the last `Open()` call in microseconds |
| `close_runtime_usec` | int64 | Duration of the last `Close()` call in microseconds |
| `process_count` | int64 | Number of `Process()` invocations measured |
| `process_time_total_usec` | int64 | Sum of all Process durations in microseconds |
| `process_time_mean_usec` | double | Mean Process duration in microseconds |

---

### PerNodeData (internal)

Internal per-node storage within `GraphProfiler`.

| Field | Type | Description |
|-------|------|-------------|
| `open_runtime_usec` | atomic<int64> | Last Open() runtime; written by SetOpenRuntime |
| `close_runtime_usec` | atomic<int64> | Last Close() runtime; written by SetCloseRuntime |
| `process_runtime` | TimeHistogram | Histogram of all Process() runtimes |

---

### Clock (abstract interface)

| Method | Return | Description |
|--------|--------|-------------|
| `TimeNowUsec()` | int64 | Returns current monotonic time in microseconds |

**Concrete implementations**:
- `RealClock` — wraps `std::chrono::steady_clock`
- (future) `MockClock` — returns preset timestamps for testing

---

### ProfilingContext::Scope (RAII wrapper)

| Member | Type | Description |
|--------|------|-------------|
| `type_` | EventType | OPEN, PROCESS, or CLOSE |
| `node_name_` | string | Target node name |
| `profiler_` | ProfilingContext* | Owning profiler instance |
| `start_time_usec_` | int64 | Timestamp read on construction |

**Lifecycle**:
1. Constructor: if `profiler_->is_profiling_` is true, read clock → `start_time_usec_`
2. Destructor: if `profiler_->is_profiling_` is true, read clock, dispatch to profiler based on `type_`

## State Transitions

### ProfilingContext lifecycle

```
Created (not initialized)
    ↓ Initialize(config, node_names)
Initialized
    ↓ Start()
Running (is_profiling_ = true)
    ↓ Pause()          ↓ Stop()
Paused                 Stopped
    ↓ Resume()
Running
```

### Per-node data lifecycle

```
Node added to graph
    ↓ profiling started
Accumulating samples
    ↓ Reset()
Cleared (count=0, total=0, buckets zeroed)
    ↓ new Process()
Accumulating samples
```

### Profile JSON File

On-disk serialization of `NodeProfile` data.

| Field | Type | Description |
|-------|------|-------------|
| `capture_time` | string | ISO 8601 timestamp of when the profile was captured |
| `node_count` | int | Number of node entries |
| `profiler_config` | object | The `ProfilerConfig` used during capture |
| `nodes` | array | Array of serialized `NodeProfile` objects |

Each node entry includes:
```json
{
  "node_name": "node_name",
  "open_runtime_usec": 42,
  "close_runtime_usec": 0,
  "process_count": 100,
  "process_time_total_usec": 123456,
  "process_time_mean_usec": 1234.56,
  "process_runtime": {
    "interval_size_usec": 1000000,
    "num_intervals": 5,
    "count": 100,
    "total_usec": 123456,
    "buckets": [80, 15, 3, 1, 1]
  }
}
```

---

### ProfileReport

Aggregated result produced by the `Reporter`.

| Field | Type | Description |
|-------|------|-------------|
| `nodes` | vector\<NodeStats\> | Per-node aggregate statistics |
| `total_process_count` | int64 | Sum of all `process_count` across nodes |
| `total_process_time_usec` | int64 | Sum of all `process_time_total_usec` across nodes |

**NodeStats** sub-entity:

| Field | Type | Description |
|-------|------|-------------|
| `node_name` | string | Node name |
| `open_runtime_usec` | int64 | Open time (last value, or mean if aggregated) |
| `close_runtime_usec` | int64 | Close time (last value, or mean if aggregated) |
| `process_count` | int64 | Number of Process calls |
| `process_time_total_usec` | int64 | Total Process time |
| `process_time_mean_usec` | double | Mean Process time |
| `process_time_min_usec` | int64 | Minimum observed Process time |
| `process_time_max_usec` | int64 | Maximum observed Process time |

---

### Reporter

Offline analysis engine. Processes profile JSON files and produces `ProfileReport`.

| Method | Description |
|--------|-------------|
| `Accumulate(json_path)` | Load and merge one profile file |
| `Report()` | Returns aggregated `ProfileReport` |
| `Compare(baseline)` | Returns per-node deltas vs a baseline `ProfileReport` |
| `Clear()` | Reset all accumulated data |

---

## State Transitions

### ProfilingContext lifecycle

```
Created (not initialized)
    ↓ Initialize(config, node_names)
Initialized
    ↓ Start()
Running (is_profiling_ = true)
    ↓ Pause()          ↓ Stop()         ↓ WriteProfile()
Paused                 Stopped           JSON file on disk
    ↓ Resume()                                            ↓ Reporter::Accumulate()
Running                                                   Report data
```

### Per-node data lifecycle

```
Node added to graph
    ↓ profiling started
Accumulating samples
    ↓ Reset()                   ↓ WriteProfile() / GetNodeProfiles()
Cleared (count=0, ...)          Snapshot (data preserved)
    ↓ new Process()
Accumulating samples
```

## Relationships

```
GraphRuntime (1) ──owns──▶ ProfilingContext (1)
  │                              │
  │                              ├── ProfilerConfig (1)
  │                              ├── Clock (1)
  │                              ├── WriteProfile() ──▶  JSON file (0..N)
  │                              └── map<string, PerNodeData> (0..N)
  │                                    │
  │                                    └── TimeHistogram (1 per node)
  │
  ├── Scheduler (1) ──references──▶ ProfilingContext (1)
  │     │
  │     └── SchedulerQueue (1..N) ──references──▶ ProfilingContext (1)
  │
  └── Reporter (0..1) ──reads──▶ JSON file (0..N)
        │
        └── print_profile CLI ──uses──▶ Reporter
```
