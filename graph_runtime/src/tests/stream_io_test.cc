#include <string>

#include "src/public/graph_runtime.h"
#include "gtest/gtest.h"

namespace graph::runtime {

TEST(StreamIOTest, AddPacketOnUninitializedReturnsError) {
  GraphRuntime runtime;
  auto pkt = Packet::MakePacket<int>(42);
  auto status = runtime.AddPacketToInputStream("input", pkt);
  EXPECT_TRUE(absl::IsNotFound(status)) << status;
}

TEST(StreamIOTest, CloseInputStreamOnUninitializedReturnsError) {
  GraphRuntime runtime;
  auto status = runtime.CloseInputStream("input");
  EXPECT_TRUE(absl::IsNotFound(status)) << status;
}

}  // namespace graph::runtime
