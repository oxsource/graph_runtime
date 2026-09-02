// fanout_graph_test.cc
//
// User Story 1 (feature 011-stream-fanout): a single output stream consumed by
// multiple downstream nodes (1→N fan-out). Each consumer receives a full,
// independent copy; the graph completes normally.
//
//   T009 1 source → 2 sinks sharing the same stream name.
//   T010 Diamond DAG: source → (A, B); A→C and B→C (C has two inputs).
//   T011 Fan-out error localization: a failing consumer's Process error reaches
//        the error callback with the node name; the healthy branch drains.
//   T014 Graph input (config.input_streams) fanned out to two consumers.
//   T015 Single-consumer graph input regression (unchanged behavior).

#include <atomic>
#include <chrono>
#include <future>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include "absl/status/status.h"
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
constexpr std::chrono::seconds kTimeout(30);

// Captures instances by node name so tests can inspect state after the graph
// terminates (GraphRuntime has no public node accessor).
std::map<std::string, class FanoutCountingSink*> g_sinks;
std::map<std::string, class FanoutForwarder*> g_forwarders;
std::map<std::string, class FanoutFailingSink*> g_failing_sinks;
std::map<std::string, class FanoutJoiningSink*> g_joiners;

void ClearCaptures() {
  g_sinks.clear();
  g_forwarders.clear();
  g_failing_sinks.clear();
  g_joiners.clear();
}

// Source: emits kEmitCount ints on "out" then StatusStop.
class FanoutSource : public Node {
 public:
  FanoutSource(const std::string& n, const NodeOptions&) : Node(n) {}
  static absl::Status GetContract(NodeContract* c) {
    c->Outputs().Get("out").Set<int>();
    return absl::OkStatus();
  }
  absl::Status Open(GraphContext&) override { return absl::OkStatus(); }
  absl::Status Process(GraphContext& ctx) override {
    if (sent_ >= kEmitCount) return StatusStop();
    ctx.Outputs().Get("out").AddPacket(
        Packet::MakePacket<int>(sent_).At(ctx.InputTimestamp()));
    ++sent_;
    return absl::OkStatus();
  }
  absl::Status Close(GraphContext&) override { return absl::OkStatus(); }
  int sent_ = 0;
};

// Pass-through: consumes "in", forwards the payload on "out" at the same
// timestamp. Used for the diamond branches.
class FanoutForwarder : public Node {
 public:
  FanoutForwarder(const std::string& n, const NodeOptions&) : Node(n) {
    g_forwarders[n] = this;
  }
  static absl::Status GetContract(NodeContract* c) {
    c->Inputs().Get("in").Set<int>();
    c->Outputs().Get("out").Set<int>();
    return absl::OkStatus();
  }
  absl::Status Open(GraphContext&) override { return absl::OkStatus(); }
  absl::Status Process(GraphContext& ctx) override {
    auto& shard = ctx.Inputs().Get("in");
    if (shard.IsEmpty()) return absl::OkStatus();
    auto r = shard.Get<int>();
    if (!r.ok()) return absl::OkStatus();
    ctx.Outputs().Get("out").AddPacket(
        Packet::MakePacket<int>(*r).At(ctx.InputTimestamp()));
    ++consumed_;
    return absl::OkStatus();
  }
  absl::Status Close(GraphContext&) override { return absl::OkStatus(); }
  int consumed_ = 0;
};

// Sink: counts packets on "in"; records when the input is observed done.
class FanoutCountingSink : public Node {
 public:
  FanoutCountingSink(const std::string& n, const NodeOptions&) : Node(n) {
    g_sinks[n] = this;
  }
  static absl::Status GetContract(NodeContract* c) {
    c->Inputs().Get("in").Set<int>();
    return absl::OkStatus();
  }
  absl::Status Open(GraphContext&) override { return absl::OkStatus(); }
  absl::Status Process(GraphContext& ctx) override {
    auto& shard = ctx.Inputs().Get("in");
    if (shard.IsDone()) saw_done_ = true;
    if (shard.IsEmpty()) return absl::OkStatus();
    ++received_;
    return absl::OkStatus();
  }
  absl::Status Close(GraphContext&) override {
    // Finalize must happen only after the input done signal was observed.
    finalized_ = true;
    finalized_after_done_ = saw_done_;
    return absl::OkStatus();
  }
  int received_ = 0;
  bool saw_done_ = false;
  bool finalized_ = false;
  bool finalized_after_done_ = false;
};

