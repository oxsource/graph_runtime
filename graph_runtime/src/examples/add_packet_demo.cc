// add_packet_demo.cc
// Demonstrates AddPacketToInputStream: feed packets to a running graph.
//
// Build: bazel build //src/examples:add_packet_demo
// Run:   bazel run //src/examples:add_packet_demo

#include "src/framework/utils/logger.h"
#include "src/framework/public/graph_runtime.h"
#include "src/framework/node/node.h"
#include "src/framework/node/node_contract.h"
#include "src/framework/node/node_registry.h"
#include "src/framework/node/graph_context.h"

namespace graph::runtime {

class DemoConsumer : public Node {
 public:
  DemoConsumer(const std::string& n, const NodeOptions&) : Node(n) {}
  static absl::Status GetContract(NodeContract* c) {
    c->Inputs().Get("input").Set<std::string>();
    return absl::OkStatus();
  }
  absl::Status Open(GraphContext&) override { return absl::OkStatus(); }
  absl::Status Process(GraphContext& ctx) override { return absl::OkStatus(); }
  absl::Status Close(GraphContext&) override { return absl::OkStatus(); }
};

}  // namespace graph::runtime

namespace { using DemoConsumer = graph::runtime::DemoConsumer; }
GRAPH_RUNTIME_REGISTER_NODE("DemoConsumer", DemoConsumer);

int main() {
  using namespace graph::runtime;

  GraphConfig config;
  config.nodes.push_back({"consumer", "DemoConsumer", {"input"}, {}, {}, {}, {}, "", "", 0, 0});

  GraphRuntime runtime;
  auto status = runtime.Initialize(config);
  if (!status.ok()) {
    Logger::Error((std::string("Init: ") + std::string(status.ToString())).c_str());
    return 1;
  }

  status = runtime.Start();
  if (!status.ok()) {
    Logger::Error((std::string("Start: ") + std::string(status.ToString())).c_str());
    return 1;
  }

  auto pkt = Packet::MakePacket<std::string>("hello");
  status = runtime.AddPacketToInputStream("input", pkt);
  Logger::Info(status.ok() ? "AddPacket OK" : std::string(status.ToString()).c_str());

  (void)runtime.CloseInputStream("input");
  runtime.Shutdown();
  Logger::Info("Demo done");
  return 0;
}
