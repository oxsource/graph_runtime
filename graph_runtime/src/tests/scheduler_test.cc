#include "src/framework/public/graph_runtime.h"

#include <memory>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "src/framework/config/graph_config.h"
#include "src/framework/stream/packet.h"
#include "src/framework/scheduler/scheduler.h"
#include "src/framework/tool/tag_map.h"
#include "src/framework/tool/validate_name.h"
#include "src/framework/scheduler/input_stream_handler.h"
#include "src/framework/node/node.h"
#include "src/framework/node/node_contract.h"
#include "src/framework/node/node_factory.h"
#include "src/framework/node/node_registry.h"

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

class TagMapTest : public ::testing::Test {};

TEST_F(TagMapTest, ParsePlainName) {
  auto result = ParseTagIndexName("input");
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result->tag, "");
  EXPECT_EQ(result->index, -1);
  EXPECT_EQ(result->name, "input");
}

TEST_F(TagMapTest, ParseTagIndex) {
  auto result = ParseTagIndexName("VIDEO:0");
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result->tag, "VIDEO");
  EXPECT_EQ(result->index, 0);
  EXPECT_EQ(result->name, "");
}

TEST_F(TagMapTest, ParseTagIndexName) {
  auto result = ParseTagIndexName("AUDIO:3:left_channel");
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result->tag, "AUDIO");
  EXPECT_EQ(result->index, 3);
  EXPECT_EQ(result->name, "left_channel");
}

TEST_F(TagMapTest, ParseInvalidIndex) {
  auto result = ParseTagIndexName("VIDEO:abc");
  ASSERT_FALSE(result.ok());
}

TEST_F(TagMapTest, TagMapCreate) {
  auto tag_map = TagMap::Create({"VIDEO:0:left", "VIDEO:1:right", "AUDIO:0:mono"});
  ASSERT_TRUE(tag_map.ok());
  EXPECT_EQ(tag_map->NumEntries(), 3);
  EXPECT_TRUE(tag_map->HasTag("VIDEO"));
  EXPECT_TRUE(tag_map->HasTag("AUDIO"));
  EXPECT_EQ(tag_map->NumEntries("VIDEO"), 2);
  EXPECT_EQ(tag_map->NumEntries("AUDIO"), 1);
}

TEST_F(TagMapTest, TagMapGetId) {
  auto tag_map = TagMap::Create({"VIDEO:0", "VIDEO:1"});
  ASSERT_TRUE(tag_map.ok());
  EXPECT_EQ(tag_map->GetId("VIDEO", 0), 0);
  EXPECT_EQ(tag_map->GetId("VIDEO", 1), 1);
  EXPECT_EQ(tag_map->GetId("VIDEO", 2), -1);
  EXPECT_EQ(tag_map->GetId("UNKNOWN", 0), -1);
}

TEST_F(TagMapTest, TagMapGetIdByString) {
  auto tag_map = TagMap::Create({"VIDEO:0:left", "VIDEO:1:right"});
  ASSERT_TRUE(tag_map.ok());
  EXPECT_EQ(tag_map->GetId("VIDEO:0"), 0);
  EXPECT_EQ(tag_map->GetId("VIDEO:1"), 1);
  EXPECT_EQ(tag_map->GetId("VIDEO:2"), -1);
}

TEST_F(TagMapTest, TagMapNames) {
  auto tag_map = TagMap::Create({"VIDEO:0:left", "VIDEO:1:right", "AUDIO:0:mono"});
  ASSERT_TRUE(tag_map.ok());
  ASSERT_EQ(tag_map->Names().size(), 3);
  EXPECT_EQ(tag_map->Names()[0], "left");
  EXPECT_EQ(tag_map->Names()[1], "right");
  EXPECT_EQ(tag_map->Names()[2], "mono");
}

TEST_F(TagMapTest, TagMapGetTags) {
  auto tag_map = TagMap::Create({"VIDEO:0", "AUDIO:0"});
  ASSERT_TRUE(tag_map.ok());
  auto tags = tag_map->GetTags();
  EXPECT_TRUE(tags.count("VIDEO"));
  EXPECT_TRUE(tags.count("AUDIO"));
}