// Two-input sink: counts packets observed on each of its two input ports.
class FanoutJoiningSink : public Node {
 public:
  FanoutJoiningSink(const std::string& n, const NodeOptions&) : Node(n) {
    g_joiners[n] = this;
  }
  static absl::Status GetContract(NodeContract* c) {
    c->Inputs().Get("a_in").Set<int>();
    c->Inputs().Get("b_in").Set<int>();
    return absl::OkStatus();
  }
  absl::Status Open(GraphContext&) override { return absl::OkStatus(); }
  absl::Status Process(GraphContext& ctx) override {
    if (!ctx.Inputs().Get("a_in").IsEmpty()) ++a_received_;
    if (!ctx.Inputs().Get("b_in").IsEmpty()) ++b_received_;
    return absl::OkStatus();
  }
  absl::Status Close(GraphContext&) override { return absl::OkStatus(); }
  int a_received_ = 0;
  int b_received_ = 0;
};

// Sink that fails on its kEmitCount-th packet (the last one), after all
// packets have been propagated to every mirror of the shared output stream.
class FanoutFailingSink : public Node {
 public:
  FanoutFailingSink(const std::string& n, const NodeOptions&) : Node(n) {
    g_failing_sinks[n] = this;
  }
  static absl::Status GetContract(NodeContract* c) {
    c->Inputs().Get("in").Set<int>();
    return absl::OkStatus();
  }
  absl::Status Open(GraphContext&) override { return absl::OkStatus(); }
  absl::Status Process(GraphContext& ctx) override {
    auto& shard = ctx.Inputs().Get("in");
    if (shard.IsDone()) saw_done_ = true;
    if (shard.IsEmpty()) return absl::OkStatus();
    ++received_;
    if (received_ >= kEmitCount) {
      return absl::InternalError("deliberate failure at last packet");
    }
    return absl::OkStatus();
  }
  absl::Status Close(GraphContext&) override { return absl::OkStatus(); }
  int received_ = 0;
  bool saw_done_ = false;
};

bool RegisterFanoutGraphNodes() {
  static const bool registered = []() {
    NodeFactoryRegistry::Register(
        "FanoutSource", std::make_unique<NodeFactoryFor<FanoutSource>>());
    NodeFactoryRegistry::Register(
        "FanoutForwarder", std::make_unique<NodeFactoryFor<FanoutForwarder>>());
    NodeFactoryRegistry::Register(
        "FanoutCountingSink",
        std::make_unique<NodeFactoryFor<FanoutCountingSink>>());
    NodeFactoryRegistry::Register(
        "FanoutJoiningSink",
        std::make_unique<NodeFactoryFor<FanoutJoiningSink>>());
    NodeFactoryRegistry::Register(
        "FanoutFailingSink",
        std::make_unique<NodeFactoryFor<FanoutFailingSink>>());
    return true;
  }();
  return registered;
}

// Runs WaitUntilDone() on a worker thread; returns whether it finished within
// kTimeout. On timeout, Shutdown() unblocks the worker so the test terminates
// with a clear failure instead of hanging.
bool WaitUntilDoneBounded(GraphRuntime& runtime) {
  std::promise<bool> done;
  auto future = done.get_future();
  std::thread worker([&]() {
    (void)runtime.WaitUntilDone();
    done.set_value(true);
  });
  bool finished = future.wait_for(kTimeout) == std::future_status::ready;
  if (!finished) {
    runtime.Shutdown();
    worker.join();
    return false;
  }
  worker.join();
  return true;
}

}  // namespace

// T009: 1 source → 2 sinks sharing the same stream name. Both sinks receive
// every packet and the graph completes (formal version of the T001 repro).
TEST(FanoutGraphTest, OneSourceToTwoSinksSameStream) {
  RegisterFanoutGraphNodes();
  ClearCaptures();

  GraphConfig config;
  config.nodes.push_back(
      {"src", "FanoutSource", {}, {"out:x"}, {}, {}, {}, "", "", 1, 0});
  config.nodes.push_back(
      {"sink_a", "FanoutCountingSink", {"in:x"}, {}, {}, {}, {}, "", "", 1, 0});
  config.nodes.push_back(
      {"sink_b", "FanoutCountingSink", {"in:x"}, {}, {}, {}, {}, "", "", 1, 0});

  GraphRuntime runtime;
  ASSERT_TRUE(runtime.Initialize(config).ok());
  ASSERT_TRUE(runtime.Start().ok());
  ASSERT_TRUE(WaitUntilDoneBounded(runtime)) << "graph deadlocked under fan-out";

  ASSERT_NE(g_sinks["sink_a"], nullptr);
  ASSERT_NE(g_sinks["sink_b"], nullptr);
  EXPECT_EQ(g_sinks["sink_a"]->received_, kEmitCount);
  EXPECT_EQ(g_sinks["sink_b"]->received_, kEmitCount);
  // Producer Close → Done must broadcast to every mirror: each consumer
  // observes its input stream done and finalizes (completion semantics under
  // fan-out, T013).
  EXPECT_TRUE(g_sinks["sink_a"]->saw_done_);
  EXPECT_TRUE(g_sinks["sink_b"]->saw_done_);
}

