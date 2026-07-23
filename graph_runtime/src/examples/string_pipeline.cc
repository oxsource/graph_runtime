#include <cctype>
#include <iostream>
#include <string>
#include <vector>

#include "src/stream/packet.h"
#include "src/stream/timestamp.h"
#include "src/node/node.h"
#include "src/node/node_contract.h"
#include "src/node/graph_context.h"

namespace graph::runtime {

class StringProducer : public Node {
 public:
  StringProducer(const std::string& n, const NodeOptions&) : Node(n) {}
  static absl::Status GetContract(NodeContract* c) {
    c->Outputs().Get("output").Set<std::string>(); return absl::OkStatus();
  }
  absl::Status Open(GraphContext&) override { std::cout<<"  [PRODUCER] Open\n"; return {}; }
  absl::Status Process(GraphContext& ctx) override {
    if (sent_ >= total_) { std::cout<<"  [PRODUCER] Done\n"; return StatusStop(); }
    auto payload = "hello_" + std::to_string(sent_);
    auto pkt = Packet::MakePacket<std::string>(payload).At(ctx.InputTimestamp());
    ctx.Outputs().Get("output").AddPacket(std::move(pkt));
    std::cout<<"  [PRODUCER] Sent \""<<payload<<"\"\n";
    ++sent_; return {};
  }
  absl::Status Close(GraphContext&) override {
    std::cout<<"  [PRODUCER] Close, sent="<<sent_<<"\n"; return {};
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
  absl::Status Open(GraphContext&) override { std::cout<<"  [TRANSFORMER] Open\n"; return {}; }
  absl::Status Process(GraphContext& ctx) override {
    auto& shard = ctx.Inputs().Get("input");
    if (shard.IsEmpty()) return {};
    auto r = shard.Get<std::string>(); if (!r.ok()) return {};
    std::string u;
    for (char c : *r) u += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    ctx.Outputs().Get("output").AddPacket(Packet::MakePacket<std::string>(u).At(ctx.InputTimestamp()));
    std::cout<<"  [TRANSFORMER] \""<<*r<<"\" -> \""<<u<<"\"\n";
    return {};
  }
  absl::Status Close(GraphContext&) override { std::cout<<"  [TRANSFORMER] Close\n"; return {}; }
};

class StringConsumer : public Node {
 public:
  StringConsumer(const std::string& n, const NodeOptions&) : Node(n) {}
  static absl::Status GetContract(NodeContract* c) {
    c->Inputs().Get("input").Set<std::string>(); return {};
  }
  absl::Status Open(GraphContext&) override { std::cout<<"  [CONSUMER] Open\n"; return {}; }
  absl::Status Process(GraphContext& ctx) override {
    auto& shard = ctx.Inputs().Get("input");
    if (shard.IsEmpty()) return {};
    auto r = shard.Get<std::string>(); if (!r.ok()) return {};
    result_.push_back(*r);
    std::cout<<"  [CONSUMER] Received \""<<*r<<"\"\n";
    return {};
  }
  absl::Status Close(GraphContext&) override {
    std::cout<<"  [CONSUMER] Close, received="<<result_.size()<<"\n";
    for (auto& s : result_) std::cout<<"    \""<<s<<"\"\n";
    return {};
  }
 private:
  std::vector<std::string> result_;
};

}  // namespace graph::runtime

int main() {
  using namespace graph::runtime;
  std::cout << "=== String Pipeline (RunOnce) ===\n";

  NodeOptions opts;
  auto producer = std::make_unique<StringProducer>("producer", opts);
  auto transformer = std::make_unique<StringUppercase>("transformer", opts);
  auto consumer = std::make_unique<StringConsumer>("consumer", opts);

  InputStreamShardSet dummy_i;
  OutputStreamShardSet dummy_o;

  // Open
  std::cout << "\n--- Open ---\n";
  { GraphContext ctx("p",1,"SP",Timestamp::Unstarted(),&dummy_i,&dummy_o,&opts); producer->Open(ctx); }
  { GraphContext ctx("t",2,"SU",Timestamp::Unstarted(),&dummy_i,&dummy_o,&opts); transformer->Open(ctx); }
  { GraphContext ctx("c",3,"SC",Timestamp::Unstarted(),&dummy_i,&dummy_o,&opts); consumer->Open(ctx); }

  // Process loop
  std::cout << "\n--- Process ---\n";
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
  std::cout << "\n--- Close ---\n";
  { GraphContext ctx("p",1,"SP",Timestamp::Done(),&dummy_i,&dummy_o,&opts); producer->Close(ctx); }
  { GraphContext ctx("t",2,"SU",Timestamp::Done(),&dummy_i,&dummy_o,&opts); transformer->Close(ctx); }
  { GraphContext ctx("c",3,"SC",Timestamp::Done(),&dummy_i,&dummy_o,&opts); consumer->Close(ctx); }

  std::cout << "\n=== Done ===\n";
  return 0;
}
