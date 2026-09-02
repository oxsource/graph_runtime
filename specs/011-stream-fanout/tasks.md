---

description: "Task list for feature 011-stream-fanout implementation"
---

# Tasks: 流扇出支持（Stream Fan-Out / Multi-Consumer）

**Input**: Design documents from `specs/011-stream-fanout/`

**Prerequisites**: plan.md (required), spec.md (required for user stories), research.md, data-model.md, contracts/

**Tests**: Test tasks are included because spec.md User Stories 1–3 define Independent Tests and Success Criteria SC-001/SC-002 require automated verification.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

## Path Conventions

- **Repository root**: `graph_runtime/`（Bazel workspace 根，即 `codes/graph_runtime/graph_runtime/`）
- **Source**: `src/framework/`；**Tests**: `src/tests/`
- **Spec docs**: `specs/011-stream-fanout/`
- 实现基准：graph_runtime 现有 `src/framework/public/graph_runtime.cc`（`GraphRuntime::Initialize` 约 60–199 行接线、`AddPacketToInputStream`/`CloseInputStream`）；对照 `/Users/moks/Develop/docker/ubuntu24/codes/mediapipe/framework/{calculator_graph.cc,calculator_node.cc}`

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: 复现性测试 + 基线确认，供后续改动对照。

- [X] T001 在 `src/tests/` 建最小复现 `fanout_repro_test.cc`（registered stub 节点：`IntSource` 输出 `output:x`；两个 sink 均 `input:x`；async 运行断言 `WaitUntilDone()` 完成且两路各收满）。先跑红（当前挂死/超时）作为基线
- [X] T002 [P] 确认图输入注入现状基线：`src/tests/stream_io_test.cc`、`scheduler_test.cc` 中单消费者 `AddPacketToInputStream`/`CloseInputStream` 用例当前全绿，记录为回归基线

**Checkpoint**: 复现红用例存在；既有单消费者注入基线绿。

> Phase 1 记录（2026-09-02）：
> - T001 已建 `src/tests/fanout_repro_test.cc` + `BUILD.bazel` 目标 `//src/tests:fanout_repro_test`。
>   - `FanoutIntSource`（`output:x`，emit 5 后 `StatusStop`）+ `FanoutCountingSink`×2（均 `input:x`）。
>   - watchdog 10s 兜底：`WaitUntilDone` 超时则 `Shutdown()` 并断言失败，避免挂死永久阻塞。
>   - **红**：10s 超时失败，报"fan-out: second consumer starved of an input queue and mis-scheduled as a never-ending source"——与 research.md 根因一致。
> - T002 基线：`bazel test //src/tests:all` → 17 个既有测试全绿（含 `scheduler_test`、`stream_io_test` 的单消费者注入/关闭用例）；仅 `fanout_repro_test` 红。回归基线已记录。

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: 核心接线改造 —— 输入侧 per-edge 队列，本阶段完成后 1→N fan-out（US1）可独立成立。

**⚠️ CRITICAL**: US1 的核心修复依赖本阶段；US2（图输入扇出）在本阶段后叠加。

- [X] T003 重构 `GraphRuntime::Initialize` 输入 manager 创建段（`src/framework/public/graph_runtime.cc` 约 66–83 行）：遍历 `config_.nodes`，为每个节点的每个 `input_streams[i]` 建**独立** `InputStreamManager` 并 `SetInputPort(PortName, mgr)` 到该节点；不再按完整 `"port:stream"` 名做跨节点去重
- [X] T004 [P] 调整 arrival/queue-size 回调接线段（`graph_runtime.cc` 约 121–146 行）：回调绑定到"该节点自己的 manager"，不被后声明消费者覆盖
- [X] T005 [P] 调整 `InputStreamHandler` 创建段（`graph_runtime.cc` 约 148–170 行）：每个节点 handler 收集**它自己的** manager 列表（顺序 = `input_streams` 声明顺序）
- [X] T006 复核 mirror 接线段（`graph_runtime.cc` 约 172–196 行）：`output_by_stream` 按 StreamName 解析、`AddMirror(consumer_handler, i)` 保持；因每消费方 handler 指向各自队列，扇出即各收一份
- [X] T007 [P] `ConfigValidator`/`GraphConfig` 复核：确认同一输出流多消费者合法通过；同节点内同名重复消费流被拒绝（validator 或文档约束）
- [X] T008 跑通：`fanout_repro_test`（T001）红转绿；`src/tests:all` 既有用例无回归

