#ifndef GRAPH_RUNTIME_GRAPH_CONTEXT_H_
#define GRAPH_RUNTIME_GRAPH_CONTEXT_H_

#include <deque>
#include <map>
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "src/framework/stream/input_stream.h"
#include "src/framework/stream/output_stream_shard.h"
#include "src/framework/stream/packet.h"
#include "src/framework/node/node_options.h"
#include "src/framework/public/types.h"
#include "src/framework/public/side_packet.h"

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
      const std::string& node_type,
      Timestamp input_timestamp,
      InputStreamShardSet* inputs,
      OutputStreamShardSet* outputs,
      const NodeOptions* options)
      : node_name_(node_name),
        node_id_(node_id),
        node_type_(node_type),
        input_timestamp_(input_timestamp),
        inputs_ptr_(inputs),
        outputs_ptr_(outputs),
        options_(options) {}

  // Compatibility constructor — takes sets by value and stores pointers
  GraphContext(
      const std::string& node_name,
      int node_id,
      const std::string& node_type,
      Timestamp input_timestamp,
      InputStreamShardSet&& inputs,
      OutputStreamShardSet&& outputs,
      const NodeOptions* options)
      : node_name_(node_name),
        node_id_(node_id),
        node_type_(node_type),
        input_timestamp_(input_timestamp),
        owned_inputs_(std::move(inputs)),
        owned_outputs_(std::move(outputs)),
        inputs_ptr_(&owned_inputs_),
        outputs_ptr_(&owned_outputs_),
        options_(options) {}

  const std::string& NodeName() const { return node_name_; }
  int NodeId() const { return node_id_; }
  const std::string& NodeType() const { return node_type_; }
  Timestamp InputTimestamp() const { return input_timestamp_; }
  void SetInputTimestamp(Timestamp ts) { input_timestamp_ = ts; }

  InputStreamShardSet& Inputs() { return *inputs_ptr_; }
  const InputStreamShardSet& Inputs() const { return *inputs_ptr_; }
  OutputStreamShardSet& Outputs() { return *outputs_ptr_; }
  const OutputStreamShardSet& Outputs() const { return *outputs_ptr_; }

  const PacketSet& InputSidePackets() const { return input_side_packets_; }
  OutputSidePacketSet& OutputSidePackets() { return output_side_packets_; }
  void SetInputSidePackets(const PacketSet& ps) { input_side_packets_ = ps; }

  const NodeOptions& Options() const { return *options_; }

  void SetOffset(TimestampDiff offset) {
    for (auto& kv : outputs_ptr_->shards_) {
      kv.second.SetOffset(offset);
    }
  }

 private:
  std::string node_name_;
  int node_id_;
  std::string node_type_;
  Timestamp input_timestamp_;
  InputStreamShardSet owned_inputs_;
  OutputStreamShardSet owned_outputs_;
  InputStreamShardSet* inputs_ptr_;
  OutputStreamShardSet* outputs_ptr_;
  PacketSet input_side_packets_;
  OutputSidePacketSet output_side_packets_;
  const NodeOptions* options_;
};

// --- GraphContextManager ---

class GraphContextManager {
 public:
  void Initialize(
      const std::string& node_name, int node_id,
      const std::string& node_type,
      InputStreamShardSet inputs, OutputStreamShardSet outputs,
      const NodeOptions* options) {
    default_context_ = std::make_unique<GraphContext>(
        node_name, node_id, node_type,
        Timestamp::Unstarted(), std::move(inputs), std::move(outputs),
        options);
  }

  GraphContext* GetDefaultContext() {
    return default_context_.get();
  }

  // Returns a prepared context from the pool or creates a new one.
  GraphContext* PrepareContext(Timestamp input_timestamp) {
    if (pool_.empty()) {
      auto ctx = std::make_unique<GraphContext>(
          default_context_->NodeName(),
          default_context_->NodeId(),
          default_context_->NodeType(),
          input_timestamp,
          &default_context_->Inputs(),
          &default_context_->Outputs(),
          &default_context_->Options());
      owned_.push_back(std::move(ctx));
      return owned_.back().get();
    }
    auto ctx = std::move(pool_.back());
    pool_.pop_back();
    ctx->SetInputTimestamp(input_timestamp);
    owned_.push_back(std::move(ctx));
    return owned_.back().get();
  }

  // Returns a context to the pool for reuse.
  void RecycleContext(GraphContext* ctx) {
    for (auto it = owned_.begin(); it != owned_.end(); ++it) {
      if (it->get() == ctx) {
        pool_.push_back(std::move(*it));
        owned_.erase(it);
        break;
      }
    }
  }

  void CleanupAfterRun() {
    default_context_.reset();
    pool_.clear();
    owned_.clear();
  }

 private:
  std::unique_ptr<GraphContext> default_context_;
  std::vector<std::unique_ptr<GraphContext>> pool_;
  std::vector<std::unique_ptr<GraphContext>> owned_;
};

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_GRAPH_CONTEXT_H_
