#include "src/framework/public/graph_runtime.h"
#include "src/framework/public/graph_builder.h"

#include <algorithm>

#include "absl/strings/str_cat.h"
#include "graph_runtime/profiler.h"
#include "src/framework/utils/hook.h"
#include "src/framework/scheduler/scheduler.h"
#include "src/framework/scheduler/input_stream_handler.h"
#include "src/framework/stream/input_stream_manager.h"
#include "src/framework/stream/output_stream_manager.h"
#include "src/framework/stream/output_stream_handler.h"

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
  std::vector<std::string> node_names;
  for (auto& node : all_nodes_) {
    node_ptrs.push_back(node.get());
    node_names.push_back(node->name());
  }
  scheduler_->SetNodes(node_ptrs);

  // Initialize profiler
  const ProfilerConfig& pcfg = config_.profiler_config;
  profiler_ = std::make_unique<ProfilingContext>();
  profiler_->SetClock(std::make_shared<RealClock>());
  profiler_->Initialize(pcfg, node_names);
  scheduler_->SetProfiler(profiler_.get());

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

  // Create OutputStreamManagers for each declared output stream and register
  // them on their owning node. This enables callback firing when PostProcess
  // propagates output packets.
  for (const auto& ndef : config_.nodes) {
    if (ndef.output_streams.empty()) continue;

    auto* node = FindNode(ndef.name);
    if (!node) continue;

    std::vector<OutputStreamManager*> managers;
    for (const auto& output_stream : ndef.output_streams) {
      auto mgr = std::make_unique<OutputStreamManager>(output_stream);
      owned_output_stream_managers_.push_back(std::move(mgr));
      managers.push_back(owned_output_stream_managers_.back().get());
    }
    auto handler = std::make_unique<OutputStreamHandler>(managers);
    owned_output_stream_handlers_.push_back(std::move(handler));
    node->SetOutputStreamHandler(owned_output_stream_handlers_.back().get());

    // Register any previously set callbacks on this handler.
    for (const auto& [stream_name, cb] : output_stream_callbacks_) {
      owned_output_stream_handlers_.back()->SetOutputStreamCallback(stream_name, cb);
    }
  }

  // Wire backpressure callbacks and arrival callbacks on each input stream.
  for (const auto& ndef : config_.nodes) {
    for (const auto& input_stream : ndef.input_streams) {
      auto it = stream_managers_.find(input_stream);
      if (it != stream_managers_.end()) {
        auto* mgr = it->second;
        mgr->SetQueueSizeCallbacks(
            [this](InputStreamManager* m, bool*) { OnInputStreamFull(m); },
            [this](InputStreamManager* m, bool*) { OnInputStreamNotFull(m); });

        // When a packet arrives on this stream, schedule the owning node.
        auto* owning_node = FindNode(ndef.name);
        if (owning_node) {
          mgr->SetArrivalCallback([owning_node]() {
            auto* q = owning_node->GetSchedulerQueue();
            if (q && q->IsRunning()) {
              q->AddNode(owning_node);
            }
          });
        }
      }
    }
  }

  // Create InputStreamHandler for each node based on config.
  for (const auto& ndef : config_.nodes) {
    if (ndef.input_streams.empty()) continue;

    auto* node = FindNode(ndef.name);
    if (!node) continue;

    // Collect managers from the node's input ports.
    std::vector<InputStreamManager*> mgrs;
    for (const auto& is : ndef.input_streams) {
      auto it = stream_managers_.find(is);
      if (it != stream_managers_.end()) {
        mgrs.push_back(it->second);
      }
    }
    if (mgrs.empty()) continue;

    auto handler = CreateInputStreamHandler(ndef.input_stream_handler,
                                            ndef.max_in_flight);
    handler->SetInputStreamManagers(mgrs);
    owned_input_stream_handlers_.push_back(std::move(handler));
    node->SetInputStreamHandler(owned_input_stream_handlers_.back().get());
  }

  return absl::OkStatus();
}

absl::Status GraphRuntime::Start() {
  if (!scheduler_) {
    return absl::FailedPreconditionError("Graph not initialized");
  }
  // Convert side_packet_map_ to PacketSet and pass to scheduler.
  PacketSet side_packets;
  for (auto& [tag, packet] : side_packet_map_) {
    side_packets.Set(tag, packet);
  }
  scheduler_->SetInputSidePackets(side_packets);
  return scheduler_->Start();
}