**Checkpoint**: 1→N fan-out 成立；图完成（WaitUntilDone）正常；单消费者无回归。

> Phase 2 记录（2026-09-02）：
> - T003：`GraphRuntime::Initialize` 输入创建段改为 per-(node, input edge) 独立 `InputStreamManager`；
>   局部 `node_input_managers`（node name → manager 列表，声明顺序）供回调/handler 接线复用；
>   不再按完整 `"port:stream"` 全图去重；`stream_managers_` 仅保留为图输入注入查找（单消费者兼容，US2 虚拟化后替换）。
> - T004：queue-size/arrival 回调改经 `node_input_managers` 逐 manager 绑定到所属节点；后声明消费者不再覆盖。
> - T005：`InputStreamHandler` 按节点从 `node_input_managers` 收集自己的 manager 列表（顺序 = `input_streams`）。
> - T006：mirror 接线保持（StreamName 解析 + `AddMirror(handler, i)`）；因各消费者 handler 指向各自 per-edge 队列，扇出成立。
> - T007：`ConfigValidator::ValidateNoDuplicateNodeInputs` 新增——同节点重复消费同一流（`"in:x"` + `"in2:x"`）拒绝；
>   `build_validation_test` 新增 `MultiConsumerFanoutPasses`（合法）+ `SameNodeDuplicateStreamConsumptionFails`（拒绝）两用例。
> - T008：`fanout_repro_test` 红转绿（0.1s）；`bazel test //...` 18/18 全绿，无回归。

---

## Phase 3: User Story 1 — 单输出 → 多消费者（1→N fan-out）(Priority: P1) 🎯 MVP

**Goal**: source 一路输出流被 2+ 下游独立消费，各收全量，图正常完成（FR-001/002/003/004/007/008，SC-001）。

**Independent Test**: `IntSource(5帧) → SinkA` + `SinkB` 同流名，async 运行两路各收 5、`WaitUntilDone()` 返回。

### Tests for User Story 1 (write first, expect FAIL before implementation) ⚠️

- [X] T009 [P] [US1] `src/tests/fanout_graph_test.cc`：1 source → 2 sinks 同流名，断言两路各收满、图完成、无超时（对应 T001 复现的正式版）
- [X] T010 [P] [US1] 菱形 DAG：source→(A,B)，A→C 且 B→C（C 双输入），断言三分支均正常、C 按既有 handler 语义处理
- [X] T011 [P] [US1] fan-out 下错误定位：双消费者之一 Process 返回错误，断言 error callback 收到带该节点名的错误、健康分支输入到达 done

### Implementation for User Story 1

- [X] T012 [US1] 依据 Phase 2（T003–T008）交付物完成接线；补 `input_stream_manager`/`output_stream_manager` 复核性小改（如需暴露 manager 归属/遍历以正确接线）
- [X] T013 [US1] 完成语义复核：生产者输出 `Close` → Done 广播全部 mirror → 各消费者 finalize；`scheduler`/`scheduler_queue` 完成判定（非 source 节点输入 done+empty）在 fan-out 下成立

**Checkpoint**: US1 完全可用、独立可测（MVP）。

