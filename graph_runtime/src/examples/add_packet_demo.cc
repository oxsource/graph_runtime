// add_packet_demo.cc
// Demonstrates the async execution path (Start + WaitUntilDone):
//   1. Initialize graph with a consumer node.
//   2. Start() — begins async execution on a thread pool.
//   3. AddPacketToInputStream("input", pkt) — injects a packet.
//   4. CloseInputStream("input") — signals no more packets.
//   5. WaitUntilDone() — blocks until the graph reaches kTerminated.
//
// In the async path, Source nodes are scheduled automatically. For
// input-only graphs (no sources), the scheduler schedules nodes when
// packets arrive via AddedPacketToInputStream.
//
// The synchronous path (Schedule) is used for batch/static graphs:
//   - Runs entirely on the calling thread.
//   - Does NOT support AddPacketToInputStream.
//   - Returns only after all nodes finish.
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
  config.input_streams.push_back("input");
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
  status = runtime.WaitUntilDone();
  Logger::Info(status.ok() ? "WaitUntilDone OK" : std::string(status.ToString()).c_str());
  Logger::Info("Demo done");
  return 0;
}