// T010: Diamond DAG. Source fans out to A and B; both forward to C (two
// inputs). Every branch drains and C consumes a full copy from each branch.
TEST(FanoutGraphTest, DiamondDagBothBranchesDrainToJoiningNode) {
  RegisterFanoutGraphNodes();
  ClearCaptures();

  GraphConfig config;
  config.nodes.push_back(
      {"src", "FanoutSource", {}, {"out:x"}, {}, {}, {}, "", "", 1, 0});
  config.nodes.push_back(
      {"a", "FanoutForwarder", {"in:x"}, {"out:a_out"}, {}, {}, {}, "", "", 1, 0});
  config.nodes.push_back(
      {"b", "FanoutForwarder", {"in:x"}, {"out:b_out"}, {}, {}, {}, "", "", 1, 0});
  config.nodes.push_back(
      {"c", "FanoutJoiningSink", {"a_in:a_out", "b_in:b_out"}, {}, {}, {}, {}, "", "", 1, 0});

  GraphRuntime runtime;
  ASSERT_TRUE(runtime.Initialize(config).ok());
  ASSERT_TRUE(runtime.Start().ok());
  ASSERT_TRUE(WaitUntilDoneBounded(runtime)) << "diamond DAG deadlocked";

  ASSERT_NE(g_forwarders["a"], nullptr);
  ASSERT_NE(g_forwarders["b"], nullptr);
  ASSERT_NE(g_joiners["c"], nullptr);
  EXPECT_EQ(g_forwarders["a"]->consumed_, kEmitCount);
  EXPECT_EQ(g_forwarders["b"]->consumed_, kEmitCount);
  EXPECT_EQ(g_joiners["c"]->a_received_, kEmitCount);
  EXPECT_EQ(g_joiners["c"]->b_received_, kEmitCount);
}

// T011: Fan-out error localization. One consumer fails on its last packet;
// the error callback must report a status naming the failing node, and the
// healthy consumer must still drain its full copy.
TEST(FanoutGraphTest, FailingConsumerErrorCarriesNodeNameHealthyBranchDrains) {
  RegisterFanoutGraphNodes();
  ClearCaptures();

  GraphConfig config;
  config.nodes.push_back(
      {"src", "FanoutSource", {}, {"out:x"}, {}, {}, {}, "", "", 1, 0});
  config.nodes.push_back(
      {"sink_a", "FanoutCountingSink", {"in:x"}, {}, {}, {}, {}, "", "", 1, 0});
  config.nodes.push_back(
      {"sink_b", "FanoutFailingSink", {"in:x"}, {}, {}, {}, {}, "", "", 1, 0});

  GraphRuntime runtime;
  ASSERT_TRUE(runtime.Initialize(config).ok());

  std::vector<absl::Status> errors;
  runtime.SetErrorCallback([&errors](absl::Status s) { errors.push_back(s); });

  ASSERT_TRUE(runtime.Start().ok());
  ASSERT_TRUE(WaitUntilDoneBounded(runtime)) << "graph deadlocked on error path";

  bool named_error = false;
  for (const auto& s : errors) {
    if (s.message().find("sink_b") != std::string::npos) named_error = true;
  }
  EXPECT_TRUE(named_error) << "error callback must name the failing node 'sink_b'";

  ASSERT_NE(g_sinks["sink_a"], nullptr);
  ASSERT_NE(g_failing_sinks["sink_b"], nullptr);
  EXPECT_EQ(g_sinks["sink_a"]->received_, kEmitCount)
      << "healthy branch must drain its full copy";
  EXPECT_TRUE(g_sinks["sink_a"]->saw_done_)
      << "healthy branch's input must reach done (not swallowed by the "
         "failing branch)";
  EXPECT_EQ(g_failing_sinks["sink_b"]->received_, kEmitCount);
}

