# Tasks: MediaPipe Gap Closure

**Input**: Design documents from `/specs/009-mediapipe-gap-closure/`

**Prerequisites**: plan.md (required), spec.md (required), research.md

## Path Conventions

- Workspace root: `graph_runtime/graph_runtime/`
- All paths relative to workspace root

---

## Phase 1: Public API Stub Closure (P1)

**Purpose**: 实现 4 个空 stub 方法，让公共 API 真正可用。

- [ ] T001 Implement `SetOutputStreamCallback(name, cb)` — 在 OutputStreamHandler 存储回调，PostProcess 时触发
- [ ] T002 Implement `ClearOutputStreamCallback(name)` — 清除已注册回调
- [ ] T003 Implement `SetInputSidePacket(tag, packet)` — 存储在 side_packet_map_，初始化传入 GraphContext
- [ ] T004 Implement `SetOutputSidePacketCallback(name, cb)` — Node::Close 时触发
- [ ] T005 Test: OutputStreamCallback 注册与触发
- [ ] T006 Test: InputSidePacket 注入与读取
- [ ] T007 Test: OutputSidePacketCallback 触发
- [ ] T008 Implement `Pause()` / `Resume()` — 状态切换 + 条件变量等待

**Checkpoint**: 4 stub 方法全部实现，新增单元测试通过 ✅

---

## Phase 2: Async Scheduler Fix (P1)

**Purpose**: 修复 `Start()` + `WaitUntilDone()` 异步路径，使 demos 不再依赖 `Shutdown()`。

- [ ] T009 Fix `Start()` async path — 建立事件驱动调度循环
- [ ] T010 Fix `scheduler_queue.cc` — 使用真实时间戳替代固定 `Timestamp(1)`
- [ ] T011 Fix `WaitUntilDone()` — 确保在异步路径中正确等待 `kTerminated`
- [ ] T012 Update `add_packet_demo.cc` — 使用 `WaitUntilDone()` 替代 `Shutdown()`
- [ ] T013 Update `async_pipeline_demo.cc` — 使用 `WaitUntilDone()` 替代 `Shutdown()`

**Checkpoint**: 两个 demo 用 `WaitUntilDone()` 正常退出 ✅

---

## Phase 3: Lifecycle Query API (P2)

**Purpose**: 新增 MediaPipe 兼容的图生命周期查询 API。

- [ ] T014 Implement `WaitForIdle()` — 队列空闲条件变量
- [ ] T015 Implement `HasGraphFinished()` — 结合 state + 输入流关闭状态
- [ ] T016 Expose `GetGraphState()` — 公开 `SchedulerState` 枚举
- [ ] T017 Continue/fix `Pause()` / `Resume()` if incomplete

**Checkpoint**: 新 API 单元测试通过 ✅

---

## Phase 4: Config Validation & Runtime Checks (P2)

**Purpose**: 强化 ConfigValidator 校验，加入运行时类型检查。

- [ ] T018 Add connectivity validation — 每个 input_stream 可追溯到上游节点
- [ ] T019 Add cycle detection — DFS 检测有向图环
- [ ] T020 Add runtime type checks — Process 调用时验证 packet 类型匹配 NodeContract

**Checkpoint**: `bazel test //...` 全部通过 ✅

---

## Phase 5: Testing & Verification

**Purpose**: 确保构建清洁、测试完备。

- [ ] T021 Fill `src/tests/scheduler_test.cc` — 调度器单元测试
- [ ] T022 Fill `src/tests/integration_test.cc` — 集成测试
- [ ] T023 `bazel build //...` — zero errors, zero warnings
- [ ] T024 `bazel test //...` — all tests pass

**Checkpoint**: `bazel build //... && bazel test //...` 零错误零警告 ✅

---

## Dependencies & Execution Order

```
Phase 1 ──→ Phase 2 ──→ Phase 3 ──→ Phase 4 ──→ Phase 5
  T001-T008   T009-T013   T014-T017   T018-T020   T021-T024
```

### Parallel Opportunities

| Phase | Parallel tasks |
|-------|---------------|
| Phase 1 | T001-T004 (无依赖, 不同模块) |
| Phase 1 | T005-T007 (测试可并行) |
| Phase 4 | T018-T020 (不同验证逻辑) |
| Phase 5 | T021-T022 (不同测试文件) |

### MVP Scope

Phase 1 + Phase 2 = **核心 13 个任务** — 消除所有 stub + 修复 `WaitUntilDone`。
