#include "gtest/gtest.h"
#include "graph_runtime/graph_runtime.h"

namespace graph::runtime {

TEST(ConsumerDemoTest, TimestampCreated) {
  Timestamp t(100);
  EXPECT_EQ(t.Value(), 100);
}

TEST(ConsumerDemoTest, PacketCreated) {
  auto p = Packet::MakePacket<int>(42);
  EXPECT_FALSE(p.IsEmpty());
  auto val = p.Get<int>();
  ASSERT_TRUE(val.ok());
  EXPECT_EQ(*val, 42);
}

TEST(ConsumerDemoTest, GraphConfigCreated) {
  GraphConfig config;
  config.nodes.push_back({"n", "T", {}, {}, {}, {}, {}, "", "", 1, 0});
  EXPECT_EQ(config.nodes.size(), 1);
  EXPECT_EQ(config.nodes[0].name, "n");
}

}  // namespace graph::runtime
