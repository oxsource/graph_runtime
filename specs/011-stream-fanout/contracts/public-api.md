# Public API Contract: 流扇出支持（Stream Fan-Out / Multi-Consumer）

本 feature 不改动 graph_runtime 的**外部消费方 API 签名**，只改变其接线语义与内部数据模型。消费者代码（声明 `input_streams`/`output_streams`/`config.input_streams`，调用 `Start`/`Schedule`/`AddPacketToInputStream`/`CloseInputStream`/`WaitUntilDone`）保持不变。

## 保持不变的外部 API

```cpp
namespace graph::runtime {

// 图配置（节点 input/output 流声明、图输入流声明）
struct GraphConfig { ... };

// async 运行
absl::Status Start();
absl::Status WaitUntilDone();
absl::Status Schedule();                       // sync

// 图输入注入/关闭（图输入流 = 虚拟 source）
absl::Status AddPacketToInputStream(const std::string& stream_name, Packet packet);
absl::Status AddPacketToInputStream(const std::string& tag, int index, Packet packet);
absl::Status CloseInputStream(const std::string& stream_name);

// 错误回调（图级错误传播，含节点名）
void SetErrorCallback(ErrorCallback cb);
bool HasError() const;
}
```

## 语义变化（向后兼容的扩展）

- **多消费者**：一路输出流可被任意多个节点消费，各收全量独立拷贝。此前"同流名仅首消费者获队列"的隐含限制消除。
- **图输入扇出**：`config.input_streams` 中的流可被多个节点消费；`AddPacketToInputStream` 注入的数据经虚拟 source 扇出到所有消费者。单消费者注入（既有 `scheduler_test`/`stream_io_test`）行为不变。
- **未知流**：`AddPacketToInputStream`/`CloseInputStream` 对未声明流返回 `NotFoundError`（保持）。

## 不新增公共头 / 公共类型

图输入虚拟 source、per-edge manager 均为内部实现细节，不暴露到公共面。
