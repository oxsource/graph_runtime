#include <iostream>

#include "graph_runtime/graph_runtime.h"

int main() {
  std::cout << "Graph Runtime consumer demo" << std::endl;

  auto ts = graph::runtime::Timestamp(100);
  std::cout << "Timestamp created: " << ts.DebugString() << std::endl;

  auto pkt = graph::runtime::Packet::MakePacket<int>(42);
  std::cout << "Packet created, empty=" << pkt.IsEmpty() << std::endl;

  graph::runtime::GraphConfig config;
  config.nodes.push_back({"test", "Dummy", {}, {}, {}, {}, {}, "", "", 1, 0});
  std::cout << "GraphConfig created with " << config.nodes.size() << " nodes" << std::endl;

  return 0;
}
