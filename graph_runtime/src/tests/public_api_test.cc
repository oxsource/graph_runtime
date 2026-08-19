#include "graph_runtime/graph_runtime.h"

#include <string>

#include "gtest/gtest.h"

namespace graph::runtime {

TEST(PublicApiTest, UmbrellaHeaderCompiles) {
  EXPECT_TRUE(true);
}

TEST(PublicApiTest, TimestampTypeIsAccessible) {
  Timestamp ts(100);
  EXPECT_EQ(ts.Value(), 100);
  EXPECT_TRUE(ts.IsAllowedInStream());
}

TEST(PublicApiTest, PacketTypeIsAccessible) {
  auto pkt = Packet::MakePacket<int>(42);
  EXPECT_FALSE(pkt.IsEmpty());
  auto value = pkt.Get<int>();
  ASSERT_TRUE(value.ok());
  EXPECT_EQ(*value, 42);
}

TEST(PublicApiTest, GraphConfigTypeIsAccessible) {
  GraphConfig config;
  config.nodes.push_back({"n1", "A", {}, {}, {}, {}, {}, "", "", 1, 0});
  EXPECT_EQ(config.nodes.size(), 1);
  EXPECT_EQ(config.nodes[0].name, "n1");
}

TEST(PublicApiTest, SidePacketTypeIsAccessible) {
  PacketSet set;
  EXPECT_EQ(set.NumEntries(), 0);
  set.Set("key", Packet::MakePacket<int>(1));
  EXPECT_EQ(set.NumEntries(), 1);
  Packet retrieved = set.Get("key");
  EXPECT_FALSE(retrieved.IsEmpty());
}

TEST(PublicApiTest, TypesAreAccessible) {
  CollectionItemId id = 42;
  EXPECT_EQ(id, 42);
  absl::Status stop = StatusStop();
  EXPECT_TRUE(IsStopStatus(stop));
}

TEST(PublicApiTest, ExportMacroIsDefined) {
#ifdef GRAPH_RUNTIME_API
  EXPECT_TRUE(true);
#else
  FAIL() << "GRAPH_RUNTIME_API not defined";
#endif
}

TEST(PublicApiTest, GraphConfigGetNodeOptionWithDefaults) {
  GraphConfig config;
  config.nodes.push_back(
      {"n1", "StreamInputNode", {}, {}, {}, {}, {}, "", "", 1, 0});
  config.nodes[0].options.Set("fps", 30);
  config.nodes[0].options.Set("name", std::string("video"));
  // Second node of the same type sets a different key only.
  config.nodes.push_back(
      {"n2", "StreamInputNode", {}, {}, {}, {}, {}, "", "", 1, 0});
  config.nodes[1].options.Set("fps", 60);
  config.nodes.push_back(
      {"n3", "SinkNode", {}, {}, {}, {}, {}, "", "", 1, 0});

  // Typed reads from the first node of the type that has the key.
  EXPECT_EQ(config.GetNodeOption<int>("StreamInputNode", "fps"), 30);
  EXPECT_EQ(config.GetNodeOption<std::string>("StreamInputNode", "name"),
            "video");

  // Defaults: implicit T{} when the key is missing...
  EXPECT_EQ(config.GetNodeOption<int>("StreamInputNode", "missing"), 0);
  EXPECT_EQ(config.GetNodeOption<std::string>("StreamInputNode", "missing"),
            "");
  // ...or explicit fallback for unknown types / missing keys.
  EXPECT_EQ(config.GetNodeOption<int>("SinkNode", "fps"), 0);
  EXPECT_EQ(config.GetNodeOption<int>("UnknownType", "fps"), 0);
  EXPECT_EQ(config.GetNodeOption<int>("SinkNode", "fps", -1), -1);
}

}  // namespace graph::runtime
