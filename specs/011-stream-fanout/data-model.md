# Data Model: 流扇出支持（Stream Fan-Out / Multi-Consumer）

## 1. 实体与关系

### 1.1 InputStreamManager（per input edge）

每个**（消费者节点, 输入边）**一个独立实例，承载该消费者自己的数据队列与流状态。

| 字段 | 类型 | 说明 |
|------|------|------|
| `name_` | string | 声明时的完整 `"port:stream"`（可同名出现在不同消费者，各自独立实例） |
| `queue_` | deque<Packet> | 本消费者独享队列 |
| `next_timestamp_bound_` | Timestamp | 下一允许时间戳上界 |
| `closed_` | bool | 是否已关闭 |
| `arrival_callback_` | function | 到包回调（调度本消费者，绑定到本 manager 所属节点） |

**约束**：同一输出流被 N 个消费者消费 → N 个独立 `InputStreamManager`，不共享、不互相抢队。

### 1.2 OutputStreamManager（per output stream，生产者侧）

生产者的输出流对象，持有消费者镜像列表；多消费者共用**同一个** OutputStreamManager 实例。

| 字段 | 类型 | 说明 |
|------|------|------|
| `name_` | string | 流名（StreamName，冒号后段，用于接线解析） |
| `mirrors_` | vector<Mirror{InputStreamHandler*, CollectionItemId}> | 每个消费边一个镜像；`id` = 该消费者的输入下标 |

**约束**：fan-out 时一个 OutputStreamManager 对 N 个 mirror 扇出。

### 1.3 InputStreamHandler（per node，消费者唯一）

| 字段 | 类型 | 说明 |
|------|------|------|
| `managers_` | vector<InputStreamManager*> | 本节点自身输入边对应的 manager 列表（顺序 = `input_streams` 声明顺序） |

**约束**：只持有本节点的 manager；`AddPacketsToStream(id)`/`MovePacketsToStream(id)` 按 `id` 路由到 `managers_[id]`。

### 1.4 GraphInputStream（图输入 = 虚拟 source 的 output manager）【方案 B 新增】

为 `config.input_streams` 中每条图输入创建的虚拟生产输出流。

| 字段 | 类型 | 说明 |
|------|------|------|
| `stream_name` | string | 图输入流名（注入方使用） |
| `output_stream_manager` | OutputStreamManager* | 虚拟输出 manager，mirror 到所有声明消费该流的节点 |
| `closed` | bool | `CloseInputStream` 后置位 |

**行为**：
- `AddPacketToInputStream(stream, pkt)` → 写入该虚拟 output manager（等价虚拟 source 的 PostProcess）→ `PropagateUpdatesToMirrors` 扇出到所有消费者各自队列。
- `CloseInputStream(stream)` → 关闭该虚拟 manager → Done 广播到全部 mirror → 各消费者 finalize。
- 完成计数（`num_open_input_streams_`）仅计 `config.input_streams`（既有语义保持）。

## 2. 接线模型

### 2.1 输入边解析（Initialize 接线）

以 `config_.nodes` 为单位遍历：为每个节点的每个 `input_streams[i]` 建**独立** `InputStreamManager`，`SetInputPort(PortName(input_streams[i]), mgr)` 到该节点自己；manager 列表按声明顺序存入该节点的 `InputStreamHandler`。**不再**按完整 `"port:stream"` 名做跨节点去重。

### 2.2 扇出接线（mirror）

- 建立 `output_by_stream`（StreamName → 生产者 OutputStreamManager）。
- 对每个消费节点输入边 `i`：按 `StreamName(input_streams[i])` 解析到生产者输出 manager，`AddMirror(consumer_handler, i)`。
- 同一输出流的所有消费者（含共享同一完整流名者）各挂一次 mirror；每个消费者 handler 指向自己的 manager 列表，`mirror.id` 命中 `managers_[id]` → 各收一份。

### 2.3 扇出写路径

`OutputStreamManager::PropagateUpdatesToMirrors`：`mirrors_` 前 N-1 个 `AddPackets`（copy）、最后一个 `MovePackets`；随后设各 mirror 的时间戳上界；清空 shard。

### 2.4 完成 / 关闭

- 生产者输出 `Close()` → 对每个 mirror `SetNextTimestampBound(id, Done)` → 各消费者输入流 done → 各自 finalize。
- 完成判定（每非 source 节点输入 done+empty）在 per-edge 队列下逐消费者成立。
- 图输入：`CloseInputStream` 走 1.4 的虚拟 manager 关闭路径。

## 3. 状态转换

```
消费者输入流：  EMPTY/等待 ←到达→ 有包(队列非空) → Process → … → Done(生产者/图输入关闭) → finalize
生产者输出流：  OPEN → 生产/PostProcess(镜像扇出) → Close → Done 广播全部 mirror
图输入流：      OPEN(虚拟source) ←AddPacket→ 镜像扇出 → CloseInputStream → Done 广播全部 mirror
```

## 4. 配置与校验

- `GraphConfig`：节点 `input_streams`/`output_streams` 语义不变；`config.input_streams`（图输入）语义不变（仅计完成）。
- `ConfigValidator`：既有规则（唯一节点名、唯一 executor 名、重复输出名校验、环检测）不变；确认 fan-out（同一输出流多消费者）合法通过。同节点内重复消费同一流名（同名两条输入边）由 validator/文档约束拒绝或视为错误。
