# Quickstart: Profiler Framework Integration

This guide shows how library consumers use the profiler.

Profiling is controlled at runtime via `ProfilerConfig::enable_profiler`. The same
binary can enable or disable profiling without recompilation. When disabled, the
Scope RAII wrapper performs a single atomic load (~1-2ns) and returns — no clock
reads or histogram updates occur.

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
bazel build //src/framework/profiler/reporter/tools:print_profile

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

## Full Demo Example

A complete, runnable example is at `src/examples/profiler_demo.cc`. It demonstrates
the full profiler workflow with a `ProfiledWorkNode` that emits 5 packets, each
preceded by a ~5ms busy-wait loop to simulate compute load.

### Build & Run

```bash
# Build (profiler is always compiled; runtime control via config)
bazel build //src/examples:profiler_demo

# Run — profiling enabled by default in the demo
bazel run //src/examples:profiler_demo
```

### Expected Output

Running the demo produces output similar to:

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
  Open:          722 us
  Close:         9 us

Profile saved to: /tmp/profiler_demo_profile.json
```

#### 字段含义

| 字段 | 说明 | 示例值 |
|------|------|--------|
| `Process calls` | Process() 被调用的次数 | `1` |
| `Process total` | 所有 Process() 调用累计耗时（微秒） | `5038 us` |
| `Process mean`  | 单次 Process() 平均耗时 | `5038.00 us` |
| `Open`          | Open() 调用耗时（微秒） | `722 us` |
| `Close`         | Close() 调用耗时（微秒） | `9 us` |

> `Process calls` 为 1 是因为 demo 使用 sync path（`Schedule()`），source node
> 完成一轮数据生成后返回 `StatusStop()`，Process 仅在活跃期间被调用一次。
> Open/Close 各被调用一次。若使用 async path（`Start()`），则每个下游节点
> 的 Process 在每次收到数据包时都会触发。

### 分析 Profile 文件

#### 1. 表格输出

```bash
print_profile --files=/tmp/profiler_demo_profile.json
```

输出示例：

```
                  Node  Count  Mean(us)  Total(us)  Open(us)  Close(us)
──────────────────────  ─────  ────────  ─────────  ────────  ─────────
                  work      1   5038.00       5038       722         9
──────────────────────  ─────  ────────  ─────────  ────────  ─────────
                 TOTAL      1   5038.00       5038    —       —
```

- **Count**: Process 调用次数
- **Mean(us)**: 单次 Process 平均耗时（= Total / Count）
- **Total(us)**: 所有 Process 累计耗时
- **Open(us) / Close(us)**: 节点 Open/Close 的耗时
- **TOTAL 行**: 所有节点聚合值

#### 2. CSV 输出（可导入电子表格）

```bash
print_profile --files=/tmp/profiler_demo_profile.json --format=csv
```

```csv
Node,Count,Mean(us),Total(us),Open(us),Close(us)
work,1,5038,5038,722,9
```

#### 3. 节点过滤

只显示名称包含 `work` 的节点：

```bash
print_profile --files=/tmp/profiler_demo_profile.json --node-filter="work"
```

#### 4. 对比分析（多文件）

运行两次 demo 得到两个 profile 文件，然后对比差异：

```bash
# 第一次运行
bazel run //src/examples:profiler_demo
cp /tmp/profiler_demo_profile.json /tmp/baseline.json

# 修改代码后第二次运行
bazel run //src/examples:profiler_demo
cp /tmp/profiler_demo_profile.json /tmp/experiment.json

# 对比
print_profile --files=/tmp/baseline.json,/tmp/experiment.json --compare
```

对比输出：

```
                  Node  Mean(us)  Delta(us)  Delta(%)
──────────────────────  ────────  ─────────  ────────
                  work   5038.00     +0.00     +0.0%
```

#### 5. 保存报告到文件

```bash
print_profile --files=/tmp/profiler_demo_profile.json \
  --output=/tmp/report.txt
```

### 禁用 Profiler（运行时关闭，无需重新编译）

在配置中将 `enable_profiler` 设为 `false` 即可关闭 profile 采集。Scope 仅执行
一次原子 load（~1-2ns）后返回，不读时钟、不写直方图。

```bash
# 直接运行 demo — 但 demo 中写死了 enable_profiler: true。
# 实际项目中通过 config 文件或 SetProfilerConfig() 控制：
#   ProfilerConfig pcfg;
#   pcfg.enable_profiler = false;
#   runtime.SetProfilerConfig(pcfg);
```

无需任何编译期 flag（`--define`），同一份二进制支持 profile 启用/禁用两种模式。
