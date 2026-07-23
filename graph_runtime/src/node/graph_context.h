#ifndef GRAPH_RUNTIME_GRAPH_CONTEXT_H_
#define GRAPH_RUNTIME_GRAPH_CONTEXT_H_

#include <deque>
#include <map>
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "src/stream/input_stream.h"
#include "src/stream/output_stream_shard.h"
#include "src/stream/packet.h"
#include "src/node/node_options.h"
#include "src/public/types.h"
#include "src/public/side_packet.h"

namespace graph::runtime {

// --- InputStreamShard ---

class InputStreamShard : public InputStream {
 public:
  InputStreamShard() = default;
  InputStreamShard(std::string name, std::queue<Packet> packets, bool done)
      : name_(std::move(name)),
        packet_queue_(std::move(packets)),
        is_done_(done) {}

  const std::string& Name() const final { return name_; }
  const Packet& Value() const final {
    static const Packet empty;
    return packet_queue_.empty() ? empty : packet_queue_.front();
  }
  Packet& Value() final {
    static Packet empty;
    return packet_queue_.empty() ? empty : packet_queue_.front();
  }
  bool IsEmpty() const final { return packet_queue_.empty(); }
  bool IsDone() const final { return is_done_; }
  Packet Header() const final { return Packet(); }

  void PushPacket(Packet p) { packet_queue_.push(std::move(p)); }
  void SetDone(bool done) { is_done_ = done; }
  void Clear() { while (!packet_queue_.empty()) packet_queue_.pop(); }

 private:
  std::string name_;
  std::queue<Packet> packet_queue_;
  bool is_done_ = false;
};

// --- InputStreamShardSet ---

class InputStreamShardSet {
 public:
  InputStreamShard& Get(const std::string& port_name) {
    return shards_[port_name];
  }
  InputStreamShard& Get(const std::string& tag, int index) {
    return shards_[tag + ":" + std::to_string(index)];
  }
  InputStreamShard& Index(int i) {
    auto it = shards_.begin();
    std::advance(it, i);
    return it->second;
  }
  int NumEntries() const { return static_cast<int>(shards_.size()); }

  using iterator = std::map<std::string, InputStreamShard>::iterator;
  iterator begin() { return shards_.begin(); }
  iterator end() { return shards_.end(); }

 private:
  std::map<std::string, InputStreamShard> shards_;
  friend class GraphContext;
};

// --- OutputStreamShardSet ---

class OutputStreamShardSet {
 public:
  OutputStreamShard& Get(const std::string& port_name) {
    return shards_[port_name];
  }
  OutputStreamShard& Get(const std::string& tag, int index) {
    return shards_[tag + ":" + std::to_string(index)];
  }
  OutputStreamShard& Index(int i) {
    auto it = shards_.begin();
    std::advance(it, i);
    return it->second;
  }
  int NumEntries() const { return static_cast<int>(shards_.size()); }

  using iterator = std::map<std::string, OutputStreamShard>::iterator;
  iterator begin() { return shards_.begin(); }
  iterator end() { return shards_.end(); }

 private:
  std::map<std::string, OutputStreamShard> shards_;
  friend class GraphContext;
};

// --- GraphContext ---

class GraphContext {
 public:
  GraphContext(
      const std::string& node_name,
      int node_id,
      const std::string& calculator_type,
      Timestamp input_timestamp,
      InputStreamShardSet inputs,
      OutputStreamShardSet outputs,
      const NodeOptions* options)
      : node_name_(node_name),
        node_id_(node_id),
        calculator_type_(calculator_type),
        input_timestamp_(input_timestamp),
        inputs_(std::move(inputs)),
        outputs_(std::move(outputs)),
        options_(options) {}

  const std::string& NodeName() const { return node_name_; }
  int NodeId() const { return node_id_; }
  const std::string& CalculatorType() const { return calculator_type_; }
  Timestamp InputTimestamp() const { return input_timestamp_; }

  InputStreamShardSet& Inputs() { return inputs_; }
  const InputStreamShardSet& Inputs() const { return inputs_; }
  OutputStreamShardSet& Outputs() { return outputs_; }
  const OutputStreamShardSet& Outputs() const { return outputs_; }

  const PacketSet& InputSidePackets() const { return input_side_packets_; }
  OutputSidePacketSet& OutputSidePackets() { return output_side_packets_; }
  void SetInputSidePackets(const PacketSet& ps) { input_side_packets_ = ps; }

  const NodeOptions& Options() const { return *options_; }

  void SetOffset(TimestampDiff offset) {
    for (auto& kv : outputs_.shards_) {
      kv.second.SetOffset(offset);
    }
  }

 private:
  std::string node_name_;
  int node_id_;
  std::string calculator_type_;
  Timestamp input_timestamp_;
  InputStreamShardSet inputs_;
  OutputStreamShardSet outputs_;
  PacketSet input_side_packets_;
  OutputSidePacketSet output_side_packets_;
  const NodeOptions* options_;
};

// --- GraphContextManager ---

class GraphContextManager {
 public:
  void Initialize(
      const std::string& node_name, int node_id,
      const std::string& calculator_type,
      InputStreamShardSet inputs, OutputStreamShardSet outputs,
      const NodeOptions* options) {
    default_context_ = std::make_unique<GraphContext>(
        node_name, node_id, calculator_type,
        Timestamp::Unstarted(), std::move(inputs), std::move(outputs),
        options);
  }

  GraphContext* GetDefaultCalculatorContext() {
    return default_context_.get();
  }

  // Phase 2 stubs
  GraphContext* PrepareCalculatorContext(Timestamp input_timestamp) {
    return default_context_.get();
  }
  void RecycleCalculatorContext() {}
  void CleanupAfterRun() { default_context_.reset(); }

 private:
  std::unique_ptr<GraphContext> default_context_;
};

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_GRAPH_CONTEXT_H_
