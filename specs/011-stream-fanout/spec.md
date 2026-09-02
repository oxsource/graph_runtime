# Feature Specification: 流扇出支持（Stream Fan-Out / Multi-Consumer）

**Feature Branch**: `011-stream-fanout`

**Created**: 2026-09-02

**Status**: Draft

**Input**: media_record 004（streaming-push）需要 encoder 的一路 `es_packets` 输出同时被 Recorder 与 StreamPushNode 两个下游节点消费；graph_runtime 当前接线在此场景下死锁/永不完成。参照 MediaPipe（`/Users/moks/Develop/docker/ubuntu24/codes/mediapipe`）分析其 fan-out 实现后，按方案 B（完整 MediaPipe 对齐）立项。

## 背景与根因（Clarifications / Root Cause）

### Q1: 单输出 → 多消费者当前为什么挂？

graph_runtime 的 `InputStreamManager` 按**完整 `"port:stream"` 字符串全图去重**（`graph_runtime/graph_runtime/src/framework/public/graph_runtime.cc` `GraphRuntime::Initialize`，约 70-83 行）：

```cpp
for (const auto& ndef : config_.nodes) {
  for (const auto& input_stream : ndef.input_streams) {
    if (stream_managers_.find(input_stream) != stream_managers_.end()) continue;
    ...
    node->SetInputPort(PortName(input_stream), raw);   // 只注册给第一个声明节点
    stream_managers_[input_stream] = raw;
  }
}
```

002 时代每路流只有一个消费者（encoder→recorder→muxer 线性链），此去重无副作用。004 起 recorder 与 push **都声明 `input:es_packets`**：manager 只建一次并注册给 recorder；push 拿不到自己的输入队列 → `input_port_count()==0` → 调度器把 push 当作 source 永转（永不 `StatusStop`）→ 图永不完成。最小复现（`IntSource → s1` + `s2` 同名流）稳定复现 30s 超时；单消费者变体正常跑完。

### Q2: MediaPipe 是怎么实现的？

- **输入侧按"消费边"各建独立队列**：`input_stream_managers_` 是按 `InputStreamInfos().size()` 铺平的一维数组（`mediapipe/framework/calculator_graph.cc` `InitializeStreams`），同一路输出流被 N 个节点消费 → N 条 input edge → N 个独立 `InputStreamManager`。
- **扇出发生在输出侧**：生产者的 `OutputStreamManager` 持 `mirrors_ = [(consumer InputStreamHandler, 该节点输入 id), ...]`；`AddMirror` 对每个消费边注册一次（`mediapipe/framework/calculator_node.cc` `InitializeInputStreams`，通过 validated graph 的 `upstream` 索引解析到源输出 manager）。生产者 PostProcess 时 `PropagateUpdatesToMirrors` **copy 给前 N-1 个 mirror、move 给最后一个**（`mediapipe/framework/output_stream_manager.cc`）。
- 每个消费者的 `InputStreamHandler` 只持有它自己的那串 manager，`mirror.id` 即该消费者的输入序号，路由到它自己的队列。

一句话：**MediaPipe = 输入侧 per-consumer-edge 独立队列 + 输出侧单 OutputStreamManager 对 N 个 mirror 扇出**。graph_runtime 输出侧的 `OutputStreamManager::PropagateUpdatesToMirrors` 已经逐行等价于 MediaPipe；错在输入侧按流名共享一个队列、把多消费者折叠成一个。

### Q3: 方案 A vs 方案 B

- **方案 A**：只把输入 manager 从"按流名全图去重"改为"按（消费方节点, 输入流）各建一个"，复用现有输出侧 N-mirror 扇出。改动面最小（集中在 `GraphRuntime::Initialize`），跨节点 1→N fan-out、N→1 多输入、菱形 DAG 均可用；性能与 MediaPipe 等价（写一次 + copy N-1 + move 1）。**局限**：外部图输入（`AddPacketToInputStream`）被多个节点消费时仍按"单消费者注册"，无扇出。
- **方案 B**（本 feature 采纳）：完整对齐 MediaPipe 数据模型 —— 不仅输入侧 per-edge，还把**图输入（Graph Input Stream）建模为虚拟 source 的 OutputStreamManager**（`AddPacketToInputStream` 写入该虚拟输出 manager，经 mirror 扇出到所有消费者），`CloseInputStream`/图完成计数同样走此路径。graph_runtime 的定位本来就是"sub-MediaPipe"（见 `docs/project_bootstrap.md`），统一数据模型避免后续在 A 的基座上返工。

## User Scenarios & Testing

### User Story 1 — 单输出 → 多消费者（1→N fan-out）(Priority: P1)

图运行库用户让一个生产节点的一路输出流被多个下游节点独立消费（例如同一路编码输出同时进本地录制节点与推流节点），每个下游各自收到完整数据流，且图在数据预算用尽后正常完成（不被误判为永不结束的 source）。

**Why this priority**: 这是本 feature 的核心价值；media_record 004 record+push 的前提，也是任意 DAG 图的基础能力。

