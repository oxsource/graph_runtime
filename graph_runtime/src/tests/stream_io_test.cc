#include <string>

#include "src/framework/public/graph_runtime.h"
#include "gtest/gtest.h"

namespace graph::runtime {

// T028: Lifecycle order — Init → Start → Shutdown succeeds
TEST(StreamIOTest, LifecycleInitStartShutdown) {
  GraphRuntime runtime;
  GraphConfig config;

  // Initialize with empty config
  EXPECT_TRUE(runtime.Initialize(config).ok());
  EXPECT_TRUE(runtime.Start().ok());
  runtime.Shutdown();
}

// T029: AddPacket before Start — should error (graph not running)
TEST(StreamIOTest, AddPacketBeforeStartReturnsError) {
  GraphRuntime runtime;
  GraphConfig config;
  EXPECT_TRUE(runtime.Initialize(config).ok());

  auto pkt = Packet::MakePacket<int>(42);
  // Stream doesn't exist because no config specifies it
  auto status = runtime.AddPacketToInputStream("input", pkt);
  EXPECT_TRUE(absl::IsNotFound(status)) << status;
}

// T030: AddPacket on unknown stream — NotFoundError
TEST(StreamIOTest, UnknownStreamReturnsNotFound) {
  GraphRuntime runtime;
  GraphConfig config;
  EXPECT_TRUE(runtime.Initialize(config).ok());

  auto pkt = Packet::MakePacket<int>(42);
  auto status = runtime.AddPacketToInputStream("nonexistent", pkt);
  EXPECT_TRUE(absl::IsNotFound(status)) << status;
}

// T031: CloseInputStream on unknown stream — NotFoundError
TEST(StreamIOTest, CloseUnknownStreamReturnsNotFound) {
  GraphRuntime runtime;
  GraphConfig config;
  EXPECT_TRUE(runtime.Initialize(config).ok());

  auto status = runtime.CloseInputStream("nonexistent");
  EXPECT_TRUE(absl::IsNotFound(status)) << status;
}

}  // namespace graph::runtime