absl::Status GraphRuntime::Schedule() {
  if (!scheduler_) {
    return absl::FailedPreconditionError("Graph not initialized");
  }
  PacketSet side_packets;
  for (auto& [tag, packet] : side_packet_map_) {
    side_packets.Set(tag, packet);
  }
  scheduler_->SetInputSidePackets(side_packets);
  return scheduler_->Schedule();
}

absl::Status GraphRuntime::WaitUntilDone() {
  if (!scheduler_) return absl::OkStatus();
  return scheduler_->WaitUntilDone();
}

absl::Status GraphRuntime::WaitForIdle() {
  if (!scheduler_) return absl::OkStatus();
  return scheduler_->WaitForIdle();
}

bool GraphRuntime::HasGraphFinished() const {
  return scheduler_ && scheduler_->HasGraphFinished();
}

SchedulerState GraphRuntime::GetGraphState() const {
  return scheduler_ ? scheduler_->state() : SchedulerState::kNotStarted;
}

void GraphRuntime::Shutdown() {
  if (scheduler_) {
    scheduler_->Shutdown();
  }
}

void GraphRuntime::Cancel() {
  if (scheduler_) {
    scheduler_->Cancel();
  }
}

absl::Status GraphRuntime::Pause() {
  if (!scheduler_) {
    return absl::FailedPreconditionError("Graph not initialized");
  }
  return scheduler_->Pause();
}

absl::Status GraphRuntime::Resume() {
  if (!scheduler_) {
    return absl::FailedPreconditionError("Graph not initialized");
  }
  return scheduler_->Resume();
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

  // Notify the scheduler that a packet arrived. The scheduler will schedule
  // the owning node.
  scheduler_->AddedPacketToInputStream();
  return absl::OkStatus();
}

absl::Status GraphRuntime::AddPacketToInputStream(
    const std::string& tag, int index, Packet packet) {
  return AddPacketToInputStream(
      tag + ":" + std::to_string(index), std::move(packet));
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
  scheduler_->AddedPacketToInputStream();
  return absl::OkStatus();
}

void GraphRuntime::SetOutputStreamCallback(
    const std::string& stream_name,
    std::function<void(const Packet&)> callback) {
  output_stream_callbacks_[stream_name] = callback;

  // Register on the OutputStreamHandler that manages this stream.
  for (auto& node : all_nodes_) {
    auto* handler = node->GetOutputStreamHandler();
    if (handler) {
      handler->SetOutputStreamCallback(stream_name, callback);
    }
  }
}

void GraphRuntime::ClearOutputStreamCallback(
    const std::string& stream_name) {
  output_stream_callbacks_.erase(stream_name);

  // Clear on all OutputStreamHandlers.
  for (auto& node : all_nodes_) {
    auto* handler = node->GetOutputStreamHandler();
    if (handler) {
      handler->ClearOutputStreamCallback(stream_name);
    }
  }
}

absl::Status GraphRuntime::SetInputSidePacket(
    const std::string& tag_name, Packet packet) {
  side_packet_map_[tag_name] = std::move(packet);
  return absl::OkStatus();
}

void GraphRuntime::SetOutputSidePacketCallback(
    const std::string& name,
    std::function<void(const Packet&)> callback) {
  output_side_packet_callbacks_[name] = std::move(callback);
}

void GraphRuntime::SetHook(int type, hook::HookFn fn) {
  hook::HookFactory::Register(type, fn);
}

void GraphRuntime::SetProfilerConfig(const ProfilerConfig& config) {
  profiler_config_override_ = config;
}

ProfilingContext* GraphRuntime::profiler() {
  return profiler_.get();
}

const ProfilingContext* GraphRuntime::profiler() const {
  return profiler_.get();
}

std::vector<NodeProfile> GraphRuntime::GetNodeProfiles() const {
  if (!profiler_) return {};
  auto internal_profiles = profiler_->GetNodeProfiles();
  std::vector<NodeProfile> result;
  result.reserve(internal_profiles.size());
  for (const auto& ip : internal_profiles) {
    NodeProfile np;
    np.node_name = ip.node_name;
    np.open_runtime_usec = ip.open_runtime_usec;
    np.close_runtime_usec = ip.close_runtime_usec;
    np.process_count = ip.process_runtime.count();
    np.process_time_total_usec = ip.process_runtime.total();
    np.process_time_mean_usec = ip.process_runtime.mean();
    result.push_back(std::move(np));
  }
  return result;
}

absl::Status GraphRuntime::WriteProfile(const std::string& path) const {
  if (!profiler_) {
    return absl::FailedPreconditionError("Profiler not initialized");
  }
  return profiler_->WriteProfile(path);
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