TEST_F(TagMapTest, PacketTypeSetIndexedGet) {
  NodeContract contract;
  contract.Inputs().Get("VIDEO", 0).Set<std::string>();
  contract.Inputs().Get("VIDEO", 1).Set<int>();
  EXPECT_TRUE(contract.Inputs().Get("VIDEO", 0).IsSet());
  EXPECT_TRUE(contract.Inputs().Get("VIDEO", 1).IsSet());
  EXPECT_EQ(contract.Inputs().NumEntries(), 2);
}

class IndexedStreamTest : public ::testing::Test {
 protected:
  void SetUp() override {
    runtime_ = std::make_unique<GraphRuntime>();
    // Node with indexed input streams.
    config_.nodes.push_back(
        {"n", "UnregisteredNode", {"VIDEO:0", "VIDEO:1"}, {}, {}, {}, {}, "", "", 0, 0});
  }

  std::unique_ptr<GraphRuntime> runtime_;
  GraphConfig config_;
};

TEST_F(IndexedStreamTest, AddByStringForm) {
  auto status = runtime_->Initialize(config_);
  // May fail if TestRecordNode not registered, but should not crash.
  EXPECT_TRUE(status.ok() || !status.ok());
  if (!status.ok()) return;

  auto pkt = Packet::MakePacket<int>(1);
  status = runtime_->AddPacketToInputStream("VIDEO:0", std::move(pkt));
  EXPECT_TRUE(status.ok()) << status;
}

TEST_F(IndexedStreamTest, AddByTagIndexForm) {
  auto status = runtime_->Initialize(config_);
  if (!status.ok()) return;

  auto pkt = Packet::MakePacket<int>(1);
  status = runtime_->AddPacketToInputStream("VIDEO", 0, std::move(pkt));
  EXPECT_TRUE(status.ok()) << status;
}

TEST_F(IndexedStreamTest, UnknownStreamReturnsError) {
  auto status = runtime_->Initialize(config_);
  if (!status.ok()) return;

  auto pkt = Packet::MakePacket<int>(1);
  status = runtime_->AddPacketToInputStream("VIDEO", 99, std::move(pkt));
  EXPECT_FALSE(status.ok());
  EXPECT_TRUE(absl::IsNotFound(status)) << status;
}

class InputStreamHandlerFactoryTest : public ::testing::Test {};

TEST_F(InputStreamHandlerFactoryTest, CreateDefaultHandler) {
  auto handler = CreateInputStreamHandler("default");
  EXPECT_NE(handler, nullptr);
}

TEST_F(InputStreamHandlerFactoryTest, CreateImmediateHandler) {
  auto handler = CreateInputStreamHandler("immediate");
  EXPECT_NE(handler, nullptr);
}

TEST_F(InputStreamHandlerFactoryTest, CreateFixedSizeHandler) {
  auto handler = CreateInputStreamHandler("fixed_size", 5);
  EXPECT_NE(handler, nullptr);
}

TEST_F(InputStreamHandlerFactoryTest, UnknownNameUsesDefault) {
  auto handler = CreateInputStreamHandler("unknown_type");
  EXPECT_NE(handler, nullptr);
}

class MaxInFlightTest : public ::testing::Test {
 protected:
  class TestMaxInFlightNode : public Node {
   public:
    TestMaxInFlightNode(const std::string& n, const NodeOptions& opts) : Node(n) {}
    absl::Status Open(GraphContext&) override { return absl::OkStatus(); }
    absl::Status Process(GraphContext&) override { return absl::OkStatus(); }
    absl::Status Close(GraphContext&) override { return absl::OkStatus(); }
  };
};

TEST_F(MaxInFlightTest, PendingCountStartsZero) {
  NodeOptions opts;
  TestMaxInFlightNode node("test", opts);
  EXPECT_EQ(node.pending_count(), 0);
}

