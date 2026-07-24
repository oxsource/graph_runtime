# Implementation Plan: MediaPipe Gap Closure

**Branch**: `009-mediapipe-gap-closure` | **Date**: 2026-07-24 | **Spec**: [spec.md](./spec.md)

## Summary

分 4 个 Layer 逐步消除 graph_runtime 与 MediaPipe 之间的功能缺口。Layer 1（公共 API stub）最急迫，对外部消费者影响最大；Layer 2（调度器）是核心痛点，直接影响 `WaitUntilDone` 的正常工作。

## Constitution Check

| Principle | Status | Notes |
|-----------|--------|-------|
| I. 现状对齐 | 🔴 Partial | 当前有 4 个公开 API 是空 stub，违反"用户期望它能工作" |
| V. Build System Integrity | ✅ PASS | 纯新增/修改代码，非重构/移动 |
| Design: Public API path | ✅ PASS | 不改变公开头文件路径 |

## Target Architecture

```text
Phase 1 (P1) —— 填补公共 API stub
  SetOutputStreamCallback     set_callback → 注册 → OutputStreamHandler 回调
  SetInputSidePacket          set_packet → GraphContext.InputSidePackets()
  SetOutputSidePacketCallback set_callback → 注册 → Node.Close() 触发

Phase 2 (P1) —— 修复调度器缺陷
  Start() 异步路径 → 事件驱动循环 → WaitUntilDone 正常工作
  Schedule() 保留同步模式，但 Start() 获得真正的异步调度

Phase 3 (P2) —— 新增图生命周期查询
  WaitForIdle() / HasGraphFinished() / GetGraphState()
  Pause() / Resume()

Phase 4 (P2) —— 强化校验与扩展
  ConfigValidator 连通性/循环检测
  运行时类型检查

Phase 5 (P1) —— Tag/Index Stream
  TagMap / ParseTagIndexName / 索引 AddPacketToInputStream
```

## Impact — Quantified

| Metric | Count |
|--------|-------|
| Files to modify | ~15 |
| New test files | ~3 |
| Stub methods to implement | 7 |
| New public API methods | 4 |
| Existing tests to remain passing | 14+ |

## Phases

### Phase 1 — Public API Stub Closure (P1)

**Target**: `src/framework/public/graph_runtime.cc`, `src/framework/scheduler/scheduler.h`, `src/framework/stream/output_stream_handler.h`

1. `SetOutputStreamCallback(name, cb)` — 在 `OutputStreamHandler` 中存储回调，`PostProcess` 时触发
2. `ClearOutputStreamCallback(name)` — 清除已注册回调
3. `SetInputSidePacket(tag, packet)` — 存储在 `GraphRuntime` 的 side_packet_map_ 中，初始化时传入 `GraphContext`
4. `SetOutputSidePacketCallback(name, cb)` — 在 Node/GraphContext Close 时触发
5. `Pause()` / `Resume()` — 实现状态切换和条件变量等待
6. 为上述每项编写单元测试

**Verification**: `bazel test //src/tests:...` 新增测试通过

### Phase 2 — Async Scheduler Fix (P1)

**Target**: `src/framework/scheduler/scheduler.cc`, `src/framework/scheduler/scheduler_queue.cc`, `src/examples/add_packet_demo.cc`, `src/examples/async_pipeline_demo.cc`

1. 修复 `Start()` 异步路径：建立事件驱动调度循环替代仅 `HandleIdle()` 触发
2. 修复 `scheduler_queue.cc` 固定 `Timestamp(1)` 问题，使用真实时间戳
3. 确保 `WaitUntilDone()` 在异步路径中正确等待 `kTerminated`
4. 修改 demo 使用 `WaitUntilDone()` 替代 `Shutdown()`

**Verification**: Demos 使用 `WaitUntilDone()` 而非 `Shutdown()` 并能正常退出

### Phase 3 — Lifecycle Query API (P2)

**Target**: `src/framework/public/graph_runtime.h`, `src/framework/scheduler/scheduler.h`

1. `WaitForIdle()` — 在队列空闲时触发条件变量
2. `HasGraphFinished()` — 结合 `state()` 和输入流关闭状态
3. `GetGraphState()` — 公开 `SchedulerState` 枚举
4. `Pause()` / `Resume()` 如有未完成实现继续完成

**Verification**: 新 API 单元测试通过

### Phase 4 — Config Validation & Runtime Checks (P2)

