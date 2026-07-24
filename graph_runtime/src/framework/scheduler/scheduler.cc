#include "src/framework/scheduler/scheduler.h"

#include <algorithm>
#include <set>
#include <string>
#include <thread>

#define GRAPHRT_LOG_TAG "graphrt::scheduler"
#include "src/framework/utils/logger.h"
#include "src/framework/scheduler/input_stream_handler.h"
#include "src/framework/stream/output_stream_handler.h"

namespace graph::runtime {

Scheduler::Scheduler() {
  all_queues_.push_back(&default_queue_);
}

void Scheduler::SetInputStreamHandler(
    std::unique_ptr<InputStreamHandler> handler) {
  input_stream_handler_ = std::move(handler);
}

void Scheduler::SetErrorCallback(ErrorCallback cb) {
  error_callback_ = std::move(cb);
}

absl::Status Scheduler::SetDefaultExecutor(
    std::shared_ptr<Executor> executor) {
  default_executor_ = executor;
  default_queue_.SetExecutor(executor.get());
  return absl::OkStatus();
}

absl::Status Scheduler::SetNonDefaultExecutor(
    const std::string& name, std::shared_ptr<Executor> executor) {
  auto queue = std::make_unique<SchedulerQueue>(name);
  queue->SetExecutor(executor.get());
  all_queues_.push_back(queue.get());
  non_default_queues_[name] = std::move(queue);
  return absl::OkStatus();
}

void Scheduler::HandleIdle() {
  if (++handling_idle_ > 1) { --handling_idle_; return; }

  bool inputs_remaining = total_graph_input_streams_ > 0 &&
                          num_closed_graph_input_streams_ < total_graph_input_streams_;

  while (IsIdle() && (state_ == SchedulerState::kRunning ||
                      state_ == SchedulerState::kCancelling)) {
    for (auto it = active_sources_.begin(); it != active_sources_.end();) {
      if (state_ == SchedulerState::kCancelling) {
        active_sources_.erase(it++);
      } else {
        ++it;
      }
    }

    bool all_inputs_closed = (total_graph_input_streams_ > 0 &&
                              num_closed_graph_input_streams_ >= total_graph_input_streams_);
    bool no_more_sources = active_sources_.empty();
    bool should_quit = has_error_ ||
                       (no_more_sources && all_inputs_closed) ||
                       (no_more_sources && total_graph_input_streams_ == 0);

    if (should_quit) {
      if (error_callback_ && has_error_) {
        error_callback_(absl::InternalError("Graph execution error"));
      }
      state_ = SchedulerState::kTerminated;
      cv_.notify_all();
      --handling_idle_;
      return;
    }

    if (!active_sources_.empty()) {
      for (auto* source : active_sources_) {
        default_queue_.AddNode(source);
      }
      --handling_idle_;
      return;
    }

    if (inputs_remaining) {
      --handling_idle_;
      return;
    }

    bool any_pending = false;
    for (auto* q : all_queues_) {
      if (!q->IsIdle()) { any_pending = true; break; }
    }
    if (!any_pending) {
      state_ = SchedulerState::kTerminated;
      cv_.notify_all();
      --handling_idle_;
      return;
    }
    break;
  }
  --handling_idle_;
}

absl::Status Scheduler::Schedule() {
  state_ = SchedulerState::kRunning;

  for (auto* node : all_nodes_) {
    if (node->input_port_count() == 0) {
      source_nodes_.push_back(node);
    }
  }
  for (auto* node : all_nodes_) {
    AssignNodeToQueue(node);
  }

  // Open all nodes
  for (auto* node : all_nodes_) {
    InputStreamShardSet input_shards;
    OutputStreamShardSet output_shards;
    NodeOptions opts;
    GraphContext ctx(node->name(), reinterpret_cast<int64_t>(node),
                     "node", Timestamp::Unstarted(),
                     &input_shards, &output_shards, &opts);
    ctx.SetInputSidePackets(input_side_packets_);
    auto status = node->Open(ctx);
    if (!status.ok()) {
      Logger::Error(std::string("Open failed for " + node->name() + ": " + std::string(status.ToString())).c_str());
      if (error_callback_) error_callback_(status);
      state_ = SchedulerState::kTerminated;
      return status;
    }
    Logger::Info(std::string("Opened " + node->name()).c_str());
    if (node->input_port_count() == 0) {
      active_sources_.insert(node);
    }
  }

  // Process loop
  bool any_source_active = true;
  while (any_source_active && !stopping_ && !has_error_) {
    any_source_active = false;

    auto it = active_sources_.begin();
    while (it != active_sources_.end()) {
      Node* source = *it;
      if (stopping_ || has_error_) break;

      InputStreamShardSet input_shards;
      OutputStreamShardSet output_shards;
      NodeOptions opts;
      Timestamp ts(static_cast<int64_t>(processed_count_));
      GraphContext ctx(source->name(), reinterpret_cast<int64_t>(source),
                       "node", ts, &input_shards, &output_shards, &opts);

      Logger::Info(std::string("Process " + source->name() + " at ts=" + std::to_string(ts.Value())).c_str());
      auto status = source->Process(ctx);

      if (!status.ok() && !IsStopStatus(status)) {
        Logger::Error(std::string("Error in " + source->name() + ": " + std::string(status.ToString())).c_str());
        if (error_callback_) error_callback_(status);
        has_error_ = true;
        break;
      }
      if (IsStopStatus(status)) {
        Logger::Info(std::string(source->name() + " stopped").c_str());
        // Propagate any output packets from this source before removing it.
        if (source->GetOutputStreamHandler()) {
          source->GetOutputStreamHandler()->PostProcess(ts, &output_shards);
        }
        it = active_sources_.erase(it);
        continue;
      }

      // Propagate source outputs via OutputStreamHandler.
      if (source->GetOutputStreamHandler()) {
        source->GetOutputStreamHandler()->PostProcess(ts, &output_shards);
      }

      // Propagate outputs downstream
      for (auto* downstream : all_nodes_) {
        if (stopping_ || has_error_) break;
        if (downstream->input_port_count() == 0) continue;

        InputStreamShardSet ds_input;
        OutputStreamShardSet ds_output;
        GraphContext dctx(downstream->name(), reinterpret_cast<int64_t>(downstream),
                          "node", ts, &ds_input, &ds_output, &opts);

        Logger::Info(std::string("Process " + downstream->name() + " at ts=" + std::to_string(ts.Value())).c_str());
        auto ds = downstream->Process(dctx);
        if (!ds.ok() && !IsStopStatus(ds)) {
          Logger::Error(std::string("Error in " + downstream->name() + ": " + std::string(ds.ToString())).c_str());
          if (error_callback_) error_callback_(ds);
          has_error_ = true;
          break;
        }
      }
      ++processed_count_;
      ++it;
    }
  }

  // Close all nodes
  for (auto* node : all_nodes_) {
    InputStreamShardSet input_shards;
    OutputStreamShardSet output_shards;
    NodeOptions opts;
    GraphContext ctx(node->name(), reinterpret_cast<int64_t>(node),
                     "node", Timestamp::Done(),
                     &input_shards, &output_shards, &opts);
    auto status = node->Close(ctx);
    if (!status.ok()) {
      Logger::Error(std::string("Close error for " + node->name() + ": " + std::string(status.ToString())).c_str());
    }
    Logger::Info(std::string("Closed " + node->name()).c_str());
  }

  state_ = SchedulerState::kTerminated;
  cv_.notify_all();
  return absl::OkStatus();
}

absl::Status Scheduler::Start() {
  state_ = SchedulerState::kRunning;

  for (auto* node : all_nodes_) {
    if (node->input_port_count() == 0) {
      source_nodes_.push_back(node);
    }
  }
  for (auto* node : all_nodes_) {
    AssignNodeToQueue(node);
  }

  // Open all nodes
  for (auto* node : all_nodes_) {
    InputStreamShardSet input_shards;
    OutputStreamShardSet output_shards;
    NodeOptions opts;
    GraphContext ctx(node->name(), reinterpret_cast<int64_t>(node),
                     "node", Timestamp::Unstarted(),
                     &input_shards, &output_shards, &opts);
    ctx.SetInputSidePackets(input_side_packets_);
    auto status = node->Open(ctx);
    if (!status.ok()) {
      Logger::Error(std::string("Open failed for " + node->name()).c_str());
      if (error_callback_) error_callback_(status);
      state_ = SchedulerState::kTerminated;
      return status;
    }
    if (node->input_port_count() == 0) {
      active_sources_.insert(node);
      default_queue_.AddNode(node);
    }
  }

  // Wire idle callbacks so queue idle → HandleIdle → termination detection.
  for (auto* q : all_queues_) {
    q->SetIdleCallback([this](bool) {
      cv_.notify_all();
      HandleIdle();
    });
    q->SetSourceStoppedCallback([this](Node* node) {
      active_sources_.erase(node);
      cv_.notify_all();
      HandleIdle();
    });
    q->SetRunning(true);
  }

  HandleIdle();
  return absl::OkStatus();
}

void Scheduler::AddedPacketToGraphInputStream() {
  for (auto* node : all_nodes_) {
    if (node->input_port_count() == 0) continue;
    bool has_data = false;
    for (const auto& [name, mgr] : node->InputPorts()) {
      if (!mgr->IsEmpty()) {
        has_data = true;
        break;
      }
    }
    if (has_data) {
      auto* q = node->GetSchedulerQueue();
      if (q && q->IsRunning()) {
        q->AddNode(node);
      }
    }
  }
  HandleIdle();
}

void Scheduler::Shutdown() {
  stopping_ = true;
  state_ = SchedulerState::kTerminated;
  for (auto* q : all_queues_) {
    q->CleanupAfterRun();
  }
  cv_.notify_all();
}

absl::Status Scheduler::WaitUntilDone() {
  std::unique_lock<std::mutex> lock(mutex_);
  cv_.wait(lock, [this] { return state_ == SchedulerState::kTerminated; });
  return absl::OkStatus();
}

absl::Status Scheduler::WaitForIdle() {
  std::unique_lock<std::mutex> lock(mutex_);
  cv_.wait(lock, [this] { return IsIdle(); });
  return absl::OkStatus();
}

void Scheduler::SetQueuesRunning(bool running) {
  for (auto* q : all_queues_) {
    q->SetRunning(running);
  }
}

absl::Status Scheduler::Pause() {
  if (state_ != SchedulerState::kRunning) {
    return absl::FailedPreconditionError("Graph is not running");
  }
  state_ = SchedulerState::kPaused;
  SetQueuesRunning(false);
  return absl::OkStatus();
}

absl::Status Scheduler::Resume() {
  if (state_ != SchedulerState::kPaused) {
    return absl::FailedPreconditionError("Graph is not paused");
  }
  state_ = SchedulerState::kRunning;
  SetQueuesRunning(true);
  // Submit any queued tasks and check if graph should terminate.
  for (auto* q : all_queues_) {
    q->SubmitWaitingTasksToExecutor();
  }
  HandleIdle();
  return absl::OkStatus();
}

void Scheduler::AssignNodeToQueue(Node* node) {
  const auto& executor_name = node->ExecutorName();
  if (!executor_name.empty()) {
    auto it = non_default_queues_.find(executor_name);
    if (it != non_default_queues_.end()) {
      node->SetSchedulerQueue(it->second.get());
      return;
    }
  }
  node->SetSchedulerQueue(&default_queue_);
}

void Scheduler::OnNodeOpened(Node* node) {
  if (node->input_port_count() == 0) {
    node->GetSchedulerQueue()->AddNode(node);
  }
}

SchedulerQueue& Scheduler::GetQueue(const std::string& executor_name) {
  if (executor_name.empty()) return default_queue_;
  auto it = non_default_queues_.find(executor_name);
  if (it != non_default_queues_.end()) return *it->second;
  return default_queue_;
}

absl::Status Scheduler::AddNode(Node* node) {
  return absl::UnimplementedError("AddNode is Phase 2");
}

absl::Status Scheduler::RemoveNode(Node* node) {
  return absl::UnimplementedError("RemoveNode is Phase 2");
}

}  // namespace graph::runtime
