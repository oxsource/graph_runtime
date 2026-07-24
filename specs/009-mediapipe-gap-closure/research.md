# Research: MediaPipe Gap Analysis

## Background

`add_packet_demo.cc` 注释中提到 `WaitUntilDone` 尚未支持，项目与 MediaPipe 之间存在功能缺口。经全面代码审查，以下是完整的差距分析。

## 一、已声明但未实现的方法（直接 stub）

| 方法 | 位置 | stub 状态 |
|---|---|---|
| `Scheduler::Pause()` / `Resume()` | `scheduler.cc:281,285` | `UnimplementedError` |
| `Scheduler::AddNode()` / `RemoveNode()` | `scheduler.cc:314,318` | 标注"Phase 2" |
| `GraphRuntime::SetOutputStreamCallback()` | `graph_runtime.cc:138` | 空函数体 |
| `GraphRuntime::ClearOutputStreamCallback()` | `graph_runtime.cc:142` | 空函数体 |
| `GraphRuntime::SetInputSidePacket()` | `graph_runtime.cc:145` | 空函数体 |
| `GraphRuntime::SetOutputSidePacketCallback()` | `graph_runtime.cc:150` | 空函数体 |
| `GraphContextManager::PrepareCalculatorContext()` / `RecycleCalculatorContext()` | `graph_context.h:199` | 标注 Phase 2 |

## 二、功能存在缺陷的部分

| 功能 | 问题 |
|---|---|
| **`Schedule()` 同步调度** | 纯同步循环，非真正的多线程异步调度器。对所有非源节点做扁平循环，无优先级/时间戳驱动 |
| **`Start()` 异步路径** | 仅 `HandleIdle()` 触发，缺乏主动调度循环。节点 Process 在 `scheduler_queue.cc:79` 固定用 `Timestamp(1)` 而非真实时间戳 |
| **运行时类型检查** | `NodeContract` 声明了输入/输出类型，但调度器从未验证 |
| **`MaxInFlight` 约束** | `NodeContract` 中有该字段但调度器不执行 |
| **`InputStreamHandler` 策略** | 只有 `DefaultInputStreamHandler`，缺少 MediaPipe 的 `ImmediateInputStreamHandler`、`SyncSetInputStreamHandler`、`FixedSizeInputStreamHandler` |
| **Config 校验** | 只有名称唯一性检查，缺少连通性、循环检测、节点依赖验证 |
| **`PerfCounters`** | `counters.h` 定义了计数器但从未实例化/连接 |

## 三、MediaPipe 有但 graph_runtime 完全缺失的 API

- `WaitForIdle()` / `WaitForGraphIdle()` / `HasGraphFinished()`
- `GetGraphState()` 公开暴露（内部有 `state()` 但未暴露为 public API）
- 批处理（`ScheduleInvocations` 的 `max_allowance` 存在但未使用）
- Task 合并/去重（MediaPipe 的 `Task` 类）
- 队列超时/唤醒定时器
- 流级别的 `input_stream_info`（每个流的 FINISH/SYNC/BACK_MIX 策略）

## 四、测试空白

- `src/tests/scheduler_test.cc` — 空文件（0 行）
- `src/tests/integration_test.cc` — 空文件（0 行）

## 五、影响范围分析

```
Layer 1: 公共 API stub（影响外部队）
  ├── SetOutputStreamCallback / ClearOutputStreamCallback
  ├── SetInputSidePacket / SetOutputSidePacketCallback
  ├── Pause / Resume
  └── WaitForIdle / HasGraphFinished / GetGraphState

Layer 2: 调度器缺陷（影响功能性）
  ├── Start() + WaitUntilDone 不工作
  ├── Schedule() 伪异步实现
  └── 无时间戳驱动的节点调度

Layer 3: 校验与安全性（影响防御性编程）
  ├── ConfigValidator 缺少连通性/循环检测
  └── 运行时类型检查缺失

Layer 4: 扩展性（影响高级用例）
  ├── 缺少 InputStreamHandler 策略变体
  ├── 缺少批处理/Task 合并
  └── PerfCounters 未接入
```

## 相关文件索引

| # | 文件 | 角色 |
|---|---|---|
| 1 | `src/framework/public/graph_runtime.h` | 主公开 API 类 |
| 2 | `src/framework/public/graph_runtime.cc` | 实现（含 stub 方法） |
| 3 | `src/framework/public/graph_builder.h` | Builder API |
| 4 | `src/framework/scheduler/scheduler.h` | 调度器 |
| 5 | `src/framework/scheduler/scheduler.cc` | 含 4 个 UnimplementedError |
| 6 | `src/framework/scheduler/scheduler_queue.cc` | 固定 Timestamp(1) 问题 |
| 7 | `src/framework/scheduler/input_stream_handler.h` | SyncSet + DefaultInputStreamHandler |
| 8 | `src/framework/config/config_validator.cc` | 仅基础校验 |
| 9 | `src/framework/config/graph_config.h` | GraphConfig 定义 |
| 10 | `src/framework/node/graph_context.h` | Phase 2 标注 |
| 11 | `src/framework/scheduler/counters.h` | 未使用计数器 |
| 12 | `src/examples/add_packet_demo.cc` | 使用 Shutdown() 而非 WaitUntilDone() |
