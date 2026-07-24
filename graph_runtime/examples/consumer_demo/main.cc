#include "graph_runtime/graph_runtime.h"

int main() {
  graph::runtime::Logger::Info("graphrt::demo", "Graph Runtime consumer demo");

  auto ts = graph::runtime::Timestamp(100);
  graph::runtime::Logger::Info("graphrt::demo", ts.DebugString().c_str());

  auto pkt = graph::runtime::Packet::MakePacket<int>(42);
  graph::runtime::Logger::Info("graphrt::demo", "Packet created");

  graph::runtime::GraphConfig config;
  config.nodes.push_back({"test", "Dummy", {}, {}, {}, {}, {}, "", "", 1, 0});
  graph::runtime::Logger::Info("graphrt::demo", "GraphConfig created");

  return 0;
}
