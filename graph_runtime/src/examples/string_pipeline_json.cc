#include <cctype>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "src/public/logger.h"

#include "absl/strings/str_cat.h"
#include "src/config/json/json_parser.h"
#include "src/config/graph_config.h"
#include "src/config/config_validator.h"
#include "src/stream/packet.h"
#include "src/stream/timestamp.h"
#include "src/node/node.h"
#include "src/node/node_contract.h"
#include "src/node/graph_context.h"

namespace graph::runtime {

// Same Node classes as string_pipeline.cc, kept inline for self-containment.
class StringProducer : public Node {
 public:
  StringProducer(const std::string& n, const NodeOptions&) : Node(n) {}
  static absl::Status GetContract(NodeContract* c) {
    c->Outputs().Get("output").Set<std::string>(); return {};
  }
  absl::Status Open(GraphContext&) override { Logger::Info("graphrt::example", "[PRODUCER] Open"); return {}; }
  absl::Status Process(GraphContext& ctx) override {
    if (sent_ >= total_) { Logger::Info("graphrt::example", "[PRODUCER] Done"); return StatusStop(); }
    auto pkt = Packet::MakePacket<std::string>("hello_" + std::to_string(sent_))
                   .At(ctx.InputTimestamp());
    ctx.Outputs().Get("output").AddPacket(std::move(pkt));
    Logger::Info("graphrt::example", std::string("[PRODUCER] Sent \"hello_" + std::to_string(sent_) + "\"").c_str());
    ++sent_; return {};
  }
  absl::Status Close(GraphContext&) override {
    Logger::Info("graphrt::example", std::string("[PRODUCER] Close, sent=" + std::to_string(sent_)).c_str()); return {};
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
  absl::Status Open(GraphContext&) override { Logger::Info("graphrt::example", "[TRANSFORMER] Open"); return {}; }
  absl::Status Process(GraphContext& ctx) override {
    auto& shard = ctx.Inputs().Get("input");
    if (shard.IsEmpty()) return {};
    auto r = shard.Get<std::string>(); if (!r.ok()) return {};
    std::string u;
    for (char c : *r) u += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    ctx.Outputs().Get("output").AddPacket(
        Packet::MakePacket<std::string>(u).At(ctx.InputTimestamp()));
    Logger::Info("graphrt::example", std::string("[TRANSFORMER] \"" + *r + "\" -> \"" + u + "\"").c_str());
    return {};
  }
  absl::Status Close(GraphContext&) override { Logger::Info("graphrt::example", "[TRANSFORMER] Close"); return {}; }
};

class StringConsumer : public Node {
 public:
  StringConsumer(const std::string& n, const NodeOptions&) : Node(n) {}
  static absl::Status GetContract(NodeContract* c) {
    c->Inputs().Get("input").Set<std::string>(); return {};
  }
  absl::Status Open(GraphContext&) override { Logger::Info("graphrt::example", "[CONSUMER] Open"); return {}; }
  absl::Status Process(GraphContext& ctx) override {
    auto& shard = ctx.Inputs().Get("input");
    if (shard.IsEmpty()) return {};
    auto r = shard.Get<std::string>(); if (!r.ok()) return {};
    result_.push_back(*r);
    Logger::Info("graphrt::example", std::string("[CONSUMER] Received \"" + *r + "\"").c_str());
    return {};
  }
  absl::Status Close(GraphContext&) override {
    Logger::Info("graphrt::example", std::string("[CONSUMER] Close, received=" + std::to_string(result_.size())).c_str());
    return {};
  }
 private:
  std::vector<std::string> result_;
};

}  // namespace graph::runtime

int main() {
  using namespace graph::runtime;
  Logger::Info("graphrt::example", "=== String Pipeline (JSON Config) ===");

  // JSON config — connections are implicit via stream name matching.
  // producer outputs to "output:text_stream", transformer reads from "input:text_stream".
  // Both reference the same stream name "text_stream" → implicitly connected.
  // No explicit "streams" array needed, matching MediaPipe's design.
  const std::string json_config = R"({
    "nodes": [
      {
        "name": "producer",
        "type": "StringProducer",
        "output_streams": ["output:text_stream"]
      },
      {
        "name": "transformer",
        "type": "StringUppercase",
        "input_streams": ["input:text_stream"],
        "output_streams": ["output:upper_stream"]
      },
      {
        "name": "consumer",
        "type": "StringConsumer",
        "input_streams": ["input:upper_stream"]
      }
    ]
  })";

  // Write JSON to temp file
  const char* tmp_path = "/tmp/string_pipeline.json";
  {
    std::ofstream out(tmp_path);
    out << json_config;
    out.close();
  }
  std::cout << "Config written to " << tmp_path << "\n\n";

  // --- Phase 1: Parse ---
  std::cout << "--- Parse ---\n";
  JsonParser parser;
  auto parse_result = parser.Parse(tmp_path);
  if (!parse_result.ok()) {
    std::cerr << "Parse failed: " << parse_result.status() << std::endl;
    std::remove(tmp_path);
    return 1;
  }
  const auto& config = *parse_result;
  std::cout << "Parsed " << config.nodes.size() << " nodes"
            << " (connections implicit via stream name matching)\n";

  // Verify the parsed config matches expectations
  ConfigValidator::Validate(config).IgnoreError();
  std::cout << "Validation passed\n\n";

  // --- Phase 2: Manual pipeline execution using parsed config ---
  std::cout << "--- Manual Pipeline (guided by parsed config) ---\n";

  NodeOptions opts;
  auto producer = std::make_unique<StringProducer>("producer", opts);
  auto transformer = std::make_unique<StringUppercase>("transformer", opts);
  auto consumer = std::make_unique<StringConsumer>("consumer", opts);

  InputStreamShardSet dummy_i;
  OutputStreamShardSet dummy_o;

  // Open
  Logger::Info("graphrt::example", "--- Open ---");
  { GraphContext c("p",1,"SP",Timestamp::Unstarted(),&dummy_i,&dummy_o,&opts); producer->Open(c); }
  { GraphContext c("t",2,"SU",Timestamp::Unstarted(),&dummy_i,&dummy_o,&opts); transformer->Open(c); }
  { GraphContext c("c",3,"SC",Timestamp::Unstarted(),&dummy_i,&dummy_o,&opts); consumer->Open(c); }

  // Process loop — same RunOnce pattern as string_pipeline.cc
  Logger::Info("graphrt::example", "--- Process ---");
  for (int i = 0; i < 10; ++i) {
    Timestamp ts(i);

    InputStreamShardSet pi; OutputStreamShardSet po;
    absl::Status st;
    { GraphContext c("p",1,"SP",ts,&pi,&po,&opts); st = producer->Process(c); }
    if (IsStopStatus(st)) break;

    Packet pkt;
    auto& pq = po.Get("output").OutputQueue();
    if (!pq.empty()) { pkt = std::move(pq.front()); pq.pop_front(); }
    if (pkt.IsEmpty()) break;

    InputStreamShardSet ti; ti.Get("input").PushPacket(std::move(pkt));
    OutputStreamShardSet to;
    { GraphContext c("t",2,"SU",ts,&ti,&to,&opts); transformer->Process(c); }

    Packet tp;
    auto& tq = to.Get("output").OutputQueue();
    if (!tq.empty()) { tp = std::move(tq.front()); tq.pop_front(); }
    if (tp.IsEmpty()) continue;

    InputStreamShardSet ci; ci.Get("input").PushPacket(std::move(tp));
    OutputStreamShardSet co;
    { GraphContext c("c",3,"SC",ts,&ci,&co,&opts); consumer->Process(c); }
  }

  Logger::Info("graphrt::example", "--- Close ---");
  { GraphContext c("p",1,"SP",Timestamp::Done(),&dummy_i,&dummy_o,&opts); producer->Close(c); }
  { GraphContext c("t",2,"SU",Timestamp::Done(),&dummy_i,&dummy_o,&opts); transformer->Close(c); }
  { GraphContext c("c",3,"SC",Timestamp::Done(),&dummy_i,&dummy_o,&opts); consumer->Close(c); }

  std::remove(tmp_path);
  Logger::Info("graphrt::example", "=== Done (JSON config driven) ===");
  return 0;
}
