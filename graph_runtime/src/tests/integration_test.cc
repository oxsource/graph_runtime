#include "graph_runtime/graph_runtime.h"

#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "src/framework/config/graph_config.h"
#include "src/framework/stream/packet.h"

namespace graph::runtime {

class InputSidePacketTest : public ::testing::Test {
 protected:
  void SetUp() override {
    runtime_ = std::make_unique<GraphRuntime>();

    // Create a minimal config.
    config_.nodes.push_back(
        {"source", "SourceNode", {}, {}, {"out"}, {}, {}, "", "", 0, 0});
  }

  std::unique_ptr<GraphRuntime> runtime_;
  GraphConfig config_;
};

TEST_F(InputSidePacketTest, SetInputSidePacketStoresPacket) {
  auto packet = Packet::MakePacket<int>(42);
  auto status = runtime_->SetInputSidePacket("my_tag", std::move(packet));
  EXPECT_TRUE(status.ok());
}

TEST_F(InputSidePacketTest, SetInputSidePacketOverwritesPrevious) {
  auto packet1 = Packet::MakePacket<int>(1);
  auto packet2 = Packet::MakePacket<int>(2);

  EXPECT_TRUE(runtime_->SetInputSidePacket("tag", std::move(packet1)).ok());
  EXPECT_TRUE(runtime_->SetInputSidePacket("tag", std::move(packet2)).ok());
  // No crash, second overwrites first.
  SUCCEED();
}

TEST_F(InputSidePacketTest, SetInputSidePacketOnEmptyTag) {
  auto packet = Packet::MakePacket<int>(100);
  auto status = runtime_->SetInputSidePacket("", std::move(packet));
  EXPECT_TRUE(status.ok());
}

class OutputSidePacketCallbackTest : public ::testing::Test {
 protected:
  void SetUp() override {
    runtime_ = std::make_unique<GraphRuntime>();
  }

  std::unique_ptr<GraphRuntime> runtime_;
};

TEST_F(OutputSidePacketCallbackTest, SetCallbackStoresIt) {
  bool callback_called = false;
  runtime_->SetOutputSidePacketCallback("out_tag",
      [&callback_called](const Packet&) { callback_called = true; });

  // Callback should be stored.
  EXPECT_FALSE(callback_called);
}

TEST_F(OutputSidePacketCallbackTest, SetCallbackOverwritesPrevious) {
  int call_count = 0;
  runtime_->SetOutputSidePacketCallback("tag",
      [&call_count](const Packet&) { call_count = 1; });
  runtime_->SetOutputSidePacketCallback("tag",
      [&call_count](const Packet&) { call_count = 2; });

  // The second callback should have overwritten the first.
  EXPECT_EQ(call_count, 0);
}

}  // namespace graph::runtime
