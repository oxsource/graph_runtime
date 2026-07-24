#define GRAPHRT_LOG_TAG "graphrt::demo"
#include "graph_runtime/graph_runtime.h"

int main() {
  GRAPHRT_LOGI("Graph Runtime consumer demo");

  auto ts = graph::runtime::Timestamp(100);
  GRAPHRT_LOGI(ts.DebugString().c_str());

  auto pkt = graph::runtime::Packet::MakePacket<int>(42);
  GRAPHRT_LOGI("Packet created");

  graph::runtime::GraphConfig config;
  config.nodes.push_back({"test", "Dummy", {}, {}, {}, {}, {}, "", "", 1, 0});
  GRAPHRT_LOGI("GraphConfig created");

  return 0;
}