**Independent Test**: 构造 `IntSource(emit_count=5) → SinkA` + `SinkB`（A、B 都消费同一路流名），运行 async runtime，断言两路各收到 5 帧、图 `WaitUntilDone()` 正常返回、无超时。

**Acceptance Scenarios**:

1. **Given** 一个 source 输出流被两个 sink 同时消费，**When** 图运行，**Then** 两个 sink 各自完整收到所有包（无丢包、无互相抢队）
2. **Given** 两个消费者之一还继续往第三节点产出（菱形 DAG），**When** 图运行，**Then** 所有分支都完成且不误判 source
3. **Given** 一个节点有多个输入（N→1，来自不同生产者），**When** 图运行，**Then** 多输入按既有 handler 语义正确同步/路由

### User Story 2 — 图输入（外部注入）扇出到多消费者 (Priority: P1)

图运行库用户经 `AddPacketToInputStream("stream", pkt)` 注入一路外部流，该流被多个下游节点消费（图输入 = 虚拟 source，MediaPipe GraphInputStream 语义），每个下游各自收到注入数据的完整拷贝；`CloseInputStream` 后各下游正常 finalize，图完成计数正确。

**Why this priority**: 方案 B 与 MediaPipe 对齐的关键差异点——把图输入建模成虚拟输出 manager，杜绝"A 只按单消费者注册"的局限，使注入路径与内部节点路径同构。

**Independent Test**: 构造一个图输入流喂两个 sink 的图，`Start()` 后对同一注入流 `AddPacketToInputStream` N 次再 `CloseInputStream`，断言两 sink 各收 N 包且图 `WaitUntilDone()` 完成（原 `scheduler_test`/`stream_io_test` 的"单消费者注入"场景保持通过）。

**Acceptance Scenarios**:

1. **Given** 一个图输入流被两个节点消费，**When** `AddPacketToInputStream` 注入 N 包并 `CloseInputStream`，**Then** 两个消费者各收到 N 包且图完成
2. **Given** 图输入只被一个节点消费（现有场景），**When** 注入/关闭，**Then** 行为与改动前一致（无回归）
3. **Given** 向未知/未声明流注入，**Then** 返回 `NotFoundError`（保持既有语义）

### User Story 3 — Done/完成语义与错误路径在 fan-out 下正确 (Priority: P2)

生产者在 fan-out 场景下关闭输出时，Done 广播到所有消费者；每个消费者独立 finalize；单消费者失败（如 push 节点 Open/Process 报错）不影响其它分支的录制完成（与 media_record FR-006 对应，graph_runtime 只负责图级语义）。

**Why this priority**: fan-out 会放大 completion/error 语义，需在框架层保证每分支独立、图级错误可定位。

**Independent Test**: 双消费者图中让其一在 Process 报错（error callback 捕获带节点名错误），另一分支正常收满；断言错误含报错节点名、健康分支仍完成到其输入 done。

**Acceptance Scenarios**:

1. **Given** fan-out 双消费者且其一报错，**When** 图运行，**Then** error callback 收到带该节点名的错误，另一分支输入到达 done
2. **Given** 生产者输出 Close，**When** 传播，**Then** 每个消费者输入流都观察到 done 并各自 finalize
3. **Given** 空图 / 无消费者输出流，**Then** 不崩溃（保持既有行为）

### Edge Cases

- 两个消费者共享同一完整 `"port:stream"` 名（`input:es_packets`）——本 feature 的核心场景，per-edge manager 消除去重冲突。
- 同一输出流被 N>2 个节点消费：mirror copy N-1 + move 1，各收一份。
- 一个节点声明多个输入且其中两个来自同一输出流（重复边）：按既有 handler 语义，重复声明由 validator 拒绝或按多输入处理——需在 validator 明确（对齐 MediaPipe：同一节点不得以同名消费同一流两次）。
- fan-out + 菱形汇聚（1→2→1）：每层 mirror/多输入独立成立。
- 图输入注入到未声明流：`NotFoundError`。
- 输出流无任何消费者：`PropagateUpdatesToMirrors` 空 mirror 时仅清空 shard（保持现有行为）。
- 生产者与消费者跨不同 executor：fan-out 写路径经 per-mirror 队列 + 线程安全 manager，不额外加锁热点。

## Requirements

### Functional Requirements

