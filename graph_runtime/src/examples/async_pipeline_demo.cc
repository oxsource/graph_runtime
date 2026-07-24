// async_pipeline_demo.cc
// Demonstrates async packet injection: a producer thread feeds packets
// while the main thread waits for completion.
//
// Build:  bazel build //src/examples:async_pipeline_demo
// Run:    bazel run //src/examples:async_pipeline_demo

#include <chrono>
#include <thread>

#include "src/framework/utils/logger.h"
#include "src/framework/public/graph_runtime.h"
#include "src/framework/node/node.h"
#include "src/framework/node/node_contract.h"
#include "src/framework/node/node_registry.h"
#include "src/framework/node/graph_context.h"

namespace graph::runtime {

class Counter : public Node {
 public:
  Counter(const std::string& n, const NodeOptions&) : Node(n) {}
  static absl::Status GetContract(NodeContract* c) {
    c->Inputs().Get("in").Set<std::string>();
    return absl::OkStatus();
  }
  absl::Status Open(GraphContext&) override { return absl::OkStatus(); }
  absl::Status Process(GraphContext& ctx) override {
    ++count_;
    return absl::OkStatus();
  }
  absl::Status Close(GraphContext&) override {
    Logger::Info(std::string("[counter] Close, received=" + std::to_string(count_)).c_str());
    return absl::OkStatus();
  }
  int count_ = 0;
};

}  // namespace graph::runtime

namespace { using Counter = graph::runtime::Counter; }
GRAPH_RUNTIME_REGISTER_NODE("Counter", Counter);

int main() {
  using namespace graph::runtime;

  // Build a minimal graph config with one consumer node and one input stream.
  GraphConfig config;
  config.nodes.push_back({"counter", "Counter", {"in"}, {}, {}, {}, {}, "", "", 0, 0});

  Logger::Info("Pipeline starting — will inject 5 packets asynchronously");

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

  // Producer thread: inject packets with 200ms intervals
  std::thread producer([&runtime] {
    for (int i = 0; i < 5; ++i) {
      auto pkt = Packet::MakePacket<std::string>("data_" + std::to_string(i));
      auto st = runtime.AddPacketToInputStream("in", pkt);
      if (st.ok()) {
        Logger::Info(std::string("[producer] Sent packet #").append(std::to_string(i)).c_str());
      } else {
        Logger::Warn(std::string("[producer] " + std::string(st.ToString())).c_str());
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    (void)runtime.CloseInputStream("in");
    Logger::Info("[producer] Stream closed");
  });

  Logger::Info("Main thread waiting for graph to complete...");
  producer.join();

  status = runtime.WaitUntilDone();
  if (!status.ok()) {
    Logger::Error((std::string("WaitUntilDone: ") + std::string(status.ToString())).c_str());
    return 1;
  }
  Logger::Info("Done — all packets processed");
  return 0;
}