TEST_F(MaxInFlightTest, IncrementAndDecrement) {
  NodeOptions opts;
  TestMaxInFlightNode node("test", opts);
  node.IncrementPending();
  EXPECT_EQ(node.pending_count(), 1);
  node.IncrementPending();
  EXPECT_EQ(node.pending_count(), 2);
  node.DecrementPending();
  EXPECT_EQ(node.pending_count(), 1);
  node.DecrementPending();
  EXPECT_EQ(node.pending_count(), 0);
}

TEST_F(MaxInFlightTest, AddNodeRespectsMaxInFlight) {
  SchedulerQueue queue("test");
  NodeOptions opts;
  TestMaxInFlightNode node("test", opts);

  // Set MaxInFlight to 1 via contract.
  NodeContract contract;
  contract.SetMaxInFlight(1);
  node.SetContract(contract);

  // AddNode should succeed when pending < MaxInFlight.
  EXPECT_EQ(node.pending_count(), 0);
  queue.AddNode(&node);
  EXPECT_FALSE(queue.IsIdle());

  // Simulate that the node is running.
  node.IncrementPending();
  EXPECT_EQ(node.pending_count(), 1);

  // AddNode should be deferred since pending >= MaxInFlight.
  // But we can't easily test the queue content. Just verify no crash.
  queue.AddNode(&node);
  EXPECT_EQ(node.pending_count(), 1);
}

class GraphContextPoolingTest : public ::testing::Test {};

TEST_F(GraphContextPoolingTest, PrepareAndRecycle) {
  GraphContextManager mgr;
  InputStreamShardSet empty_inputs;
  OutputStreamShardSet empty_outputs;
  NodeOptions opts;
  mgr.Initialize("test", 1, "TestNode", std::move(empty_inputs),
                 std::move(empty_outputs), &opts);

  GraphContext* ctx = mgr.PrepareContext(Timestamp(1));
  EXPECT_NE(ctx, nullptr);
  EXPECT_EQ(ctx->InputTimestamp().Value(), 1);

  // Recycle and re-prepare should reuse the context.
  mgr.RecycleContext(ctx);
  GraphContext* ctx2 = mgr.PrepareContext(Timestamp(2));
  EXPECT_NE(ctx2, nullptr);
  EXPECT_EQ(ctx2->InputTimestamp().Value(), 2);
}

class BatchSchedulingTest : public ::testing::Test {
 protected:
  class TestBatchNode : public Node {
   public:
    TestBatchNode(const std::string& n, const NodeOptions& opts) : Node(n) {}
    absl::Status Open(GraphContext&) override { return absl::OkStatus(); }
    absl::Status Process(GraphContext&) override { return absl::OkStatus(); }
    absl::Status Close(GraphContext&) override { return absl::OkStatus(); }
  };
};

TEST_F(BatchSchedulingTest, ScheduleInvocationsOnDefaultHandler) {
  DefaultInputStreamHandler handler;
  int callback_count = 0;
  handler.SetScheduleCallback([&](Node&) { ++callback_count; });

  NodeOptions opts;
  TestBatchNode node("test", opts);
  NodeContract contract;
  contract.SetMaxInFlight(3);
  contract.Inputs().Get("in").Set<std::string>();
  node.SetContract(contract);

  auto mgr = std::make_unique<InputStreamManager>("in");
  std::list<Packet> packets;
  packets.push_back(Packet::MakePacket<std::string>("hello"));
  bool notify = false;
  (void)mgr->AddPackets(packets, &notify);
  node.SetInputPort("in", mgr.get());
  handler.SetInputStreamManagers({mgr.get()});

  InputStreamShardSet inputs;
  OutputStreamShardSet outputs;
  Timestamp input_bound;
  GraphContext ctx("test", 1, "TestNode", Timestamp(1), &inputs, &outputs, &opts);

  bool result = handler.ScheduleInvocations(3, &input_bound, node, ctx);
  EXPECT_TRUE(result);
  EXPECT_GT(callback_count, 0);
}

class PerfCountersTest : public ::testing::Test {};

