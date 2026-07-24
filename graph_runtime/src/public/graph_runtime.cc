#include "src/public/graph_runtime.h"
#include "src/public/graph_builder.h"

#include "absl/strings/str_cat.h"
#include "src/hook/factory.h"
#include "src/scheduler/scheduler.h"
#include "src/scheduler/input_stream_handler.h"
#include "src/stream/input_stream_manager.h"
#include "src/stream/output_stream_manager.h"

namespace graph::runtime {

GraphRuntime::GraphRuntime()
    : scheduler_(std::make_unique<Scheduler>()) {}

GraphRuntime::~GraphRuntime() = default;

absl::Status GraphRuntime::Initialize(const GraphConfig& config) {
  config_ = config;
  auto result = GraphBuilder::Build(config);
  if (!result.ok()) return result.status();
  auto built = std::move(*result);
  scheduler_ = std::move(built->scheduler_);
  all_nodes_ = std::move(built->all_nodes_);

  std::vector<Node*> node_ptrs;
  for (auto& node : all_nodes_) {
    node_ptrs.push_back(node.get());
  }
  scheduler_->SetNodes(node_ptrs);

  // Build input stream manager map for AddPacketToInputStream lookup.
  // Each input stream declared in the config maps to its node's InputStreamManager.
  for (const auto& ndef : config_.nodes) {
    for (const auto& input_stream : ndef.input_streams) {
      if (stream_managers_.find(input_stream) == stream_managers_.end()) {
        auto* node = FindNode(ndef.name);
        if (node) {
          auto* mgr = node->GetInputPort(input_stream);
          if (mgr) {
            stream_managers_[input_stream] = mgr;
            if (graph_input_streams_set_.find(input_stream) ==
                graph_input_streams_set_.end()) {
              graph_input_streams_set_.insert(input_stream);
              ++num_open_input_streams_;
            }
          }
        }
      }
    }
  }

  scheduler_->SetTotalGraphInputStreams(num_open_input_streams_);
  return absl::OkStatus();
}

absl::Status GraphRuntime::Start() {
  if (!scheduler_) {
    return absl::FailedPreconditionError("Graph not initialized");
  }
  return scheduler_->Start();
}

absl::Status GraphRuntime::WaitUntilDone() {
  if (!scheduler_) return absl::OkStatus();
  return scheduler_->WaitUntilDone();
}

void GraphRuntime::Shutdown() {
  if (scheduler_) {
    scheduler_->Shutdown();
  }
}

absl::Status GraphRuntime::AddPacketToInputStream(
    const std::string& stream_name, Packet packet) {
  auto it = stream_managers_.find(stream_name);
  if (it == stream_managers_.end()) {
    return absl::NotFoundError(
        absl::StrCat("Unknown input stream: ", stream_name));
  }

  std::list<Packet> packets;
  packets.push_back(std::move(packet));
  bool notify = false;
  it->second->AddPackets(packets, &notify);
  scheduler_->AddedPacketToGraphInputStream();
  return absl::OkStatus();
}

absl::Status GraphRuntime::CloseInputStream(
    const std::string& stream_name) {
  auto it = stream_managers_.find(stream_name);
  if (it == stream_managers_.end()) {
    return absl::NotFoundError(
        absl::StrCat("Unknown input stream: ", stream_name));
  }

  it->second->Close();
  scheduler_->IncClosedGraphInputStreams();
  scheduler_->AddedPacketToGraphInputStream();
  return absl::OkStatus();
}

void GraphRuntime::SetOutputStreamCallback(
    const std::string& stream_name,
    std::function<void(const Packet&)> callback) {}

void GraphRuntime::ClearOutputStreamCallback(
    const std::string& stream_name) {}

absl::Status GraphRuntime::SetInputSidePacket(
    const std::string& tag_name, Packet packet) {
  return absl::OkStatus();
}

void GraphRuntime::SetOutputSidePacketCallback(
    const std::string& name,
    std::function<void(const Packet&)> callback) {}

void GraphRuntime::SetHook(int type, hook::HookFn fn) {
  hook::HookFactory::Register(type, fn);
}

Node* GraphRuntime::FindNode(const std::string& name) {
  for (auto& node : all_nodes_) {
    if (node->name() == name) return node.get();
  }
  return nullptr;
}

}  // namespace graph::runtime
