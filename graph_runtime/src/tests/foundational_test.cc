#include "graph_runtime/src/stream/timestamp.h"
#include "graph_runtime/src/stream/packet.h"
#include "graph_runtime/src/node/node_options.h"
#include "graph_runtime/src/node/options_registry.h"
#include "graph_runtime/src/public/types.h"

#include "gtest/gtest.h"

namespace graph::runtime {

// --- Timestamp tests ---

TEST(TimestampTest, SpecialValues) {
  EXPECT_TRUE(Timestamp::Unset().IsEmpty());
  EXPECT_TRUE(Timestamp::Unset().IsSpecialValue());
  EXPECT_FALSE(Timestamp::Unset().IsRangeValue());
  EXPECT_FALSE(Timestamp::Unset().IsAllowedInStream());

  EXPECT_TRUE(Timestamp::Unstarted().IsSpecialValue());
  EXPECT_FALSE(Timestamp::Unstarted().IsRangeValue());

  EXPECT_TRUE(Timestamp::Min().IsSpecialValue());
  EXPECT_TRUE(Timestamp::Min().IsRangeValue());

  EXPECT_TRUE(Timestamp::Max().IsSpecialValue());
  EXPECT_TRUE(Timestamp::Max().IsRangeValue());

  EXPECT_TRUE(Timestamp::Done().IsSpecialValue());
  EXPECT_FALSE(Timestamp::Done().IsRangeValue());
  EXPECT_FALSE(Timestamp::Done().IsAllowedInStream());
}

TEST(TimestampTest, RegularTimestamp) {
  Timestamp t(1000000);
  EXPECT_EQ(t.Value(), 1000000);
  EXPECT_FALSE(t.IsSpecialValue());
  EXPECT_TRUE(t.IsRangeValue());
  EXPECT_TRUE(t.IsAllowedInStream());
  EXPECT_FALSE(t.IsEmpty());
  EXPECT_FALSE(t.DebugString().empty());
}

TEST(TimestampTest, NextAllowedInStream) {
  Timestamp t(100);
  EXPECT_EQ(t.NextAllowedInStream().Value(), 101);
}

TEST(TimestampTest, Comparison) {
  EXPECT_TRUE(Timestamp(1) < Timestamp(2));
  EXPECT_TRUE(Timestamp(1) == Timestamp(1));
  EXPECT_TRUE(Timestamp(2) != Timestamp(1));
}

// --- Packet tests ---

TEST(PacketTest, DefaultConstructedIsEmpty) {
  Packet p;
  EXPECT_TRUE(p.IsEmpty());
  EXPECT_TRUE(p.timestamp().IsEmpty());
}

TEST(PacketTest, MakePacket) {
  auto p = Packet::MakePacket<int>(42);
  EXPECT_FALSE(p.IsEmpty());
  auto result = p.Get<int>();
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(*result, 42);
}

TEST(PacketTest, Adopt) {
  auto p = Packet::Adopt(new int(99));
  EXPECT_FALSE(p.IsEmpty());
  auto result = p.Get<int>();
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(*result, 99);
}

TEST(PacketTest, CopySharesHolder) {
  auto p1 = Packet::MakePacket<int>(7);
  Packet p2 = p1;
  EXPECT_TRUE(p1 == p2);
  auto r1 = p1.Get<int>();
  auto r2 = p2.Get<int>();
  ASSERT_TRUE(r1.ok());
  ASSERT_TRUE(r2.ok());
  EXPECT_EQ(*r1, *r2);
}

TEST(PacketTest, MoveEmptiesSource) {
  auto p1 = Packet::MakePacket<int>(7);
  Packet p2 = std::move(p1);
  EXPECT_TRUE(p1.IsEmpty());
  EXPECT_FALSE(p2.IsEmpty());
  EXPECT_FALSE(p1 == p2);
}

TEST(PacketTest, AtTimestamp) {
  auto p = Packet::MakePacket<int>(1).At(Timestamp(500));
  EXPECT_EQ(p.timestamp().Value(), 500);
}

TEST(PacketTest, Validation) {
  auto p = Packet::MakePacket<double>(3.14);
  EXPECT_TRUE(p.ValidateAsType<double>().ok());
  EXPECT_FALSE(p.ValidateAsType<int>().ok());
}

TEST(PacketTest, GetTypeMismatch) {
  auto p = Packet::MakePacket<double>(2.5);
  auto result = p.Get<int>();
  EXPECT_FALSE(result.ok());
}

TEST(PacketTest, Share) {
  auto p = Packet::MakePacket<std::string>("hello");
  auto result = p.Share<std::string>();
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(**result, "hello");
}

TEST(PacketTest, DebugString) {
  auto p = Packet::MakePacket<int>(42).At(Timestamp(100));
  auto s = p.DebugString();
  EXPECT_FALSE(s.empty());
  EXPECT_TRUE(s.find("int") != std::string::npos);
}

TEST(PacketTest, EmptyPacketDebug) {
  Packet p;
  EXPECT_EQ(p.DebugTypeName(), "{empty}");
}

// --- NodeOptions tests ---

TEST(NodeOptionsTest, SetAndGet) {
  NodeOptions opts;
  opts.Set("threshold", 100);
  opts.Set("name", std::string("test"));

  const auto* t = opts.Get<int>("threshold");
  ASSERT_NE(t, nullptr);
  EXPECT_EQ(*t, 100);

  const auto* n = opts.Get<std::string>("name");
  ASSERT_NE(n, nullptr);
  EXPECT_EQ(*n, "test");
}

TEST(NodeOptionsTest, Has) {
  NodeOptions opts;
  opts.Set("key", 1);
  EXPECT_TRUE(opts.Has("key"));
  EXPECT_FALSE(opts.Has("missing"));
}

TEST(NodeOptionsTest, Keys) {
  NodeOptions opts;
  opts.Set("a", 1);
  opts.Set("b", 2);
  auto keys = opts.Keys();
  EXPECT_EQ(keys.size(), 2);
}

TEST(NodeOptionsTest, MissingKeyReturnsNull) {
  NodeOptions opts;
  EXPECT_EQ(opts.Get<int>("missing"), nullptr);
}

// --- Types tests ---

TEST(TypesTest, IsStopStatus) {
  EXPECT_TRUE(IsStopStatus(StatusStop()));
  EXPECT_FALSE(IsStopStatus(absl::OkStatus()));
  EXPECT_FALSE(IsStopStatus(absl::InternalError("err")));
}

TEST(TypesTest, StatusStop) {
  auto s = StatusStop();
  EXPECT_TRUE(IsStopStatus(s));
}

}  // namespace graph::runtime
