#include "gtest/gtest.h"
#include "src/framework/stream/input_stream_manager.h"
#include "src/framework/stream/input_stream.h"
#include "src/framework/stream/packet.h"

namespace graph::runtime {

TEST(InputChainTest, InputStreamManagerAddAndPop) {
  InputStreamManager mgr("test_stream");
  bool notify = false;

  std::list<Packet> packets;
  packets.push_back(Packet::MakePacket<int>(1).At(Timestamp(100)));
  packets.push_back(Packet::MakePacket<int>(2).At(Timestamp(200)));

  auto status = mgr.AddPackets(packets, &notify);
  EXPECT_TRUE(status.ok());
  EXPECT_TRUE(notify);
  EXPECT_EQ(mgr.QueueSize(), 2);

  bool done = false;
  int dropped = -1;
  Packet p = mgr.PopPacketAtTimestamp(Timestamp(100), &dropped, &done);
  EXPECT_EQ(p.timestamp().Value(), 100);
  auto val = p.Get<int>();
  EXPECT_TRUE(val.ok());
  EXPECT_EQ(*val, 1);

  p = mgr.PopPacketAtTimestamp(Timestamp(200), &dropped, &done);
  EXPECT_EQ(p.timestamp().Value(), 200);
  val = p.Get<int>();
  EXPECT_TRUE(val.ok());
  EXPECT_EQ(*val, 2);
}

TEST(InputChainTest, InputStreamManagerCloseAndIsDone) {
  InputStreamManager mgr("test");
  EXPECT_FALSE(mgr.IsDone());

  mgr.Close();
  EXPECT_TRUE(mgr.IsDone());

  bool done = false;
  Packet p = mgr.PopQueueHead(&done);
  EXPECT_TRUE(p.IsEmpty());
}

TEST(InputChainTest, MinTimestampOrBound) {
  InputStreamManager mgr("test");
  bool empty = false;
  Timestamp ts = mgr.MinTimestampOrBound(&empty);
  EXPECT_TRUE(empty);

  auto p = Packet::MakePacket<int>(42).At(Timestamp(500));
  std::list<Packet> packets = {p};
  bool notify = false;
  mgr.AddPackets(packets, &notify);

  ts = mgr.MinTimestampOrBound(&empty);
  EXPECT_FALSE(empty);
  EXPECT_EQ(ts.Value(), 500);
}

TEST(InputChainTest, MovePackets) {
  InputStreamManager mgr("test");
  auto p = Packet::MakePacket<int>(99).At(Timestamp(300));
  std::list<Packet> packets;
  packets.push_back(std::move(p));

  bool notify = false;
  auto status = mgr.MovePackets(&packets, &notify);
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(mgr.QueueSize(), 1);
  EXPECT_TRUE(packets.empty());
}

}  // namespace graph::runtime
