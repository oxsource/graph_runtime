// fanout_repro_test.cc
//
// Minimal reproduction (T001) for feature 011-stream-fanout: one output stream
// consumed by two downstream nodes (1->N fan-out).
//
// Current (pre-fix) behavior: GraphRuntime::Initialize dedups input managers by
// the full "port:stream" name, so only the first consumer (sink_a) gets an input
// manager registered on its input port. sink_b ends up with input_port_count()==0
// and is mis-scheduled as a never-ending source, so WaitUntilDone() never returns
// and the graph deadlocks.
//
// This test is the RED baseline for that bug: it expects WaitUntilDone() to return
// and both consumers to receive every packet. It is bounded by a watchdog so the
// pre-fix deadlock surfaces as a clear test failure instead of an infinite hang.

#include <atomic>
#include <chrono>
#include <future>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include "gtest/gtest.h"
#include "src/framework/node/node.h"
#include "src/framework/node/node_contract.h"
#include "src/framework/node/node_factory.h"
#include "src/framework/node/node_registry.h"
#include "src/framework/node/graph_context.h"
#include "src/framework/stream/packet.h"
#include "src/framework/public/graph_runtime.h"

namespace graph::runtime {

namespace {

constexpr int kEmitCount = 5;
constexpr std::chrono::seconds kReproTimeout(10);

// IntSource emits kEmitCount ints on output port "output" then StatusStop.
class FanoutIntSource : public Node {
 public:
  FanoutIntSource(const std::string& n, const NodeOptions&) : Node(n) {}
  static absl::Status GetContract(NodeContract* c) {
    c->Outputs().Get("output").Set<int>();
    return absl::OkStatus();
  }
  absl::Status Open(GraphContext&) override { return absl::OkStatus(); }
  absl::Status Process(GraphContext& ctx) override {
    if (sent_ >= kEmitCount) return StatusStop();
    ctx.Outputs().Get("output").AddPacket(
        Packet::MakePacket<int>(sent_).At(ctx.InputTimestamp()));
    ++sent_;
    return absl::OkStatus();
  }
  absl::Status Close(GraphContext&) override { return absl::OkStatus(); }
  int sent_ = 0;
};

// CountingSink counts every packet observed on input port "input". Instances are
// captured by node name so the test can inspect them after the graph terminates
// (GraphRuntime has no public node accessor).
class FanoutCountingSink : public Node {
 public:
  FanoutCountingSink(const std::string& n, const NodeOptions&) : Node(n) {
    s_captured[n] = this;
  }
  static absl::Status GetContract(NodeContract* c) {
    c->Inputs().Get("input").Set<int>();
    return absl::OkStatus();
  }
  absl::Status Open(GraphContext&) override { return absl::OkStatus(); }
  absl::Status Process(GraphContext& ctx) override {
    auto& shard = ctx.Inputs().Get("input");
    if (shard.IsEmpty()) return absl::OkStatus();
    ++received_;
    return absl::OkStatus();
  }
  absl::Status Close(GraphContext&) override { return absl::OkStatus(); }
  int received_ = 0;

  static FanoutCountingSink* Captured(const std::string& name) {
    auto it = s_captured.find(name);
    return it != s_captured.end() ? it->second : nullptr;
  }
  static std::map<std::string, FanoutCountingSink*> s_captured;
};

std::map<std::string, FanoutCountingSink*> FanoutCountingSink::s_captured;

bool RegisterFanoutNodes() {
  static const bool registered = []() {
    NodeFactoryRegistry::Register(
        "FanoutIntSource", std::make_unique<NodeFactoryFor<FanoutIntSource>>());
    NodeFactoryRegistry::Register(
        "FanoutCountingSink",
        std::make_unique<NodeFactoryFor<FanoutCountingSink>>());
    return true;
  }();
  return registered;
}

// Runs WaitUntilDone() on a worker thread and returns whether it completed
// within kReproTimeout. On timeout, Shutdown() unblocks the worker so the test
// terminates cleanly with a deadlock failure instead of hanging.
bool WaitUntilDoneBounded(GraphRuntime& runtime) {
  std::promise<bool> done;
  auto future = done.get_future();
  std::thread worker([&]() {
    (void)runtime.WaitUntilDone();
    done.set_value(true);
  });
  bool finished = future.wait_for(kReproTimeout) == std::future_status::ready;
  if (!finished) {
    runtime.Shutdown();
    worker.join();
    return false;
  }
  worker.join();
  return true;
}

}  // namespace

TEST(FanoutReproTest, IntSourceToTwoSinksSameStreamCompletesAndBothReceiveFull) {
  RegisterFanoutNodes();
  FanoutCountingSink::s_captured.clear();

  GraphConfig config;
  config.nodes.push_back(
      {"src", "FanoutIntSource", {}, {"output:x"}, {}, {}, {}, "", "", 1, 0});
  config.nodes.push_back(
      {"sink_a", "FanoutCountingSink", {"input:x"}, {}, {}, {}, {}, "", "", 1, 0});
  config.nodes.push_back(
      {"sink_b", "FanoutCountingSink", {"input:x"}, {}, {}, {}, {}, "", "", 1, 0});

  GraphRuntime runtime;
  ASSERT_TRUE(runtime.Initialize(config).ok());
  ASSERT_TRUE(runtime.Start().ok());

  ASSERT_TRUE(WaitUntilDoneBounded(runtime))
      << "Graph deadlocked: WaitUntilDone() did not return within "
      << kReproTimeout.count()
      << "s (fan-out: second consumer starved of an input queue and "
         "mis-scheduled as a never-ending source)";

  FanoutCountingSink* sink_a = FanoutCountingSink::Captured("sink_a");
  FanoutCountingSink* sink_b = FanoutCountingSink::Captured("sink_b");
  ASSERT_NE(sink_a, nullptr);
  ASSERT_NE(sink_b, nullptr);
  EXPECT_EQ(sink_a->received_, kEmitCount)
      << "sink_a must receive every packet from the shared stream";
  EXPECT_EQ(sink_b->received_, kEmitCount)
      << "sink_b must receive every packet from the shared stream";
}

}  // namespace graph::runtime