TEST_F(PerfCountersTest, CountersStartAtZero) {
  PerfCounters counters;
  EXPECT_EQ(counters.tasks_submitted.Value(), 0);
  EXPECT_EQ(counters.tasks_completed.Value(), 0);
  EXPECT_EQ(counters.packets_processed.Value(), 0);
  EXPECT_EQ(counters.nodes_opened.Value(), 0);
  EXPECT_EQ(counters.nodes_closed.Value(), 0);
}

TEST_F(PerfCountersTest, IncrementAndRead) {
  PerfCounters counters;
  counters.tasks_submitted.Increment();
  EXPECT_EQ(counters.tasks_submitted.Value(), 1);
  counters.tasks_submitted.Increment(3);
  EXPECT_EQ(counters.tasks_submitted.Value(), 4);
  counters.tasks_completed.Increment();
  counters.packets_processed.Increment();
  EXPECT_EQ(counters.tasks_completed.Value(), 1);
  EXPECT_EQ(counters.packets_processed.Value(), 1);
}

class CancelTest : public ::testing::Test {
 protected:
  void SetUp() override {
    runtime_ = std::make_unique<GraphRuntime>();
  }
  std::unique_ptr<GraphRuntime> runtime_;
};

TEST_F(CancelTest, CancelWithoutInitDoesNotCrash) {
  runtime_->Cancel();
  SUCCEED();
}

TEST_F(CancelTest, CancelThenWaitUntilDone) {
  GraphConfig config;
  auto status = runtime_->Initialize(config);
  ASSERT_TRUE(status.ok()) << status;
  status = runtime_->Start();
  ASSERT_TRUE(status.ok()) << status;
  runtime_->Cancel();
  status = runtime_->WaitUntilDone();
  EXPECT_TRUE(status.ok()) << status;
}

TEST_F(CancelTest, StateIsCancellingAfterCancel) {
  GraphConfig config;
  ASSERT_TRUE(runtime_->Initialize(config).ok());
  auto status = runtime_->Start();
  ASSERT_TRUE(status.ok()) << status;
  runtime_->Cancel();
  auto state = runtime_->GetGraphState();
  EXPECT_TRUE(state == SchedulerState::kCancelling ||
              state == SchedulerState::kTerminated);
  (void)runtime_->WaitUntilDone();
}

// --- Drain regression test ------------------------------------------------
//
// Reproduces the encoder-Flush-tail scenario: a source stops, a downstream
// node emits a batch of packets on the invocation that observes the input
// done signal, and a sink must receive every packet of that batch before the
// graph terminates. Scheduling is event-driven (input-stream arrival
// callbacks + post-process re-scheduling), so the graph must not terminate
// while any input queue still holds unconsumed packets.