> Phase 3 记录（2026-09-02）：
> - T009/T010/T011 新增 `src/tests/fanout_graph_test.cc` + `//src/tests:fanout_graph_test`：
>   - T009 `OneSourceToTwoSinksSameStream`：1 source → 2 sinks 同 `input:x`，两路各收 5、`WaitUntilDone()` 返回、均观察到输入 done。
>   - T010 `DiamondDagBothBranchesDrainToJoiningNode`：src→(A,B)，A/B→C 双输入；A/B 各转发 5、C 从两路各收 5。
>   - T011 `FailingConsumerErrorCarriesNodeNameHealthyBranchDrains`：sink_b 在末包报错，error callback 收到含 `sink_b` 的 status，sink_a 仍收满 5。
> - T012：接线复核——per-edge manager + 每节点独立 handler + mirror 接线已由 Phase 2 完成，无缺口。
> - T013：发现并修复**输出型 consumer finalize 缺口**（`scheduler_queue.cc` `RunNode`）：
>   原 `all_inputs_done` 判定要求 `node->GetOutputStreamHandler()` 非空，导致无输出流的 sink（如 recorder/push）永远不被调度观察输入 done；
>   改为 `input_port_count()>0` 即进入 finalize 路径，并对 `FinalizeAction::kClose` 加空指针保护。
>   修复后两路 sink 都能观察到输入 done（saw_done_=true）。对齐 MediaPipe kReadyForClose→EndScheduling。
> - 验证：`fanout_graph_test` 3/3 通过；`bazel test //...` 19/19 全绿；fanout 两测试多轮重跑无 flake。

---

## Phase 4: User Story 2 — 图输入扇出到多消费者 (Priority: P1)

**Intent**: 把图输入建模为虚拟 source 的 OutputStreamManager，`AddPacketToInputStream`/`CloseInputStream` 经 mirror 扇出（FR-005/006/009，SC-002）。

**Independent Test**: 图输入流喂 2 sinks：`AddPacketToInputStream` N 次 + `CloseInputStream` → 两路各收 N、图完成。

### Tests for User Story 2 (write first, expect FAIL before implementation) ⚠️

- [X] T014 [P] [US2] `src/tests/fanout_graph_test.cc`：`config.input_streams=["graph_in"]`，两个节点消费 `graph_in`，注入 N 包 + Close，断言两路各收 N 且 `WaitUntilDone()` 完成
- [X] T015 [P] [US2] 单消费者图输入回归：仅一个节点消费图输入，注入/关闭行为与改动前一致（对照 `stream_io_test`/`scheduler_test` 场景）

### Implementation for User Story 2

- [X] T016 [US2] 在 `GraphRuntime` 内部为 `config.input_streams` 每条创建虚拟 source `OutputStreamManager`（图输入句柄），并让所有声明消费该流的节点按 mirror 接入
- [X] T017 [US2] 改造 `AddPacketToInputStream`/`CloseInputStream`：注入写入虚拟输出 manager（经 `PropagateUpdatesToMirrors` 扇出）；`CloseInputStream` 关闭虚拟 manager → Done 广播各消费者
- [X] T018 [US2] 完成计数复核：`num_open_input_streams_`/`SetTotalGraphInputStreams` 仅计 `config.input_streams`（既有语义保持）

**Checkpoint**: 图输入与内部边共用同一套 mirror/manager 机制（数据模型统一，SC-006）；US1+US2 均绿。

> Phase 4 记录（2026-09-02）：
> - T014/T015 在 `fanout_graph_test.cc` 新增两用例（先红后绿）：
>   - T014 `GraphInputFansOutToTwoSinks`：`config.input_streams=["graph_in"]`，两 sink 均 `input:graph_in`；注入 5 包 + Close → 两路各收 5、`WaitUntilDone()` 完成。
>   - T015 `SingleConsumerGraphInputRegression`：单消费者图输入注入/关闭行为与改动前一致。
> - T016：`graph_runtime.{h,cc}` 新增 `graph_input_outputs_`（stream → 虚拟 `OutputStreamManager*`）。
>   `Initialize` 为每条 `config.input_streams` 建虚拟输出 manager，并插入 `output_by_stream`，让通用 mirror 接线循环对所有声明消费该流的节点 `AddMirror`。
> - T017：`AddPacketToInputStream`/`CloseInputStream` 改为经虚拟输出 manager：
>   - 注入：建 `OutputStreamShard` → `AddPacket` → `ComputeOutputTimestampBound` + `PropagateUpdatesToMirrors`（copy N-1 + move 1）扇出到全部消费者。
>   - Close：`CloseInputStream` → 虚拟 manager `Close()` → Done 广播全部 mirror → 各消费者 finalize。
>   - 未知流仍返回 `NotFoundError`（`stream_io_test` 保持）。
> - T018：完成计数不变——仅 `config.input_streams` 计入 `num_open_input_streams_`/`SetTotalGraphInputStreams`（FR-006）。
> - 清理：删除已无引用的 `stream_managers_`（原注入查找表），`owned_stream_managers_` 保留。
> - 验证：`fanout_graph_test` 5/5（US1 3 + US2 2）；`bazel test //...` 19/19 全绿；fanout/stream_io 多轮重跑无 flake。
> - 数据模型统一达成：图输入与 node-to-node 边走同一套 mirror/manager 机制（SC-006），无"A 独有特判"。

