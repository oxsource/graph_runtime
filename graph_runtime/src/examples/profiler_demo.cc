// profiler_demo.cc
// Demonstrates the complete profiler workflow:
//   1. Build a graph with a node that has measurable workload
//   2. Enable profiling via ProfilerConfig
//   3. Execute the graph via GraphRuntime::Schedule() (sync path)
//   4. Query per-node timing data via GetNodeProfiles()
//   5. Persist profiles to JSON via WriteProfile()
//   6. Show how to analyze with the print_profile CLI tool
//
// Build (stub, no-op profiler):
//   bazel build //src/examples:profiler_demo
//
// Build (real profiler):
//   bazel build //src/examples:profiler_demo --define graph_runtime_profiler=true
//
// Run:
//   bazel run //src/examples:profiler_demo --define graph_runtime_profiler=true
//
// Analyze with CLI:
//   print_profile --files=/tmp/profiler_demo_profile.json
//   print_profile --files=/tmp/profiler_demo_profile.json --format=csv

#include <chrono>
#include <cstdio>
#include <thread>

#include "src/framework/public/graph_runtime.h"
#include "src/framework/node/node.h"
#include "src/framework/node/node_contract.h"
#include "src/framework/node/node_registry.h"
#include "src/framework/node/graph_context.h"
#include "src/framework/utils/logger.h"

namespace graph::runtime {

// A source node that simulates work by sleeping ~5ms per Process() call.
// It emits 5 packets and then returns StatusStop() to signal completion.
class ProfiledWorkNode : public Node {
 public:
  ProfiledWorkNode(const std::string& n, const NodeOptions&) : Node(n) {}
  static absl::Status GetContract(NodeContract* c) {
    c->Outputs().Get("out").Set<std::string>();
    return absl::OkStatus();
  }
  absl::Status Open(GraphContext&) override {
    Logger::Info("[WorkNode] Open");
    return absl::OkStatus();
  }
  absl::Status Process(GraphContext& ctx) override {
    if (sent_ >= total_) {
      Logger::Info("[WorkNode] Done");
      return StatusStop();
    }
    // Simulate compute workload: ~5ms busy wait
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now() - start)
               .count() < 5000) {
    }
    auto payload = "packet_" + std::to_string(sent_);
    auto pkt = Packet::MakePacket<std::string>(payload).At(ctx.InputTimestamp());
    ctx.Outputs().Get("out").AddPacket(std::move(pkt));
    ++sent_;
    return absl::OkStatus();
  }
  absl::Status Close(GraphContext&) override {
    Logger::Info(std::string("[WorkNode] Close, sent=" + std::to_string(sent_)).c_str());
    return absl::OkStatus();
  }

 private:
  int sent_ = 0;
  int total_ = 5;
};

}  // namespace graph::runtime

namespace { using ProfiledWorkNode = graph::runtime::ProfiledWorkNode; }
GRAPH_RUNTIME_REGISTER_NODE("ProfiledWorkNode", ProfiledWorkNode);

int main() {
  using namespace graph::runtime;

  printf("=== Profiler Demo ===\n\n");

  // Build config with profiling enabled
  GraphConfig config;
  ProfilerConfig pcfg;
  pcfg.enable_profiler = true;
  pcfg.histogram_interval_size_usec = 100000;
  pcfg.num_histogram_intervals = 5;
  config.profiler_config = pcfg;
  GraphConfig::NodeDef node_def;
  node_def.name = "work";
  node_def.type = "ProfiledWorkNode";
  node_def.output_streams = {"work:out"};
  config.nodes.push_back(std::move(node_def));

  printf("Config: %zu node(s), profiler enabled=%s\n",
         config.nodes.size(),
         config.profiler_config.enable_profiler ? "true" : "false");

  GraphRuntime runtime;
  auto status = runtime.Initialize(config);
  if (!status.ok()) {
    printf("Initialize failed: %s\n", std::string(status.ToString()).c_str());
    return 1;
  }

  // Execute synchronously
  printf("Running graph (sync mode)...\n");
  status = runtime.Schedule();
  if (!status.ok()) {
    printf("Schedule failed: %s\n", std::string(status.ToString()).c_str());
    return 1;
  }
  printf("Graph completed.\n\n");

  // Query and display profile results
  auto profiles = runtime.GetNodeProfiles();
  printf("=== Profile Results ===\n");
  if (profiles.empty()) {
    printf("(No profile data — profiler is disabled or no nodes were instrumented)\n");
    printf("Build with: --define graph_runtime_profiler=true\n\n");
  } else {
    for (const auto& p : profiles) {
      printf("Node: %s\n", p.node_name.c_str());
      printf("  Process calls: %lld\n", static_cast<long long>(p.process_count));
      printf("  Process total: %lld us\n", static_cast<long long>(p.process_time_total_usec));
      printf("  Process mean:  %.2f us\n", p.process_time_mean_usec);
      printf("  Open:          %lld us\n", static_cast<long long>(p.open_runtime_usec));
      printf("  Close:         %lld us\n", static_cast<long long>(p.close_runtime_usec));
    }
  }

  // Persist to JSON
  const char* profile_path = "/tmp/profiler_demo_profile.json";
  auto ws = runtime.WriteProfile(profile_path);
  if (ws.ok()) {
    printf("\nProfile saved to: %s\n", profile_path);
  } else {
    printf("\nWriteProfile: %s\n", std::string(ws.ToString()).c_str());
  }

  // Show CLI usage
  printf("\n=== CLI Analysis ===\n");
  printf("Table output:\n");
  printf("  print_profile --files=%s\n", profile_path);
  printf("\nCSV output:\n");
  printf("  print_profile --files=%s --format=csv\n", profile_path);
  printf("\n=== Done ===\n");
  return 0;
}
