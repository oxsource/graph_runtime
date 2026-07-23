#include "src/public/graph_runtime.h"
#include "src/public/graph_builder.h"

#include "src/scheduler/scheduler.h"
#include "src/scheduler/input_stream_handler.h"

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
  return absl::OkStatus();
}

absl::Status GraphRuntime::Start() {
  if (!scheduler_) {
    return absl::FailedPreconditionError("Graph not initialized");
  }
  return scheduler_->Schedule();
}

absl::Status GraphRuntime::WaitUntilDone() {
  if (!scheduler_) {
    return absl::FailedPreconditionError("Graph not initialized");
  }
  return scheduler_->WaitUntilDone();
}

void GraphRuntime::Shutdown() {
  if (scheduler_) {
    scheduler_->Shutdown();
  }
}

absl::Status GraphRuntime::AddPacketToInputStream(
    const std::string& stream_name, Packet packet) {
  return absl::UnimplementedError("AddPacketToInputStream: Phase 2");
}

absl::Status GraphRuntime::CloseInputStream(
    const std::string& stream_name) {
  return absl::UnimplementedError("CloseInputStream: Phase 2");
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

}  // namespace graph::runtime
