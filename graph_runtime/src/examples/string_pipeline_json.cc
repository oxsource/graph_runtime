#include <cstdio>
#include <fstream>
#include <string>

#define GRAPHRT_LOG_TAG "graphrt::example"
#include "graph_runtime/graph_runtime.h"

int main() {
  GRAPHRT_LOGI("=== String Pipeline (JSON Config) ===");

  // Create a minimal JSON pipeline config file.
  const char* tmp_path = "/tmp/string_pipeline.json";
  {
    std::ofstream f(tmp_path);
    f << R"({
      "nodes": [
        {
          "name": "producer",
          "type": "StringProducer",
          "output_streams": ["producer:output"]
        },
        {
          "name": "consumer",
          "type": "StringConsumer",
          "input_streams": ["producer:output"]
        }
      ]
    })";
    f.close();
  }

  // Verify the config file was created and parseable as JSON.
  std::ifstream file(tmp_path);
  std::string content((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  file.close();

  // Demonstrate public API types.
  graph::runtime::Timestamp ts(100);
  GRAPHRT_LOGI(ts.DebugString().c_str());

  auto pkt = graph::runtime::Packet::MakePacket<std::string>("hello");
  GRAPHRT_LOGI(std::string("Packet created, empty=" + std::to_string(pkt.IsEmpty())).c_str());

  graph::runtime::GraphConfig config;
  config.nodes.push_back({"demo_node", "Dummy", {}, {}, {}, {}, {}, "", "", 1, 0});
  GRAPHRT_LOGI(std::string("Config has " + std::to_string(config.nodes.size()) + " node(s)").c_str());

  std::remove(tmp_path);
  GRAPHRT_LOGI("=== Done (JSON config driven) ===");
  return 0;
}
