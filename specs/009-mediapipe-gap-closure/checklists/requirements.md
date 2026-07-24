# Requirements Checklist: MediaPipe Gap Closure

## Phase 1 — Public API Stub Closure

| # | Requirement | Status |
|---|---|---|
| R001 | `SetOutputStreamCallback` 注册回调后，节点输出时触发 | ✅ |
| R002 | `ClearOutputStreamCallback` 清除后不再触发 | ✅ |
| R003 | `SetInputSidePacket` 注入的 side packet 在 Node::Open 时可读 | ✅ |
| R004 | `SetOutputSidePacketCallback` 注册后，节点 Close 产生 side packet 时触发 | ✅ |
| R005 | `Pause()` 暂停图执行 | ✅ |
| R006 | `Resume()` 恢复已暂停的图执行 | ✅ |

## Phase 2 — Async Scheduler Fix

| # | Requirement | Status |
|---|---|---|
| R007 | `Start()` 异步路径事件驱动调度节点 | ✅ |
| R008 | `WaitUntilDone()` 在异步路径中正确等待 kTerminated | ✅ |
| R009 | `scheduler_queue.cc` 使用真实时间戳而非固定 `Timestamp(1)` | ✅ |
| R010 | `add_packet_demo.cc` 使用 `WaitUntilDone()` 替代 `Shutdown()` | ✅ |
| R011 | `async_pipeline_demo.cc` 使用 `WaitUntilDone()` 替代 `Shutdown()` | ✅ |

## Phase 3 — Lifecycle Query API

| # | Requirement | Status |
|---|---|---|
| R012 | `WaitForIdle()` 在队列空闲时返回 | ✅ |
| R013 | `HasGraphFinished()` 返回 bool 状态 | ✅ |
| R014 | `GetGraphState()` 公开 SchedulerState 枚举 | ✅ |

## Phase 4 — Config Validation

| # | Requirement | Status |
|---|---|---|
| R015 | 连通性验证：每个 input_stream 可追溯到上游节点 | ✅ |
| R016 | 循环检测：DFS 检测有向图环 | ✅ |
| R017 | 运行时类型检查：Process 时验证 packet 类型匹配 NodeContract | ✅ |

## Phase 5 — Testing

| # | Requirement | Status |
|---|---|---|
| R018 | `src/tests/scheduler_test.cc` 包含调度器单元测试 | ✅ |
| R019 | `src/tests/integration_test.cc` 包含集成测试 | ✅ |
| R020 | `bazel build //...` 零错误零警告 | ✅ |
| R021 | `bazel test //...` 全部通过 | ✅ |