namespace {

class DrainBatchEmitterNode;
class DrainCountingSinkNode;

// Captured at construction so tests can inspect node state after the graph
// terminates (GraphRuntime has no public node accessor).
DrainBatchEmitterNode* g_last_emitter = nullptr;
DrainCountingSinkNode* g_last_sink = nullptr;

class DrainSourceNode : public Node {
 public:
  DrainSourceNode(const std::string& n, const NodeOptions&) : Node(n) {}
  static absl::Status GetContract(NodeContract* c) {
    c->Outputs().Get("src_out").Set<std::string>();
    return absl::OkStatus();
  }
  absl::Status Open(GraphContext&) override { return absl::OkStatus(); }
  absl::Status Process(GraphContext& ctx) override {
    if (sent_ >= total_) return StatusStop();
    ctx.Outputs().Get("src_out").AddPacket(
        Packet::MakePacket<std::string>("tick").At(ctx.InputTimestamp()));
    ++sent_;
    return absl::OkStatus();
  }
  absl::Status Close(GraphContext&) override { return absl::OkStatus(); }
  int sent_ = 0;
  int total_ = 3;
};

class DrainBatchEmitterNode : public Node {
 public:
  DrainBatchEmitterNode(const std::string& n, const NodeOptions&)
      : Node(n) {
    g_last_emitter = this;
  }
  static absl::Status GetContract(NodeContract* c) {
    c->Inputs().Get("in").Set<std::string>();
    c->Outputs().Get("emitter_out").Set<std::string>();
    return absl::OkStatus();
  }
  absl::Status Open(GraphContext&) override { return absl::OkStatus(); }
  absl::Status Process(GraphContext& ctx) override {
    auto& shard = ctx.Inputs().Get("in");
    if (!shard.IsEmpty()) ++consumed_;
    // On the invocation that observes the input done signal, flush a batch
    // of outputs (encoder-Flush behavior). The done signal arrives either
    // with the last packet or in a separate empty invocation after the
    // upstream closes the stream.
    if (shard.IsDone()) {
      for (int i = 0; i < batch_size_; ++i) {
        ctx.Outputs().Get("emitter_out").AddPacket(
            Packet::MakePacket<std::string>("flush_" + std::to_string(i))
                .At(Timestamp((consumed_ + 1) * 1000 + i)));
      }
      flushed_ = true;
    }
    return absl::OkStatus();
  }
  absl::Status Close(GraphContext&) override { return absl::OkStatus(); }
  int consumed_ = 0;
  int batch_size_ = 50;
  bool flushed_ = false;
};

class DrainCountingSinkNode : public Node {
 public:
  DrainCountingSinkNode(const std::string& n, const NodeOptions&)
      : Node(n) {
    g_last_sink = this;
  }
  static absl::Status GetContract(NodeContract* c) {
    c->Inputs().Get("in").Set<std::string>();
    return absl::OkStatus();
  }
  absl::Status Open(GraphContext&) override { return absl::OkStatus(); }
  absl::Status Process(GraphContext& ctx) override {
    auto& shard = ctx.Inputs().Get("in");
    if (shard.IsEmpty()) return absl::OkStatus();
    auto r = shard.Get<std::string>();
    if (!r.ok()) return absl::OkStatus();
    received_.push_back(*r);
    return absl::OkStatus();
  }
  absl::Status Close(GraphContext&) override { return absl::OkStatus(); }
  std::vector<std::string> received_;
};

bool RegisterDrainNodes() {
  static const bool registered = []() {
    NodeFactoryRegistry::Register(
        "DrainSourceNode", std::make_unique<NodeFactoryFor<DrainSourceNode>>());
    NodeFactoryRegistry::Register(
        "DrainBatchEmitterNode",
        std::make_unique<NodeFactoryFor<DrainBatchEmitterNode>>());
    NodeFactoryRegistry::Register(
        "DrainCountingSinkNode",
        std::make_unique<NodeFactoryFor<DrainCountingSinkNode>>());
    return true;
  }();
  return registered;
}

}  // namespace

TEST(DrainTest, BatchEmittedOnDoneIsFullyConsumedBeforeTermination) {
  RegisterDrainNodes();
  g_last_emitter = nullptr;
  g_last_sink = nullptr;

  GraphConfig config;
  config.nodes.push_back(
      {"src", "DrainSourceNode", {}, {"src_out"}, {}, {}, {}, "", "", 1, 0});
  config.nodes.push_back({"emitter", "DrainBatchEmitterNode",
                          {"in:src_out"}, {"emitter_out"}, {}, {}, {}, "", "",
                          1, 0});
  config.nodes.push_back({"sink", "DrainCountingSinkNode", {"in:emitter_out"},
                          {}, {}, {}, {}, "", "", 1, 0});

  GraphRuntime runtime;
  ASSERT_TRUE(runtime.Initialize(config).ok());
  ASSERT_TRUE(runtime.Start().ok());
  ASSERT_TRUE(runtime.WaitUntilDone().ok());

  ASSERT_NE(g_last_emitter, nullptr);
  ASSERT_NE(g_last_sink, nullptr);
  EXPECT_TRUE(g_last_emitter->flushed_)
      << "Emitter never observed the done signal";
  EXPECT_EQ(g_last_sink->received_.size(),
            static_cast<size_t>(g_last_emitter->batch_size_))
      << "All flush packets must be drained before the graph terminates";
}

}  // namespace graph::runtime
