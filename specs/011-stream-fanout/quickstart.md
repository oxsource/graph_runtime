# Quickstart: 流扇出支持（Stream Fan-Out / Multi-Consumer）

## 前置

- graph_runtime Bazel workspace：`/Users/moks/Develop/docker/ubuntu24/codes/graph_runtime/graph_runtime/`
- 对照参考（仅实现时查阅，非构建依赖）：`/Users/moks/Develop/docker/ubuntu24/codes/mediapipe`

## 验证

```bash
# 在 workspace 根（graph_runtime/graph_runtime/）执行
bazel test //...            # 全量回归（含新增 fanout 测试）
```

重点测试：
- `//src/tests:fanout_graph_test`（新增）—— 1→N、图输入→N、Done/error
- `//src/tests:scheduler_test`、`//src/tests:stream_io_test` 等（既有，确保无回归）

## 消费方用法（语义变化）

一个输出流被多个节点消费：

```json
{ "nodes": [
    { "name": "src",  "type": "...",  "output_streams": ["output:es"] },
    { "name": "a",    "type": "...",  "input_streams": ["input:es"] },
    { "name": "b",    "type": "...",  "input_streams": ["input:es"] }
] }
```

a、b 各自收到 es 的全量独立拷贝（此前第二个消费者会丢失输入被当 source）。

图输入喂多个节点（方案 B）：

```json
{ "input_streams": ["graph_in"],
  "nodes": [
    { "name": "a", "type": "...", "input_streams": ["input:graph_in"] },
    { "name": "b", "type": "...", "input_streams": ["input:graph_in"] }
  ] }
```

`AddPacketToInputStream("graph_in", pkt)` 注入 → 虚拟 source 扇出到 a、b。

## 验收路径

1. `bazel test //...` 全绿（SC-003）。
2. 新增 fan-out 单测证明 1→N 与图输入→N 各收满且图完成（SC-001/002）。
3. media_record 004 e2e（stream_push.json：recorder+push 同吃 es_packets）不再死锁（跨仓库验证，SC-004）。
