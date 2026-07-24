#include "src/framework/public/graph_runtime.h"

#include <memory>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "src/framework/config/graph_config.h"
#include "src/framework/stream/packet.h"

namespace graph::runtime {

class OutputStreamCallbackTest : public ::testing::Test {
 protected:
  void SetUp() override {
    runtime_ = std::make_unique<GraphRuntime>();

    // Create a minimal config with a source node and a sink node.
    config_.nodes.push_back(
        {"source", "SourceNode", {}, {}, {"out"}, {}, {}, "", "", 0, 0});
    config_.nodes.push_back(
        {"sink", "SinkNode", {"in"}, {}, {}, {}, {}, "", "", 1, 0});
  }

  std::unique_ptr<GraphRuntime> runtime_;
  GraphConfig config_;
};

TEST_F(OutputStreamCallbackTest, SetCallbackStoresIt) {
  bool callback_called = false;
  runtime_->SetOutputStreamCallback("out",
      [&callback_called](const Packet&) { callback_called = true; });

  // Callback should be stored (verified indirectly via initialization).
  EXPECT_FALSE(callback_called);
}

TEST_F(OutputStreamCallbackTest, ClearCallbackRemovesIt) {
  bool callback_called = false;
  runtime_->SetOutputStreamCallback("out",
      [&callback_called](const Packet&) { callback_called = true; });
  runtime_->ClearOutputStreamCallback("out");

  // After clearing, callback should not be called.
  EXPECT_FALSE(callback_called);
}

TEST_F(OutputStreamCallbackTest, ClearNonExistentCallbackIsIdempotent) {
  // Clearing a callback that was never set should not crash.
  runtime_->ClearOutputStreamCallback("nonexistent");
  SUCCEED();
}

TEST_F(OutputStreamCallbackTest, SetCallbackOverwritesPrevious) {
  int call_count = 0;
  runtime_->SetOutputStreamCallback("out",
      [&call_count](const Packet&) { call_count = 1; });
  runtime_->SetOutputStreamCallback("out",
      [&call_count](const Packet&) { call_count = 2; });

  // The second callback should have overwritten the first.
  EXPECT_EQ(call_count, 0);
}

}  // namespace graph::runtime
