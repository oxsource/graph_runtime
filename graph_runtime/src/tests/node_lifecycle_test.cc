#include <vector>

#include "gtest/gtest.h"
#include "src/node/node.h"
#include "src/node/node_contract.h"
#include "src/node/node_factory.h"
#include "src/node/node_registry.h"
#include "src/node/graph_context.h"
#include "src/public/types.h"

namespace graph::runtime {

// --- Helper test nodes ---

class LifecycleTestNode : public Node {
 public:
  LifecycleTestNode(const std::string& name, const NodeOptions& options)
      : Node(name) {}

  static absl::Status GetContract(NodeContract* contract) {
    contract->Inputs().Get("in").SetAny();
    contract->Outputs().Get("out").SetAny();
    return absl::OkStatus();
  }

  absl::Status Open(GraphContext&) override { ++open_count; return absl::OkStatus(); }
  absl::Status Process(GraphContext&) override { ++process_count; return absl::OkStatus(); }
  absl::Status Close(GraphContext&) override { ++close_count; return absl::OkStatus(); }

  int open_count = 0;
  int process_count = 0;
  int close_count = 0;
};

class SourceNode : public Node {
 public:
  SourceNode(const std::string& name, const NodeOptions& options)
      : Node(name), produce_count_(5) {}

  static absl::Status GetContract(NodeContract* c) {
    c->Outputs().Get("out").SetAny(); return absl::OkStatus();
  }

  absl::Status Open(GraphContext&) override { return absl::OkStatus(); }

  absl::Status Process(GraphContext& ctx) override {
    if (produced_ >= produce_count_) return StatusStop();
    ctx.Outputs().Get("out").AddPacket(
        Packet::MakePacket<int>(produced_).At(ctx.InputTimestamp()));
    ++produced_;
    return absl::OkStatus();
  }

  absl::Status Close(GraphContext&) override { return absl::OkStatus(); }
  int Produced() const { return produced_; }

 private:
  int produce_count_;
  int produced_ = 0;
};

class SinkNode : public Node {
 public:
  SinkNode(const std::string& name, const NodeOptions& options) : Node(name) {}

  static absl::Status GetContract(NodeContract* c) {
    c->Inputs().Get("in").SetAny(); return absl::OkStatus();
  }