- **FR-001**: graph_runtime MUST 允许一路输出流被任意多个消费者节点消费（1→N），每个消费者收到该流的完整独立拷贝（不共享同一输入队列、不互相抢队）。
- **FR-002**: 输入侧数据模型 MUST 对齐 MediaPipe：每个（消费者节点, 输入边）各有一个独立 `InputStreamManager`；输入边不再按完整 `"port:stream"` 名做全图去重。
- **FR-003**: 输出侧扇出 MUST 复用/保持 `OutputStreamManager::AddMirror` + `PropagateUpdatesToMirrors`（copy 前 N-1、move 最后）既有语义；多消费者各经自己的 `InputStreamHandler` 收进自己的队列。
- **FR-004**: 接线阶段 MUST 把同一输出流的每个消费者（含共享同一完整流名者）都正确挂 mirror，并让每个消费者的 arrival/queue-size 回调绑定到它自己的 manager（不能被后声明者覆盖）。
- **FR-005**: 图输入（Graph Input Stream）MUST 建模为虚拟 source 的 `OutputStreamManager`：`AddPacketToInputStream` 注入到该虚拟输出 manager，经 mirror 扇出到所有声明消费它的节点；`CloseInputStream` 触发各消费者 finalize。
- **FR-006**: 图完成计数 MUST 与 MediaPipe 一致：仅图输入流（`config.input_streams`）计入 `num_open_input_streams_`；node-to-node 流由生产者驱动，不单独计入（既有语义保持）。
- **FR-007**: fan-out 下生产者输出 `Close` 时，Done MUST 广播到全部消费者（每 mirror 的输入流 done），各消费者独立 finalize；完成判定（每非 source 节点输入 done+empty）在 fan-out 后仍成立。
- **FR-008**: 任何单一消费者节点 Process 错误 MUST 经既有 error 路径上报（带节点名），不使其它健康分支在语义上被吞掉（图级错误传播语义保持）。
- **FR-009**: `AddPacketToInputStream`/`CloseInputStream` 对未知流 MUST 仍返回 `NotFoundError`；对"仅单消费者图输入"的既有场景 MUST 不回归。
- **FR-010**: ConfigValidator MUST 在 fan-out 语义下保持一致校验（重复输出名、环检测等既有规则不变；重复同名消费由实现/文档明确）。
- **FR-011**: 输出流无消费者时，`PropagateUpdatesToMirrors` 空 mirror 行为 MUST 保持（仅清 shard，不崩溃）。

### Key Entities

- **InputStreamManager（per input edge）**: 每个（消费者节点, 输入边）独立的数据队列；节点经 `SetInputPort` 把自己的输入端口指向它；承载 arrival/queue-size/done 状态。
- **OutputStreamManager（per output stream）**: 生产者的输出流对象，持 `mirrors_`（消费者 handler + 输入 id 列表）；`AddMirror` 每消费边一次；PostProcess 经 `PropagateUpdatesToMirrors` 扇出。
- **InputStreamHandler（per node）**: 消费节点唯一，持该节点自身的 manager 列表（顺序 = 输入声明顺序），按 `id` 路由 mirror 写。
- **GraphInputStream（虚拟 source / 图输入）**: 方案 B 新增——为 `config.input_streams` 中每条图输入创建虚拟输出 manager；`AddPacketToInputStream` 注入它，`CloseInputStream` 关闭它，mirror 扇出到所有消费者。对齐 MediaPipe `CalculatorGraph::GraphInputStream`。
- **input edge / "port:stream" 接线**: 消费者 `input_streams[i]` 以 StreamName（冒号后段）解析到生产者输出 manager；mirror id = 消费者输入下标。

## Success Criteria

### Measurable Outcomes

- **SC-001**: 同一 source 输出流被 2 个 sink 消费的 async 图，两路各收满 `emit_count` 包且 `WaitUntilDone()` 正常返回（最小复现从超时 → 通过）。
- **SC-002**: 图输入流（`config.input_streams`）喂 2 个消费者的图：`AddPacketToInputStream` N 次 + `CloseInputStream` 后两路各收 N 包且图完成。
- **SC-003**: 既有测试全绿：`scheduler_test`、`stream_io_test`、`input_chain_test`、`output_chain_test`、`integration_test`、`pipeline_*` 等（graph_runtime 全量 `bazel test //...`）无回归。
- **SC-004**: media_record 004 端到端（`stream_push_e2e_test` 短预算，MediaMTX 缺省时 record 侧完成 + push 侧降级不挂）通过。
- **SC-005**: fan-out 写路径性能与 MediaPipe 对齐：每包 1 次输出写 + N-1 copy + 1 move，无额外全量拷贝/全局锁热点。
- **SC-006**: 数据模型对齐后，图输入与内部 node-to-node 边共用同一套 mirror/manager 机制，代码无"A 独有特判"。

## Assumptions

- 对齐对象为本地 MediaPipe 源码 `/Users/moks/Develop/docker/ubuntu24/codes/mediapipe`（validated graph / calculator / stream manager / mirror 机制）。
- graph_runtime 定位为 sub-MediaPipe（`docs/project_bootstrap.md`），本 feature 做数据模型与接线对齐，不引入 MediaPipe 的 proto/config 体系。
- 消费节点内输入流名唯一（同节点不得以同名消费同一输出流两次），重复由 validator/文档约束。
- 图完成语义保持既有：仅 `config.input_streams` 计入完成计数；node-to-node 由生产者驱动。
- fan-out 后的 per-consumer 独立队列与 MediaPipe 一致，因此 1→N 的性能开销与 MediaPipe 相同（N-1 copy + 1 move）。
- 输出流允许无消费者；空 mirror 时传播仅清 shard。
- 单消费者全部既有路径（同步 `Schedule()` 与异步 `Start()`）行为不变。
