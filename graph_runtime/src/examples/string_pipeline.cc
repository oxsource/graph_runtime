#include <cctype>
#include <iostream>
#include <string>
#include <vector>

#include "src/public/graph_runtime.h"
#include "src/public/graph_builder.h"
#include "src/public/types.h"
#include "src/node/node.h"
#include "src/node/node_contract.h"
#include "src/node/node_factory.h"
#include "src/node/node_registry.h"
#include "src/node/graph_context.h"

namespace graph::runtime {

// --- Producer: generates strings numbered 0..99 ---
class StringProducer : public Node {
 public:
  StringProducer(const std::string& name, const NodeOptions& options)
      : Node(name) {}

  static absl::Status GetContract(NodeContract* contract) {
    contract->Outputs().Get("output").Set<std::string>();
    return absl::OkStatus();
  }

  absl::Status Open(GraphContext& context) override {
    counter_ = 0;
    return absl::OkStatus();
  }

  absl::Status Process(GraphContext& context) override {
    if (counter_ >= 100) {
      return StatusStop();
    }
    auto packet = Packet::MakePacket<std::string>(
        "hello_" + std::to_string(counter_));
    packet = packet.At(Timestamp(counter_));
    context.Outputs().Get("output").AddPacket(std::move(packet));
    ++counter_;
    return absl::OkStatus();
  }

  absl::Status Close(GraphContext& context) override {
    return absl::OkStatus();
  }

 private:
  int counter_ = 0;
};

// --- Transformer: uppercases input strings ---
class StringUppercase : public Node {
 public:
  StringUppercase(const std::string& name, const NodeOptions& options)
      : Node(name) {}

  static absl::Status GetContract(NodeContract* contract) {
    contract->Inputs().Get("input").Set<std::string>();
    contract->Outputs().Get("output").Set<std::string>();
    return absl::OkStatus();
  }

  absl::Status Open(GraphContext& context) override {
    return absl::OkStatus();
  }

  absl::Status Process(GraphContext& context) override {
    auto& input = context.Inputs().Get("input");
    if (input.IsEmpty()) {
      return absl::OkStatus();
    }
    auto msg_result = input.Get<std::string>();
    if (!msg_result.ok()) return absl::OkStatus();
    const auto& msg = *msg_result;
    std::string upper;
    upper.reserve(msg.size());
    for (char c : msg) {
      upper += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    auto packet = Packet::MakePacket<std::string>(upper);
    packet = packet.At(context.InputTimestamp());
    context.Outputs().Get("output").AddPacket(std::move(packet));
    return absl::OkStatus();
  }

  absl::Status Close(GraphContext& context) override {
    return absl::OkStatus();
  }
};

// --- Consumer: collects and prints output strings ---
class StringConsumer : public Node {
 public:
  StringConsumer(const std::string& name, const NodeOptions& options)
      : Node(name) {}

  static absl::Status GetContract(NodeContract* contract) {
    contract->Inputs().Get("input").Set<std::string>();
    return absl::OkStatus();
  }

  absl::Status Open(GraphContext& context) override {
    return absl::OkStatus();
  }

  absl::Status Process(GraphContext& context) override {
    auto& input = context.Inputs().Get("input");
    if (input.IsEmpty()) {
      return absl::OkStatus();
    }
    auto msg_result = input.Get<std::string>();
    if (!msg_result.ok()) return absl::OkStatus();
    const auto& msg = *msg_result;
    result_.push_back(msg);
    return absl::OkStatus();
  }

  absl::Status Close(GraphContext& context) override {
    std::cout << "StringConsumer received " << result_.size() << " packets"
              << std::endl;
    for (const auto& s : result_) {
      std::cout << "  " << s << std::endl;
    }
    return absl::OkStatus();
  }

  const std::vector<std::string>& result() const { return result_; }

 private:
  std::vector<std::string> result_;
};

}  // namespace graph::runtime

int main() {
  using namespace graph::runtime;

  // Register node types
  GRAPH_RUNTIME_REGISTER_NODE("StringProducer", StringProducer);
  GRAPH_RUNTIME_REGISTER_NODE("StringUppercase", StringUppercase);
  GRAPH_RUNTIME_REGISTER_NODE("StringConsumer", StringConsumer);

  // Build graph config programmatically
  GraphConfig config;
  config.max_queue_size = 100;

  GraphConfig::NodeDef producer;
  producer.name = "producer";
  producer.type = "StringProducer";
  producer.output_streams = {"output:producer_to_upper"};
  config.nodes.push_back(std::move(producer));

  GraphConfig::NodeDef transformer;
  transformer.name = "transformer";
  transformer.type = "StringUppercase";
  transformer.input_streams = {"input:producer_to_upper"};
  transformer.output_streams = {"output:upper_to_consumer"};
  config.nodes.push_back(std::move(transformer));

  GraphConfig::NodeDef consumer;
  consumer.name = "consumer";
  consumer.type = "StringConsumer";
  consumer.input_streams = {"input:upper_to_consumer"};
  config.nodes.push_back(std::move(consumer));

  GraphConfig::StreamDef s1;
  s1.name = "producer_to_upper";
  s1.source_node = "producer";
  s1.source_port = "output";
  s1.dest_node = "transformer";
  s1.dest_port = "input";
  config.streams.push_back(std::move(s1));

  GraphConfig::StreamDef s2;
  s2.name = "upper_to_consumer";
  s2.source_node = "transformer";
  s2.source_port = "output";
  s2.dest_node = "consumer";
  s2.dest_port = "input";
  config.streams.push_back(std::move(s2));

  // Build and run
  auto runtime = GraphBuilder::Build(config);
  if (!runtime.ok()) {
    std::cerr << "Build failed: " << runtime.status() << std::endl;
    return 1;
  }

  auto status = (*runtime)->Start();
  if (!status.ok()) {
    std::cerr << "Start failed: " << status << std::endl;
    return 1;
  }

  status = (*runtime)->WaitUntilDone();
  if (!status.ok()) {
    std::cerr << "Execution failed: " << status << std::endl;
    return 1;
  }

  std::cout << "Pipeline completed successfully." << std::endl;
  return 0;
}
