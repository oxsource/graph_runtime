#include "src/framework/public/graph_runtime.h"
#include "src/framework/public/graph_builder.h"

#include <algorithm>

#include "absl/strings/str_cat.h"
#include "src/framework/utils/factory.h"
#include "src/framework/scheduler/scheduler.h"
#include "src/framework/scheduler/input_stream_handler.h"
#include "src/framework/stream/input_stream_manager.h"
#include "src/framework/stream/output_stream_manager.h"

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

  // Create InputStreamManagers for each declared input stream and register
  // them on their owning node. This enables AddPacketToInputStream to find
  // the correct queue by stream name.
  for (const auto& ndef : config_.nodes) {
    for (const auto& input_stream : ndef.input_streams) {
      if (stream_managers_.find(input_stream) != stream_managers_.end()) continue;

      auto* node = FindNode(ndef.name);
      if (!node) continue;

      auto mgr = std::make_unique<InputStreamManager>(input_stream);
      auto* raw = mgr.get();
      node->SetInputPort(input_stream, raw);
      owned_stream_managers_.push_back(std::move(mgr));
      stream_managers_[input_stream] = raw;

      if (graph_input_streams_set_.find(input_stream) ==
          graph_input_streams_set_.end()) {
        graph_input_streams_set_.insert(input_stream);
        ++num_open_input_streams_;
      }
    }
  }

  scheduler_->SetTotalGraphInputStreams(num_open_input_streams_);

  // Wire backpressure callbacks on each input stream.
  for (const auto& ndef : config_.nodes) {
    for (const auto& input_stream : ndef.input_streams) {
      auto it = stream_managers_.find(input_stream);
      if (it != stream_managers_.end()) {
        auto* mgr = it->second;
        mgr->SetQueueSizeCallbacks(
            [this](InputStreamManager* m, bool*) { OnInputStreamFull(m); },
            [this](InputStreamManager* m, bool*) { OnInputStreamNotFull(m); });
      }
    }
  }

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

  // Throttle check: if queue is full, reject
  if (it->second->IsFull()) {
    return absl::UnavailableError(
        absl::StrCat("Input stream is full: ", stream_name));
  }

  std::list<Packet> packets;
  packets.push_back(std::move(packet));
  bool notify = false;
  absl::Status st = it->second->AddPackets(packets, &notify);
  if (!st.ok()) return st;

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

  // Idempotency: skip if already closed
  if (closed_streams_.count(stream_name)) {
    return absl::OkStatus();
  }

  it->second->Close();
  closed_streams_.insert(stream_name);
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

void GraphRuntime::OnInputStreamFull(InputStreamManager* mgr) {
  for (auto& node : all_nodes_) {
    for (auto& [port_name, port_mgr] : node->InputPorts()) {
      if (port_mgr == mgr) {
        full_input_streams_[node.get()].insert(mgr);
        return;
      }
    }
  }
}

void GraphRuntime::OnInputStreamNotFull(InputStreamManager* mgr) {
  for (auto& [node, streams] : full_input_streams_) {
    streams.erase(mgr);
  }
  scheduler_->HandleIdle();
}

void GraphRuntime::UnthrottleSources() {
  for (auto& [node, streams] : full_input_streams_) {
    for (auto* mgr : streams) {
      int current = mgr->MaxQueueSize();
      mgr->SetMaxQueueSize(std::max(1, current + 1));
    }
  }
  full_input_streams_.clear();
}

bool GraphRuntime::IsNodeThrottled(Node* node) const {
  auto it = full_input_streams_.find(node);
  return it != full_input_streams_.end() && !it->second.empty();
}

}  // namespace graph::runtime