---

## Phase 5: User Story 3 — Done/完成语义与错误路径在 fan-out 下正确 (Priority: P2)

**Intent**: fan-out 下 Done 广播、各消费者独立 finalize、图级错误传播完整（FR-007/008/011，SC-003）。

### Tests for User Story 3 (write first, expect FAIL before implementation) ⚠️

- [X] T019 [P] [US3] fan-out + 生产者 Close：双消费者均观察到输入流 done 并各自 finalize（对照 T011 错误分支，本任务覆盖正常 Close 全分支）
- [X] T020 [P] [US3] 输出流无消费者：`PropagateUpdatesToMirrors` 空 mirror 仅清 shard、不崩溃（既有行为固化）

### Implementation for User Story 3

- [X] T021 [US3] 复核/加固输出 Close → mirror Done 广播路径（`output_stream_manager.cc`），fan-out 下逐消费者正确
- [X] T022 [US3] 复核错误传播路径（error callback 带节点名）在 fan-out 下不被其它分支吞掉

**Checkpoint**: US1+US2+US3 全绿；fan-out 完成/错误语义完备（SC-003）。

> Phase 5 记录（2026-09-02）：
> - T019 `ProducerCloseDoneBroadcastsToAllConsumersAndTheyFinalize`：src Close → 两 sink 均观察 done 且仅于 done 后 finalize（`finalized_after_done_`）。
> - T020 `OutputStreamWithNoConsumersDoesNotCrash`：无消费者输出流，空 mirror 传播仅清 shard、不崩溃、图完成。
> - T021：`OutputStreamManager::Close` 广播 Done 到全部 mirror 已复核；扇出下逐消费者经各自 handler 收 Done，idempotent。
> - T022：T011 补断言——健康分支不仅收满且输入到达 done（不被报错分支吞掉）。
> - **并发健壮性修复**（stress 驱动，`gtest_repeat`+ASan+并行负载复现）：
>   1. **done-observation 丢失**：done 到达时消费者正处于 MaxInFlight 的最后一次包 drain 运行，arrival `AddNode` 被 MaxInFlight 门丢弃 → 节点 idle 且输入 done+empty 但从未 finalize → 图终止（saw_done_ 偶发 false）或永不终止。
>      修复：`scheduler_queue` 新增 `finalized_nodes_` + RunNode 收尾安全网；`DrainInputQueues` 对 done+empty 但未 finalize 的非 source 节点调度 finalize 并延迟终止（MediaPipe kReadyForClose 语义）。`close_pending_`/`finalized_nodes_` 状态转换回到锁内（原实现有锁，重写时误删，已恢复；并发 worker 无数据竞争）。
>   2. **teardown use-after-free / SEGV**：`GraphRuntime` 析构中成员销毁顺序 = all_nodes_ 先于 scheduler_，executor 线程上的 in-flight `RunNode` 任务引用已释放 Node；且 Scheduler 成员销毁顺序 default_queue_ 先于 default_executor_，ThreadPool 析构会 drain 剩余任务 → 对已销毁队列 `RunNextTask`。
>      修复：`GraphRuntime::~GraphRuntime` 先 `scheduler_.reset()`（在节点存活时 join executor）；`Scheduler::~Scheduler` 先 drop executors（join worker）再销毁队列。
> - 验证：`fanout_graph_test` 7/7；`bazel test //...` 19/19；fanout 测试 8/8 轮×12 jobs 并行负载全绿；ASan 通过；scheduler_test DrainTest 在并行负载下的偶发 1% flake 属既有 encoder-flush 并发敏感，非 fan-out 回归（隔离运行 15/15）。

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: 收尾 —— 全量回归 + 跨仓库联动验证。

