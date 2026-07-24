#include "gtest/gtest.h"
#include "src/framework/stream/output_stream_manager.h"
#include "src/framework/stream/output_stream_shard.h"
#include "src/framework/stream/output_stream_handler.h"
#include "src/framework/stream/packet.h"

namespace graph::runtime {

TEST(OutputChainTest, OutputStreamShardAddPacket) {
  OutputStreamSpec spec;
  spec.name = "test_port";
  OutputStreamShard shard(&spec);

  EXPECT_TRUE(shard.IsEmpty());
  auto p = Packet::MakePacket<int>(7).At(Timestamp(100));
  shard.AddPacket(p);
  EXPECT_FALSE(shard.IsEmpty());
  EXPECT_EQ(shard.LastAddedPacketTimestamp().Value(), 100);
}

TEST(OutputChainTest, OutputStreamShardClose) {
  OutputStreamShard shard;
  EXPECT_FALSE(shard.IsClosed());
  shard.Close();
  EXPECT_TRUE(shard.IsClosed());
}

TEST(OutputChainTest, OutputStreamManagerResetShard) {
  OutputStreamManager mgr("test_port");
  OutputStreamShard shard;

  mgr.ResetShard(&shard);
  EXPECT_TRUE(shard.IsEmpty());
  EXPECT_FALSE(shard.IsClosed());
}

TEST(OutputChainTest, OutputStreamManagerClose) {
  OutputStreamManager mgr("test_port");
  EXPECT_FALSE(mgr.IsClosed());
  mgr.Close();
  EXPECT_TRUE(mgr.IsClosed());
}

TEST(OutputChainTest, ComputeOutputTimestampBound) {
  OutputStreamManager mgr("test_port");
  OutputStreamShard shard;
  mgr.ResetShard(&shard);

  auto p = Packet::MakePacket<int>(42).At(Timestamp(1000));
  shard.AddPacket(p);

  Timestamp bound = mgr.ComputeOutputTimestampBound(shard, Timestamp(500));
  EXPECT_TRUE(bound.IsRangeValue());
}

TEST(OutputChainTest, PropagateUpdatesToMirrors) {
  OutputStreamManager mgr("test_port");
  OutputStreamShard shard;
  mgr.ResetShard(&shard);

  auto p = Packet::MakePacket<int>(42).At(Timestamp(100));
  shard.AddPacket(p);

  // Propagate without mirrors — should clear the shard
  mgr.PropagateUpdatesToMirrors(Timestamp(101), &shard);
  EXPECT_TRUE(shard.IsEmpty());
}

}  // namespace graph::runtime