// T014: Graph input (config.input_streams) fanned out to two consumers. Inject
// N packets + CloseInputStream; both consumers receive a full copy and the
// graph completes (Plan B: graph input = virtual source output manager).
TEST(FanoutGraphTest, GraphInputFansOutToTwoSinks) {
  RegisterFanoutGraphNodes();
  ClearCaptures();

  GraphConfig config;
  config.input_streams.push_back("graph_in");
  config.nodes.push_back(
      {"sink_a", "FanoutCountingSink", {"in:graph_in"}, {}, {}, {}, {}, "", "", 1, 0});
  config.nodes.push_back(
      {"sink_b", "FanoutCountingSink", {"in:graph_in"}, {}, {}, {}, {}, "", "", 1, 0});

  GraphRuntime runtime;
  ASSERT_TRUE(runtime.Initialize(config).ok());
  ASSERT_TRUE(runtime.Start().ok());

  for (int i = 0; i < kEmitCount; ++i) {
    ASSERT_TRUE(runtime
                    .AddPacketToInputStream(
                        "graph_in", Packet::MakePacket<int>(i).At(Timestamp(i)))
                    .ok());
  }
  ASSERT_TRUE(runtime.CloseInputStream("graph_in").ok());
  ASSERT_TRUE(WaitUntilDoneBounded(runtime))
      << "graph input fan-out deadlocked";

  ASSERT_NE(g_sinks["sink_a"], nullptr);
  ASSERT_NE(g_sinks["sink_b"], nullptr);
  EXPECT_EQ(g_sinks["sink_a"]->received_, kEmitCount);
  EXPECT_EQ(g_sinks["sink_b"]->received_, kEmitCount);
}

// T015: Single-consumer graph input regression. Injecting + closing a graph
// input consumed by exactly one node behaves identically to before.
TEST(FanoutGraphTest, SingleConsumerGraphInputRegression) {
  RegisterFanoutGraphNodes();
  ClearCaptures();

  GraphConfig config;
  config.input_streams.push_back("graph_in");
  config.nodes.push_back(
      {"sink", "FanoutCountingSink", {"in:graph_in"}, {}, {}, {}, {}, "", "", 1, 0});

  GraphRuntime runtime;
  ASSERT_TRUE(runtime.Initialize(config).ok());
  ASSERT_TRUE(runtime.Start().ok());

  for (int i = 0; i < kEmitCount; ++i) {
    ASSERT_TRUE(runtime
                    .AddPacketToInputStream(
                        "graph_in", Packet::MakePacket<int>(i).At(Timestamp(i)))
                    .ok());
  }
  ASSERT_TRUE(runtime.CloseInputStream("graph_in").ok());
  ASSERT_TRUE(WaitUntilDoneBounded(runtime))
      << "single-consumer graph input regression deadlocked";

  ASSERT_NE(g_sinks["sink"], nullptr);
  EXPECT_EQ(g_sinks["sink"]->received_, kEmitCount);
}

// T019: Fan-out + producer Close. Both consumers observe their input stream
// done and finalize only after the done signal (normal Close across all
// branches; complements T011's error branch).
TEST(FanoutGraphTest, ProducerCloseDoneBroadcastsToAllConsumersAndTheyFinalize) {
  RegisterFanoutGraphNodes();
  ClearCaptures();

  GraphConfig config;
  config.nodes.push_back(
      {"src", "FanoutSource", {}, {"out:x"}, {}, {}, {}, "", "", 1, 0});
  config.nodes.push_back(
      {"sink_a", "FanoutCountingSink", {"in:x"}, {}, {}, {}, {}, "", "", 1, 0});
  config.nodes.push_back(
      {"sink_b", "FanoutCountingSink", {"in:x"}, {}, {}, {}, {}, "", "", 1, 0});

  GraphRuntime runtime;
  ASSERT_TRUE(runtime.Initialize(config).ok());
  ASSERT_TRUE(runtime.Start().ok());
  ASSERT_TRUE(WaitUntilDoneBounded(runtime))
      << "producer Close + fan-out deadlocked";

  ASSERT_NE(g_sinks["sink_a"], nullptr);
  ASSERT_NE(g_sinks["sink_b"], nullptr);
  for (const auto& name : {"sink_a", "sink_b"}) {
    EXPECT_EQ(g_sinks[name]->received_, kEmitCount);
    EXPECT_TRUE(g_sinks[name]->saw_done_)
        << name << " must observe its input stream done";
    EXPECT_TRUE(g_sinks[name]->finalized_)
        << name << " must finalize";
    EXPECT_TRUE(g_sinks[name]->finalized_after_done_)
        << name << " must finalize only after observing done";
  }
}

// T020: An output stream with no consumers. PropagateUpdatesToMirrors with an
// empty mirror list must only clear the shard and must not crash; the graph
// still completes.
TEST(FanoutGraphTest, OutputStreamWithNoConsumersDoesNotCrash) {
  RegisterFanoutGraphNodes();
  ClearCaptures();

  GraphConfig config;
  config.nodes.push_back(
      {"src", "FanoutSource", {}, {"out:orphan"}, {}, {}, {}, "", "", 1, 0});

  GraphRuntime runtime;
  ASSERT_TRUE(runtime.Initialize(config).ok());
  ASSERT_TRUE(runtime.Start().ok());
  ASSERT_TRUE(WaitUntilDoneBounded(runtime))
      << "unconsumed output stream deadlocked";
}

}  // namespace graph::runtime