- [X] T023 [P] 全量回归：`bazel test //...`（graph_runtime 全部既有 + 新增 fanout 测试）全绿
- [X] T024 [P] 文档同步：更新 `docs/thread_safety.md`（如接线/镜像扇出涉线程模型说明）、`docs/project_bootstrap.md`（如能力描述）与 spec 结论一致
- [X] T025 外部下游工程联动验证：004 死锁场景（同一输出流被 recorder 与 push 两个下游节点消费）的端到端验证由用户在下游工程侧执行（本仓库侧已由 fanout_graph_test 证明该拓扑可正常完成）

**Checkpoint**: feature 完成；graph_runtime 侧全绿，下游工程联动由用户在外部验证。

> Phase 6 记录（2026-09-02）：
> - T023：`bazel test //...` → 19/19 全绿（18 既有 + fanout_graph/fanout_repro 新增；scheduler/stream_io 等单消费者注入无回归）。
> - T024：文档同步
>   - `docs/thread_safety.md` 新增 "Stream Fan-Out (1→N) & Done Semantics"：per-edge 输入队列、finalize-on-done（含无输出型消费者）、
>     图输入虚拟 source 注入、teardown 线程 join 顺序说明。
>   - `docs/project_bootstrap.md` 补充 sub-MediaPipe 定位与 fan-out/图输入扇出能力描述，与 spec 结论一致。
> - T025：graph_runtime 侧交付边界
>   - 本仓库为外部能力库（经本地路径被下游 Bazel 工程引用），改动无需在 graph_runtime 内额外打包。
>   - 004 死锁场景（同一 `es_packets` 输出流被 recorder 与 push 两个下游节点消费）的**最终端到端验证**由用户在下游工程侧执行
>     （按 spec SC-004/SC-007）；graph_runtime 侧已由
>     `fanout_graph_test`（T009 1→N 双消费者、T014 图输入→N）证明该拓扑可正常完成。

---

## Dependencies & Execution Order

### Phase Dependencies

- Setup（Phase 1）：无依赖，可立即开始（T001 红基线先行）
- Foundational（Phase 2）：依赖 Phase 1 复现；T003–T008 顺序/并行推进，**T008 是本 feature 能否成立的关口**
- US1（Phase 3）：依赖 Phase 2；MVP
- US2（Phase 4）：依赖 US1（图输入虚拟化叠加在 per-edge 基座上）
- US3（Phase 5）：依赖 US1+US2 的正常/错误完成语义
- Polish（Phase 6）：依赖全部

### Within Each User Story

- Tests（write-first）先于实现；实现依据 data-model/contracts。
- US1：T003–T006（接线）→ T012/T013（语义复核）。
- US2：T016/T017（图输入虚拟化）→ T018（计数复核）。

### Parallel Opportunities

- Phase 1：T001、T002 并行。
- Phase 2：T004、T005、T007 并行（不同接线段/文件）；T003、T006 顺序依赖各自接线段。
- Phase 3/4 测试：各 US 的测试 task 先写红，可并行。

---

## Implementation Strategy

### MVP First（US1 Only）

1. Phase 1：T001–T002（红基线）
2. Phase 2：T003–T008 —— 1→N fan-out 核心成立
3. **STOP and VALIDATE**：`bazel test //src/tests:all`

### Incremental Delivery

1. Phase 2 → 输入侧 per-edge 队列完成，单消费者无回归
2. US1 → 1→N fan-out（MVP），独立验证
3. US2 → 图输入扇出（数据模型统一），独立验证
4. US3 → fan-out 完成/错误语义，独立验证
5. Polish → 全量回归 + 外部下游工程 004 死锁联动验证（用户侧执行）

## Notes

- 所有 `src/...` 路径相对 Bazel workspace 根 `graph_runtime/graph_runtime/`。
- 不新增仓库；不改公共 API 签名（语义向后兼容扩展）。
- 环/back-edge、子图不在本期。
- 性能目标：扇出 = 写 1 + copy N-1 + move 1，无额外全量拷贝/全局锁。
