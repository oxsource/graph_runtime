# Research（Phase 0）：流扇出支持（Stream Fan-Out / Multi-Consumer）

## 1. 问题

media_record 004（streaming-push）需要 encoder 的 `es_packets` 输出同时被 Recorder（本地 MP4）与 StreamPushNode（WebRTC/WHIP 推流）两个下游节点消费。当前 graph_runtime 接线在此 1→N 场景下死锁/永不完成。

**已用最小复现证实**（media_record 临时测试，随后已删除）：
`IntSource(emit_count=5) → SinkA` + `SinkB`，A、B 均消费同一 `"input:x"` → async 图 30s 超时挂死；把 SinkB 去掉（单消费者）→ 正常跑完。

## 2. 根因：输入 manager 按流名全图去重

`graph_runtime/graph_runtime/src/framework/public/graph_runtime.cc` `GraphRuntime::Initialize`（约 66–83 行）：

```cpp
for (const auto& ndef : config_.nodes) {
  for (const auto& input_stream : ndef.input_streams) {
    if (stream_managers_.find(input_stream) != stream_managers_.end()) continue;  // ← 全图去重
    auto* node = FindNode(ndef.name);
    ...
    node->SetInputPort(PortName(input_stream), raw);   // 只注册给第一个声明节点
    stream_managers_[input_stream] = raw;
  }
}
```

- `stream_managers_` 以完整 `"port:stream"`（如 `"input:es_packets"`）为 key。
- recorder 先声明 → 建全图唯一 manager 并注册到 recorder；push 再声明同名 → `find` 命中 → `continue`，**push 没有输入 manager/输入端口**。
- push `input_port_count()==0` → 调度器把无输入端口的节点当 source（`scheduler.cc` / `scheduler_queue.cc` 的 source 判定）→ source 自调度永转、不 `StatusStop` → 图永不完成。

002 时代每路流单消费者（线性链），去重无副作用；004 是首个真 1→N 场景，暴露该缺陷。

## 3. MediaPipe 对照（/Users/moks/Develop/docker/ubuntu24/codes/mediapipe）

### 3.1 输入侧：per input edge 独立 manager

`mediapipe/framework/calculator_graph.cc` `InitializeStreams()`：

```cpp
input_stream_managers_ = std::make_unique<InputStreamManager[]>(
    validated_graph_->InputStreamInfos().size());      // 按"输入边总数"铺平
for (int index = 0; index < validated_graph_->InputStreamInfos().size(); ++index) {
  const EdgeInfo& edge_info = validated_graph_->InputStreamInfos()[index];
  ...input_stream_managers_[index].Initialize(edge_info.name, ...);
}
```

`InputStreamInfos()` 每条 = 一个（消费者节点, 输入边）。同一输出流被 N 个节点消费 → N 条输入边 → N 个独立 manager。每个 `CalculatorNode` 持有自己的那一段（`CalculatorNode::InitializeInputStreams`，`&input_stream_managers[InputStreamBaseIndex()]`）。

### 3.2 接线：生产者输出 manager 对每个消费边 AddMirror

`mediapipe/framework/calculator_node.cc` `InitializeInputStreams()`（约 315–335 行）：

```cpp
for (CollectionItemId id = ...BeginId(); id < ...EndId(); ++id) {
  int output_stream_index = validated_graph_->InputStreamInfos()[... + id.value()].upstream;
  OutputStreamManager* origin_output_stream_manager = &output_stream_managers[output_stream_index];
  origin_output_stream_manager->AddMirror(input_stream_handler_.get(), id);
}
```

`EdgeInfo.upstream` = validated graph 在构建期解析出的"该输入边源自哪条输出边"索引。同一生产者输出 manager 被多个消费边的 `AddMirror` 调用 → `mirrors_` 多个。

### 3.3 扇出写路径：copy N-1 + move 最后

`mediapipe/framework/output_stream_manager.cc` `PropagateUpdatesToMirrors()`：

