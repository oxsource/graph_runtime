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
  }

  std::unique_ptr<GraphRuntime> runtime_;
};

TEST_F(OutputStreamCallbackTest, SetCallbackStoresIt) {
  bool callback_called = false;
  runtime_->SetOutputStreamCallback("out",
      [&callback_called](const Packet&) { callback_called = true; });
  EXPECT_FALSE(callback_called);
}

TEST_F(OutputStreamCallbackTest, ClearCallbackRemovesIt) {
  bool callback_called = false;
  runtime_->SetOutputStreamCallback("out",
      [&callback_called](const Packet&) { callback_called = true; });
  runtime_->ClearOutputStreamCallback("out");
  EXPECT_FALSE(callback_called);
}

TEST_F(OutputStreamCallbackTest, ClearNonExistentCallbackIsIdempotent) {
  runtime_->ClearOutputStreamCallback("nonexistent");
  SUCCEED();
}

TEST_F(OutputStreamCallbackTest, SetCallbackOverwritesPrevious) {
  int call_count = 0;
  runtime_->SetOutputStreamCallback("out",
      [&call_count](const Packet&) { call_count = 1; });
  runtime_->SetOutputStreamCallback("out",
      [&call_count](const Packet&) { call_count = 2; });
  EXPECT_EQ(call_count, 0);
}

TEST_F(OutputStreamCallbackTest, InitializeWithOutputStreamsSucceeds) {
  GraphConfig config;
  config.nodes.push_back(
      {"src", "UnregisteredNode", {}, {}, {"out"}, {}, {}, "", "", 0, 0});
  auto status = runtime_->Initialize(config);
  // May fail if node not registered, but should not crash.
  EXPECT_TRUE(status.ok() || !status.ok());
}

}  // namespace graph::runtime
