#include <cctype>
#include <string>
#include <vector>

#define GRAPHRT_LOG_TAG "graphrt::example"
#include "src/framework/utils/logger.h"
#include "src/framework/stream/packet.h"
#include "src/framework/stream/timestamp.h"
#include "src/framework/node/node.h"
#include "src/framework/node/node_contract.h"
#include "src/framework/node/graph_context.h"

namespace graph::runtime {

class StringProducer : public Node {
 public:
  StringProducer(const std::string& n, const NodeOptions&) : Node(n) {}
  static absl::Status GetContract(NodeContract* c) {
    c->Outputs().Get("output").Set<std::string>(); return absl::OkStatus();
  }
  absl::Status Open(GraphContext&) override { Logger::Info("[PRODUCER] Open"); return {}; }
  absl::Status Process(GraphContext& ctx) override {
    if (sent_ >= total_) { Logger::Info("[PRODUCER] Done"); return StatusStop(); }
    auto payload = "hello_" + std::to_string(sent_);
    auto pkt = Packet::MakePacket<std::string>(payload).At(ctx.InputTimestamp());
    ctx.Outputs().Get("output").AddPacket(std::move(pkt));
    Logger::Info(std::string("[PRODUCER] Sent \"" + payload + "\"").c_str());
    ++sent_; return {};
  }
  absl::Status Close(GraphContext&) override {
    Logger::Info(std::string("[PRODUCER] Close, sent=" + std::to_string(sent_)).c_str()); return {};
  }
 private:
  int sent_ = 0, total_ = 5;
};

class StringUppercase : public Node {
 public:
  StringUppercase(const std::string& n, const NodeOptions&) : Node(n) {}
  static absl::Status GetContract(NodeContract* c) {
    c->Inputs().Get("input").Set<std::string>();
    c->Outputs().Get("output").Set<std::string>(); return {};
  }
  absl::Status Open(GraphContext&) override { Logger::Info("[TRANSFORMER] Open"); return {}; }
  absl::Status Process(GraphContext& ctx) override {
    auto& shard = ctx.Inputs().Get("input");
    if (shard.IsEmpty()) return {};
    auto r = shard.Get<std::string>(); if (!r.ok()) return {};
    std::string u;
    for (char c : *r) u += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    ctx.Outputs().Get("output").AddPacket(Packet::MakePacket<std::string>(u).At(ctx.InputTimestamp()));
    Logger::Info(std::string("[TRANSFORMER] \"" + *r + "\" -> \"" + u + "\"").c_str());
    return {};
  }
  absl::Status Close(GraphContext&) override { Logger::Info("[TRANSFORMER] Close"); return {}; }
};

class StringConsumer : public Node {
 public:
  StringConsumer(const std::string& n, const NodeOptions&) : Node(n) {}
  static absl::Status GetContract(NodeContract* c) {
    c->Inputs().Get("input").Set<std::string>(); return {};
  }
  absl::Status Open(GraphContext&) override { Logger::Info("[CONSUMER] Open"); return {}; }
  absl::Status Process(GraphContext& ctx) override {
    auto& shard = ctx.Inputs().Get("input");
    if (shard.IsEmpty()) return {};
    auto r = shard.Get<std::string>(); if (!r.ok()) return {};
    result_.push_back(*r);
    Logger::Info(std::string("[CONSUMER] Received \"" + *r + "\"").c_str());
    return {};
  }
  absl::Status Close(GraphContext&) override {
    Logger::Info(std::string("[CONSUMER] Close, received=" + std::to_string(result_.size())).c_str());
    return {};
  }
 private:
  std::vector<std::string> result_;
};

}  // namespace graph::runtime

int main() {
  using namespace graph::runtime;
  Logger::Info("=== String Pipeline (RunOnce) ===");

  NodeOptions opts;
  auto producer = std::make_unique<StringProducer>("producer", opts);
  auto transformer = std::make_unique<StringUppercase>("transformer", opts);
  auto consumer = std::make_unique<StringConsumer>("consumer", opts);

  InputStreamShardSet dummy_i;
  OutputStreamShardSet dummy_o;

  // Open
  Logger::Info("--- Open ---");
  { GraphContext ctx("p",1,"SP",Timestamp::Unstarted(),&dummy_i,&dummy_o,&opts); producer->Open(ctx); }
  { GraphContext ctx("t",2,"SU",Timestamp::Unstarted(),&dummy_i,&dummy_o,&opts); transformer->Open(ctx); }
  { GraphContext ctx("c",3,"SC",Timestamp::Unstarted(),&dummy_i,&dummy_o,&opts); consumer->Open(ctx); }

  // Process loop
  Logger::Info("--- Process ---");
  for (int i = 0; i < 10; ++i) {
    Timestamp ts(i);

    // Producer
    InputStreamShardSet pi; OutputStreamShardSet po;
    absl::Status status;
    { GraphContext ctx("p",1,"SP",ts,&pi,&po,&opts); status = producer->Process(ctx); }
    if (IsStopStatus(status)) break;

    Packet pkt;
    auto& pq = po.Get("output").OutputQueue();
    if (!pq.empty()) { pkt = std::move(pq.front()); pq.pop_front(); }
    if (pkt.IsEmpty()) break;

    // Transformer
    InputStreamShardSet ti; ti.Get("input").PushPacket(std::move(pkt));
    OutputStreamShardSet to;
    { GraphContext ctx("t",2,"SU",ts,&ti,&to,&opts); transformer->Process(ctx); }

    Packet tp;
    auto& tq = to.Get("output").OutputQueue();
    if (!tq.empty()) { tp = std::move(tq.front()); tq.pop_front(); }
    if (tp.IsEmpty()) continue;

    // Consumer
    InputStreamShardSet ci; ci.Get("input").PushPacket(std::move(tp));
    OutputStreamShardSet co;
    { GraphContext ctx("c",3,"SC",ts,&ci,&co,&opts); consumer->Process(ctx); }
  }

  // Close
  Logger::Info("--- Close ---");
  { GraphContext ctx("p",1,"SP",Timestamp::Done(),&dummy_i,&dummy_o,&opts); producer->Close(ctx); }
  { GraphContext ctx("t",2,"SU",Timestamp::Done(),&dummy_i,&dummy_o,&opts); transformer->Close(ctx); }
  { GraphContext ctx("c",3,"SC",Timestamp::Done(),&dummy_i,&dummy_o,&opts); consumer->Close(ctx); }

  Logger::Info("=== Done ===");
  return 0;
}
