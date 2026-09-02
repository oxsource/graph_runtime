# Execution Contract: 流扇出支持（Stream Fan-Out / Multi-Consumer）

## 拓扑

fan-out 允许的图（示例）：

```
            ┌─→ SinkA（独立队列）
Source ──→（一路输出流）
            └─→ SinkB（独立队列）

Producer ──→ Consumer1 ──→ Consumer2   （菱形汇聚：Consumer2 多输入）

config.input_streams: [ "graph_in" ]
             ┌─→ SinkA
(注入 graph_in)──  （虚拟 source 扇出）
             └─→ SinkB
```

## 执行语义

### 数据面

- 每（消费者节点, 输入边）一个独立 `InputStreamManager`；生产者输出写一次，mirror copy 前 N-1 + move 最后，各消费者收全量。
- 各消费者独立调度（arrival 回调绑定各自 manager/节点），不互相抢队。

### 完成面

- 完成计数仅计 `config.input_streams`（图输入）。
- 生产者输出 `Close` → Done 广播全部 mirror → 各消费者 finalize；完成判定（每非 source 节点输入 done+empty）逐消费者成立。
- 图输入：`AddPacketToInputStream` 注入虚拟输出 manager → mirror 扇出；`CloseInputStream` → 关闭虚拟 manager → Done 广播。

### 错误面

- 消费者 Process 错误经既有 error 回调上报（带节点名）；不使其它分支语义被吞（图级传播保持）。
- fan-out 下任一消费者失败不影响其它分支在框架层的完成判定。

## 验证路径

- 最小复现（1 source → 2 sink 同流名）从超时 → `WaitUntilDone()` 正常返回、两路各收满。
- 图输入注入到 2 消费者：`AddPacketToInputStream` N 次 + `CloseInputStream` → 两路各收 N、图完成。
- 单消费者全部既有场景（同步/异步、注入/关闭、错误定位）无回归。
