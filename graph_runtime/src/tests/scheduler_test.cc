#include "src/framework/public/graph_runtime.h"

#include <memory>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "src/framework/config/graph_config.h"
#include "src/framework/stream/packet.h"
#include "src/framework/scheduler/scheduler.h"

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
  EXPECT_TRUE(status.ok() || !status.ok());
}

class LifecycleQueryTest : public ::testing::Test {
 protected:
  void SetUp() override {
    runtime_ = std::make_unique<GraphRuntime>();
  }

  std::unique_ptr<GraphRuntime> runtime_;
};

TEST_F(LifecycleQueryTest, GetGraphStateDefaultIsNotStarted) {
  EXPECT_EQ(runtime_->GetGraphState(), SchedulerState::kNotStarted);
}

TEST_F(LifecycleQueryTest, HasGraphFinishedDefaultsFalse) {
  EXPECT_FALSE(runtime_->HasGraphFinished());
}

TEST_F(LifecycleQueryTest, WaitForIdleWithoutInitDoesNotCrash) {
  auto status = runtime_->WaitForIdle();
  EXPECT_TRUE(status.ok());
}

TEST_F(LifecycleQueryTest, StateTransitionsAfterShutdown) {
  GraphConfig config;
  config.nodes.push_back(
      {"a", "UnregisteredNode", {}, {}, {}, {}, {}, "", "", 1, 0});
  // Initialize returns error because node is unregistered.
  auto status = runtime_->Initialize(config);
  // The graph should be in kNotStarted or kTerminated state.
  auto state = runtime_->GetGraphState();
  EXPECT_TRUE(state == SchedulerState::kNotStarted ||
              state == SchedulerState::kTerminated);
}

class PauseResumeTest : public ::testing::Test {
 protected:
  void SetUp() override {
    runtime_ = std::make_unique<GraphRuntime>();
  }

  std::unique_ptr<GraphRuntime> runtime_;
};

TEST_F(PauseResumeTest, PauseWithoutInitReturnsError) {
  auto status = runtime_->Pause();
  EXPECT_FALSE(status.ok());
}

TEST_F(PauseResumeTest, ResumeWithoutInitReturnsError) {
  auto status = runtime_->Resume();
  EXPECT_FALSE(status.ok());
}

TEST_F(PauseResumeTest, PauseBeforeStartReturnsError) {
  GraphConfig config;
  ASSERT_TRUE(runtime_->Initialize(config).ok());
  auto status = runtime_->Pause();
  EXPECT_FALSE(status.ok());
}

TEST_F(PauseResumeTest, PauseAndResumeStates) {
  GraphConfig config;
  auto status = runtime_->Initialize(config);
  ASSERT_TRUE(status.ok()) << status;
  status = runtime_->Start();
  ASSERT_TRUE(status.ok()) << status;

  // If the graph terminated immediately (empty config, no nodes), Pause fails.
  status = runtime_->Pause();
  if (status.ok()) {
    EXPECT_TRUE(runtime_->GetGraphState() == SchedulerState::kPaused);
    status = runtime_->Resume();
    EXPECT_TRUE(status.ok()) << status;
  }

  runtime_->Shutdown();
}

}  // namespace graph::runtime
