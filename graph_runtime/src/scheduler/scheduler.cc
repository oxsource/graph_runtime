#include "src/scheduler/scheduler.h"

#include <algorithm>
#include <iostream>
#include <set>
#include <thread>

#include "src/scheduler/input_stream_handler.h"
#include "src/stream/output_stream_handler.h"

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

absl::Status Scheduler::Schedule() {
  state_ = SchedulerState::kRunning;

  // Sort sources from non-sources
  for (auto* node : all_nodes_) {
    if (node->input_port_count() == 0) {
      source_nodes_.push_back(node);
    }
  }
  for (auto* node : all_nodes_) {
    AssignNodeToQueue(node);
  }

  // Phase 1: Open all nodes
  for (auto* node : all_nodes_) {
    InputStreamShardSet input_shards;
    OutputStreamShardSet output_shards;
    NodeOptions opts;
    GraphContext ctx(node->name(), reinterpret_cast<int64_t>(node),
                     "node", Timestamp::Unstarted(),
                     &input_shards, &output_shards, &opts);
    auto status = node->Open(ctx);
    if (!status.ok()) {
      std::cerr << "  Open failed for " << node->name() << ": " << status << std::endl;
      if (error_callback_) error_callback_(status);
      state_ = SchedulerState::kTerminated;
      return status;
    }
    std::cout << "  Opened " << node->name() << std::endl;
    if (node->input_port_count() == 0) {
      active_sources_.insert(node);
    }
  }

  // Phase 2: Process loop — run sources, propagate downstream
  bool any_source_active = true;
  while (any_source_active && !stopping_ && !has_error_) {
    any_source_active = false;

    for (auto* source : active_sources_) {
      if (stopping_ || has_error_) break;

      InputStreamShardSet input_shards;
      OutputStreamShardSet output_shards;
      NodeOptions opts;
      Timestamp ts(static_cast<int64_t>(processed_count_));
      GraphContext ctx(source->name(), reinterpret_cast<int64_t>(source),
                       "node", ts, &input_shards, &output_shards, &opts);

      std::cout << "  Process " << source->name() << " at ts=" << ts.Value() << std::endl;
      auto status = source->Process(ctx);

      if (!status.ok() && !IsStopStatus(status)) {
        std::cerr << "  Error in " << source->name() << ": " << status << std::endl;
        if (error_callback_) error_callback_(status);
        has_error_ = true;
        break;
      }

      if (IsStopStatus(status)) {
        std::cout << "  " << source->name() << " stopped" << std::endl;
        active_sources_.erase(source);
        continue;
      }

      // Propagate outputs downstream
      for (auto& kv : output_shards) {
        (void)kv;
      }

      // Find and process downstream nodes (simple BFS)
      for (auto* downstream : all_nodes_) {
        if (stopping_ || has_error_) break;
        if (downstream->input_port_count() == 0) continue;

        InputStreamShardSet ds_input;
        OutputStreamShardSet ds_output;
        GraphContext dctx(downstream->name(), reinterpret_cast<int64_t>(downstream),
                          "node", ts, &ds_input, &ds_output, &opts);

        std::cout << "  Process " << downstream->name() << " at ts=" << ts.Value() << std::endl;
        auto ds = downstream->Process(dctx);
        if (!ds.ok() && !IsStopStatus(ds)) {
          std::cerr << "  Error in " << downstream->name() << ": " << ds << std::endl;
          if (error_callback_) error_callback_(ds);
          has_error_ = true;
          break;
        }
      }
      ++processed_count_;
    }
  }

  // Phase 3: Close all nodes
  for (auto* node : all_nodes_) {
    InputStreamShardSet input_shards;
    OutputStreamShardSet output_shards;
    NodeOptions opts;
    GraphContext ctx(node->name(), reinterpret_cast<int64_t>(node),
                     "node", Timestamp::Done(),
                     &input_shards, &output_shards, &opts);
    auto status = node->Close(ctx);
    if (!status.ok()) {
      std::cerr << "  Close error for " << node->name() << ": " << status << std::endl;
    }
    std::cout << "  Closed " << node->name() << std::endl;
  }

  state_ = SchedulerState::kTerminated;
  cv_.notify_all();
  return absl::OkStatus();
}

absl::Status Scheduler::WaitUntilDone() {
  std::unique_lock<std::mutex> lock(mutex_);
  cv_.wait(lock, [this] { return state_ == SchedulerState::kTerminated; });
  return absl::OkStatus();
}

void Scheduler::Shutdown() {
  stopping_ = true;
  state_ = SchedulerState::kTerminated;
  cv_.notify_all();
}

absl::Status Scheduler::Pause() {
  return absl::UnimplementedError("Pause not supported in sync mode");
}

absl::Status Scheduler::Resume() {
  return absl::UnimplementedError("Resume not supported in sync mode");
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

void Scheduler::HandleIdle() {
  // In sync mode, idle is handled by the main event loop.
}

absl::Status Scheduler::AddNode(Node* node) {
  return absl::UnimplementedError("AddNode is Phase 2");
}

absl::Status Scheduler::RemoveNode(Node* node) {
  return absl::UnimplementedError("RemoveNode is Phase 2");
}

}  // namespace graph::runtime
