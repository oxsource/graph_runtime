// interactive_pipeline_demo.cc
// Demonstrates callback-driven async interaction:
//   1. Graph config from JSON string (not file).
//   2. Start() + WaitUntilDone() async execution.
//   3. OutputStreamCallback drives the next packet injection.
//   4. After 5 rounds, Cancel() terminates the graph.
//
// Build: bazel build //src/examples:interactive_pipeline_demo
// Run:   bazel run //src/examples:interactive_pipeline_demo

#include <string>

#include "src/framework/utils/logger.h"
#include "src/framework/public/graph_runtime.h"
#include "src/framework/node/node.h"
#include "src/framework/node/node_contract.h"
#include "src/framework/node/node_registry.h"
#include "src/framework/node/graph_context.h"
#include "src/framework/config/json/json_parser.h"

namespace graph::runtime {

// EchoNode: reads a string from "input:text" and writes it to "output:text".
class EchoNode : public Node {
 public:
  EchoNode(const std::string& n, const NodeOptions&) : Node(n) {}
  static absl::Status GetContract(NodeContract* c) {
    c->Inputs().Get("input_stream").Set<std::string>();
    c->Outputs().Get("output_stream").Set<std::string>();
    return absl::OkStatus();
  }
  absl::Status Open(GraphContext&) override { return absl::OkStatus(); }
  absl::Status Process(GraphContext& ctx) override {
    auto input = ctx.Inputs().Get("input_stream");
    if (!input.IsEmpty()) {
      auto val = input.Get<std::string>();
      if (val.ok()) {
        Logger::Info(std::string("[echo] received: ").append(*val).c_str());
        ctx.Outputs().Get("output_stream").AddPacket(
            Packet::MakePacket<std::string>(*val));
      }
    } else {
      Logger::Warn("[echo] empty input — skipping");
    }
    return absl::OkStatus();
  }
  absl::Status Close(GraphContext&) override { return absl::OkStatus(); }
};

}  // namespace graph::runtime

namespace { using EchoNode = graph::runtime::EchoNode; }
GRAPH_RUNTIME_REGISTER_NODE("EchoNode", EchoNode);

int main() {
  using namespace graph::runtime;

  // Graph config from JSON string.
  const std::string config_json = R"({
    "input_streams": ["input_stream"],
    "nodes": [
      {
        "name": "echo",
        "type": "EchoNode",
        "input_streams": ["input_stream"],
        "output_streams": ["output_stream"]
      }
    ]
  })";

  JsonParser parser;
  auto config = parser.ParseFromString(config_json);
  if (!config.ok()) {
    Logger::Error((std::string("Parse: ") + std::string(config.status().ToString())).c_str());
    return 1;
  }

  Logger::Info("Interactive pipeline starting — callback-driven, 5 rounds");

  GraphRuntime runtime;
  auto status = runtime.Initialize(*config);
  if (!status.ok()) {
    Logger::Error((std::string("Init: ") + std::string(status.ToString())).c_str());
    return 1;
  }

  status = runtime.Start();
  if (!status.ok()) {
    Logger::Error((std::string("Start: ") + std::string(status.ToString())).c_str());
    return 1;
  }

  // Register output callback. Each callback triggers the next round or cancel.
  int round = 0;
  runtime.SetOutputStreamCallback("output_stream", [&](const Packet& pkt) {
    auto val = pkt.Get<std::string>();
    if (val.ok()) {
      Logger::Info(std::string("[callback] round ").append(std::to_string(round))
                       .append(" got: ").append(*val).c_str());
    } else {
      Logger::Warn("[callback] packet extraction failed");
    }
    ++round;
    if (round >= 5) {
      Logger::Info("[callback] 5 rounds done, cancelling graph");
      runtime.Cancel();
    } else {
      auto next = Packet::MakePacket<std::string>(
          "hello_" + std::to_string(round));
      auto st = runtime.AddPacketToInputStream("input_stream", std::move(next));
      if (!st.ok()) {
        Logger::Warn(std::string("[callback] inject error: " + std::string(st.ToString())).c_str());
      } else {
        Logger::Info(std::string("[callback] injected round ").append(std::to_string(round)).c_str());
      }
    }
  });

  // Inject first packet.
  Logger::Info("[main] injecting initial packet");
  auto st = runtime.AddPacketToInputStream("input_stream",
      Packet::MakePacket<std::string>("hello_0"));
  if (!st.ok()) {
    Logger::Error(std::string("Inject error: " + std::string(st.ToString())).c_str());
    return 1;
  }

  // Wait for completion (cancel or natural termination).
  status = runtime.WaitUntilDone();
  Logger::Info(status.ok() ? "WaitUntilDone OK" : status.ToString().c_str());
  Logger::Info("Interactive demo done");
  return 0;
}