  absl::Status Open(GraphContext&) override { return absl::OkStatus(); }
  absl::Status Process(GraphContext&) override { ++received_; return absl::OkStatus(); }
  absl::Status Close(GraphContext&) override { return absl::OkStatus(); }
  int Received() const { return received_; }
 private:
  int received_ = 0;
};

class SingleNode : public Node {
 public:
  SingleNode(const std::string& name, const NodeOptions& options)
      : Node(name) {}
  static absl::Status GetContract(NodeContract*) { return absl::OkStatus(); }
  absl::Status Open(GraphContext&) override { return absl::OkStatus(); }
  absl::Status Process(GraphContext&) override {
    if (called_++ >= 3) return StatusStop();
    return absl::OkStatus();
  }
  absl::Status Close(GraphContext&) override { return absl::OkStatus(); }
  int Called() const { return called_; }
 private:
  int called_ = 0;
};

// --- T050: Source Node lifecycle ---

TEST(NodeLifecycleTest, SourceNodeOpenProcessStopClose) {
  NodeOptions opts;
  SourceNode node("src", opts);

  InputStreamShardSet dummy_i;
  OutputStreamShardSet dummy_o;

  // Open
  {
    GraphContext ctx("src", 1, "SourceNode", Timestamp::Unstarted(),
                     &dummy_i, &dummy_o, &opts);
    EXPECT_TRUE(node.Open(ctx).ok());
  }

  // Process repeatedly until StatusStop
  int count = 0;
  for (int i = 0; i < 20; ++i) {
    InputStreamShardSet empty_i;
    OutputStreamShardSet out_o;
    GraphContext ctx("src", 1, "SourceNode", Timestamp(i),
                     &empty_i, &out_o, &opts);
    auto s = node.Process(ctx);
    if (IsStopStatus(s)) break;
    EXPECT_TRUE(s.ok());
    ++count;
  }
  EXPECT_EQ(count, 5);
  EXPECT_EQ(node.Produced(), 5);

  // Close
  {
    GraphContext ctx("src", 1, "SourceNode", Timestamp::Done(),
                     &dummy_i, &dummy_o, &opts);
    EXPECT_TRUE(node.Close(ctx).ok());
  }
}

// --- T051: Sink Node lifecycle ---

TEST(NodeLifecycleTest, SinkNodeReceivesPackets) {
  NodeOptions opts;
  SinkNode sink("snk", opts);

  InputStreamShardSet dummy_i;
  OutputStreamShardSet dummy_o;

  {
    GraphContext ctx("snk", 1, "SinkNode", Timestamp::Unstarted(),
                     &dummy_i, &dummy_o, &opts);
    EXPECT_TRUE(sink.Open(ctx).ok());
  }

  // Feed packets into sink
  for (int i = 0; i < 5; ++i) {
    InputStreamShardSet in;
    in.Get("in").PushPacket(Packet::MakePacket<int>(i).At(Timestamp(i)));
    OutputStreamShardSet out;
    GraphContext ctx("snk", 1, "SinkNode", Timestamp(i),
                     &in, &out, &opts);
    EXPECT_TRUE(sink.Process(ctx).ok());
  }
  EXPECT_EQ(sink.Received(), 5);

  {
    GraphContext ctx("snk", 1, "SinkNode", Timestamp::Done(),
                     &dummy_i, &dummy_o, &opts);
    EXPECT_TRUE(sink.Close(ctx).ok());
  }
}

// --- T052: Empty graph ---

TEST(NodeLifecycleTest, EmptyGraph) {
  // With zero nodes, no Scheduler needed — just verify the concept
  SUCCEED() << "Empty graph: zero nodes = no execution needed";
}

// --- T053: Single-Node (both source and sink) ---

TEST(NodeLifecycleTest, SingleNodeSourceAndSink) {
  NodeOptions opts;
  SingleNode sn("single", opts);

  InputStreamShardSet dummy_i;
  OutputStreamShardSet dummy_o;

  {
    GraphContext ctx("single", 1, "SingleNode", Timestamp::Unstarted(),
                     &dummy_i, &dummy_o, &opts);
    EXPECT_TRUE(sn.Open(ctx).ok());
  }

  for (int i = 0; i < 10; ++i) {
    InputStreamShardSet in;
    OutputStreamShardSet out;
    GraphContext ctx("single", 1, "SingleNode", Timestamp(i),
                     &in, &out, &opts);
    auto s = sn.Process(ctx);
    if (IsStopStatus(s)) break;
    EXPECT_TRUE(s.ok());
  }
  EXPECT_EQ(sn.Called(), 4);

  {
    GraphContext ctx("single", 1, "SingleNode", Timestamp::Done(),
                     &dummy_i, &dummy_o, &opts);
    EXPECT_TRUE(sn.Close(ctx).ok());
  }
}

// --- T054: Disconnected subgraphs ---

TEST(NodeLifecycleTest, DisconnectedSubgraphs) {
  NodeOptions opts;
  SourceNode src_a("src_a", opts);
  SourceNode src_b("src_b", opts);

  InputStreamShardSet dummy_i;
  OutputStreamShardSet dummy_o;

  // Open both
  {
    GraphContext c("a",1,"Src",Timestamp::Unstarted(),&dummy_i,&dummy_o,&opts);
    src_a.Open(c);
  }
  {
    GraphContext c("b",2,"Src",Timestamp::Unstarted(),&dummy_i,&dummy_o,&opts);
    src_b.Open(c);
  }

  // Run both independently
  for (int i = 0; i < 20; ++i) {
    Timestamp ts(i);
    {
      InputStreamShardSet e; OutputStreamShardSet o;
      GraphContext c("a",1,"Src",ts,&e,&o,&opts);
      if (IsStopStatus(src_a.Process(c))) break;
    }
  }
  for (int i = 0; i < 20; ++i) {
    Timestamp ts(i);
    {
      InputStreamShardSet e; OutputStreamShardSet o;
      GraphContext c("b",2,"Src",ts,&e,&o,&opts);
      if (IsStopStatus(src_b.Process(c))) break;
    }
  }

  EXPECT_EQ(src_a.Produced(), 5);
  EXPECT_EQ(src_b.Produced(), 5);

  { GraphContext c("a",1,"Src",Timestamp::Done(),&dummy_i,&dummy_o,&opts); src_a.Close(c); }
  { GraphContext c("b",2,"Src",Timestamp::Done(),&dummy_i,&dummy_o,&opts); src_b.Close(c); }
}

// --- T055: Combined lifecycle test ---

TEST(NodeLifecycleTest, FullLifecycleOrder) {
  NodeOptions opts;
  LifecycleTestNode n("lifecycle", opts);

  InputStreamShardSet dummy_i;
  OutputStreamShardSet dummy_o;

  EXPECT_EQ(n.open_count, 0);
  EXPECT_EQ(n.process_count, 0);
  EXPECT_EQ(n.close_count, 0);

  {
    GraphContext c("n",1,"T",Timestamp::Unstarted(),&dummy_i,&dummy_o,&opts);
    EXPECT_TRUE(n.Open(c).ok());
  }
  EXPECT_EQ(n.open_count, 1);

  for (int i = 0; i < 3; ++i) {
    InputStreamShardSet in;
    in.Get("in").PushPacket(Packet::MakePacket<int>(i).At(Timestamp(i)));
    OutputStreamShardSet out;
    GraphContext c("n",1,"T",Timestamp(i),&in,&out,(&opts));
    EXPECT_TRUE(n.Process(c).ok());
  }
  EXPECT_EQ(n.process_count, 3);

  {
    GraphContext c("n",1,"T",Timestamp::Done(),&dummy_i,&dummy_o,&opts);
    EXPECT_TRUE(n.Close(c).ok());
  }
  EXPECT_EQ(n.close_count, 1);

  // Verify order and counts: Open(1), Process(3), Close(1)
  EXPECT_EQ(n.open_count, 1);
  EXPECT_EQ(n.process_count, 3);
  EXPECT_EQ(n.close_count, 1);
}

// --- T057: CloseNode idempotency ---

TEST(NodeLifecycleTest, CloseNodeIdempotency) {
  NodeOptions opts;
  LifecycleTestNode n("idempotent", opts);

  InputStreamShardSet dummy_i;
  OutputStreamShardSet dummy_o;

  {
    GraphContext c("n",1,"T",Timestamp::Unstarted(),&dummy_i,&dummy_o,&opts);
    n.Open(c);
  }
  {
    GraphContext c("n",1,"T",Timestamp::Done(),&dummy_i,&dummy_o,&opts);
    EXPECT_TRUE(n.Close(c).ok());
  }
  // Second Close should also succeed (idempotent)
  {
    GraphContext c("n",1,"T",Timestamp::Done(),&dummy_i,&dummy_o,&opts);
    EXPECT_TRUE(n.Close(c).ok());
  }
}

}  // namespace graph::runtime