```cpp
int mirror_count = mirrors_.size();
for (int idx = 0; idx < mirror_count; ++idx) {
  if (add_packets) {
    if (idx == mirror_count - 1) mirror.input_stream_handler->MovePackets(mirror.id, packets_to_propagate);
    else                        mirror.input_stream_handler->AddPackets(mirror.id, *packets_to_propagate);
  }
  if (set_bound) mirror.input_stream_handler->SetNextTimestampBound(mirror.id, next_timestamp_bound);
}
packets_to_propagate->clear();
```

### 3.4 图输入 = 虚拟 source 的 output manager

`mediapipe/framework/calculator_graph.cc` `InitializeStreams()`：

```cpp
graph_input_streams_[stream_name] = std::make_unique<GraphInputStream>(
    &output_stream_managers_[output_stream_index]);   // 图输入=一条输出边（虚拟source产出）
```

`AddPacketToInputStream` 注入到该 output manager，`CloseInputStream` 关闭它；下游消费者照常经 mirror 收。这样图输入与 node-to-node 边走同一套机制。

### 3.5 小结

| 维度 | MediaPipe | graph_runtime 现状 |
|---|---|---|
| 输入队列 | 每输入边独立 `InputStreamManager`（扁平数组） | 每完整 `"port:stream"` 名全图唯一、共享 |
| 扇出 | 生产者 `OutputStreamManager.mirrors_` 对每消费边 AddMirror | 已有同款 mirrors_（实现基本等价） |
| 写路径 | copy N-1 + move 最后 | 已有同款 `PropagateUpdatesToMirrors` |
| 图输入 | 虚拟 source 的 output manager → mirror | 直接按节点声明名指向节点输入 manager（单消费者特判） |
| 完成计数 | 仅图输入计入 graph input streams | 仅 `config.input_streams` 计入（已对齐） |

**结论**：graph_runtime 输出侧已基本等价 MediaPipe；缺陷集中在**输入侧 per-edge 队列缺失** + **图输入未虚拟 source 化**。这正是方案 B 要补的两块。

## 4. 方案对比

### 方案 A（仅输入侧 per-edge，最小改动）
- 输入 manager 改按（节点, 输入流）各建一个；arrival/queue-size 绑定各自节点；mirror 接线复用。
- 效果：跨节点 1→N fan-out、N→1、菱形 DAG 均可用；性能 = 写 1 + copy N-1 + move 1。
- 局限：图输入（`AddPacketToInputStream`）仍按单消费者注册，不做扇出；图输入与内部边两套语义并存。

### 方案 B（本 feature 采纳）
A 的全部 + **图输入建模为虚拟 source 的 OutputStreamManager**（`AddPacketToInputStream` 注入虚拟输出 manager → mirror 扇出 → 所有消费者各自收全量），`CloseInputStream` 触发各消费者 finalize。图输入与 node-to-node 边同构。

选 B 的理由：
- graph_runtime 定位就是 sub-MediaPipe（`docs/project_bootstrap.md`），统一数据模型符合项目方向。
- 避免"A 之后再返工"：后续若出现外部喂流 + 一喂多（如 audio 输入源同时进 recorder/push），A 的基座仍需补图输入虚拟化。
- 图输入扇出场景在 graph_runtime 自测中已有注入需求（`scheduler_test`/`stream_io_test` 的 AddPacketToInputStream），B 使其语义完整。
- 两者性能相同（写 1 + copy N-1 + move 1），B 无额外开销。

## 5. 采纳范围

- 输入侧 per-consumer-edge `InputStreamManager`（FR-001/002/003/004）。
- 图输入虚拟 source output manager + 注入/关闭扇出（FR-005/006/009）。
- Done/完成语义在 fan-out 下逐消费者正确（FR-007/008/011）。
- 环/back-edge、子图不在本期（后续另行评估，沿用既有 reject 环行为）。