**Target**: `src/framework/config/config_validator.cc`, `src/framework/node/node_contract.h`

1. 连通性验证：每个节点的 input_stream 都能追溯到上游节点
2. 循环检测：使用 DFS 检测有向环
3. 可选：类型匹配运行时检查（在 `Process` 调用时验证 packet 类型）

**Verification**: `bazel test //...` 全部通过

### Phase 9 — Tag/Index Stream Support (P1)

**Target**: `src/framework/tool/tag_map.h`, `src/framework/tool/validate_name.h`, `src/framework/node/node_contract.h`, `src/framework/public/graph_runtime.h`

1. **TagMap**: 新建 `src/framework/tool/tag_map.h`，解析 `"TAG:index:name"` → `{tag, index, name}`，维护 `tag → {first_id, count}` 映射
2. **ParseTagIndexName**: 新建 `src/framework/tool/validate_name.h`，提供 `ParseTagIndexName("VIDEO:2:left")` → `{tag="VIDEO", index=2, name="left"}`
3. **PacketTypeSet 索引访问**: `node_contract.h` 增加 `Get(tag, index)` const 和非 const 重载
4. **AddPacketToInputStream 索引重载**: `graph_runtime.h` 增加 `AddPacketToInputStream(tag, index, packet)`，同时保留 `AddPacketToInputStream("TAG:index", packet)` 字符串解析
5. **Test**: TagMap 单元测试、索引投递集成测试

**Verification**: `bazel test //...` 全部通过，新增索引投递测试通过

### Phase 10 — Runtime Engine Hardening (P1)

**Target**: `src/framework/scheduler/input_stream_handler.h/cc`, `src/framework/node/graph_context.h/cc`, `src/framework/scheduler/scheduler_queue.h/cc`, `src/framework/scheduler/counters.h`

**Layer 1 — InputStreamHandler Strategies:**
1. DefaultInputStreamHandler 完善同步逻辑（当前已基本可用）
2. **ImmediateInputStreamHandler**: 任何输入到达立即调度节点，不等待其他输入
3. **SyncSetInputStreamHandler**: 所有输入在同一 timestamp 都有数据时才调度
4. **FixedSizeInputStreamHandler**: 每个输入队列固定大小，超限丢弃或阻塞
5. 为每种 handler 注册工厂/配置支持

**Layer 2 — MaxInFlight 约束:**
6. 在 `SchedulerQueue::AddNode()` 检查 `node->GetContract().MaxInFlight()`
7. 同一节点已运行的实例数达到上限时推迟调度
8. `Node::EndScheduling()` 释放 slot

**Layer 3 — GraphContext 回收:**
9. 实现 `GraphContextManager::PrepareContext()`：从池中取或新建
10. 实现 `GraphContextManager::RecycleContext()`：归还回池
11. SchedulerQueue 中 Process 完成后调用 Recycle

**Layer 4 — Batch 调度:**
12. `ScheduleInvocations(max_allowance)` 根据 allowance 批量调度节点
13. 替代单次 `AddNode` → `SubmitToExecutor` 循环

**Layer 5 — Performance Counters:**
14. `PerfCounters` 实例化并挂接到 Scheduler 和 SchedulerQueue
15. `tasks_submitted` / `tasks_completed` / `packets_processed` 在工作流中 Increment

**Layer 6 — Cancel 机制:**
16. `Scheduler::Cancel()` 实现：state → kCancelling，SetQueuesRunning(false)
17. `HandleIdle()` 中 kCancelling 状态下 drain 待处理工作并终止
18. `GraphRuntime::Cancel()` 公开方法

**Verification**: `bazel test //...` 全部通过，新增 15+ 测试

## Dependencies & Execution Order

```
Phase 1 (Public API stubs) ──→ Phase 2 (Async scheduler)
                                        ↓
                                Phase 3 (Lifecycle queries)
                                        ↓
                                Phase 4 (Validation)
```

Phase 1 与 Phase 2 可部分并行（不同文件），但 Phase 2 的 demo 改造依赖 Phase 1 的回调机制。
Phase 3 依赖 Phase 2 的 WaitUntilDone 修复。
Phase 4 独立。

## MVP Scope

Phase 1 + Phase 2 = **核心 2 阶段** — 消除所有 stub 并修复 `WaitUntilDone`。此时 demos 可用 `WaitUntilDone()` 正常退出。
