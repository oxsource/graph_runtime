#include "src/scheduler/scheduler.h"

#include <algorithm>
#include <set>
#include <thread>

#include "src/scheduler/input_stream_handler.h"

namespace graph::runtime {

Scheduler::Scheduler() {
  default_queue_.SetIdleCallback(
      [this](bool idle) {
        if (idle) {
          --non_idle_queue_count_;
          if (non_idle_queue_count_ == 0) HandleIdle();
        } else {
          ++non_idle_queue_count_;
        }
      });
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
  queue->SetIdleCallback(
      [this, name](bool idle) {
        if (idle) {
          --non_idle_queue_count_;
          if (non_idle_queue_count_ == 0) HandleIdle();
        } else {
          ++non_idle_queue_count_;
        }
      });
  all_queues_.push_back(queue.get());
  non_default_queues_[name] = std::move(queue);
  return absl::OkStatus();
}

absl::Status Scheduler::Schedule() {
  state_ = SchedulerState::kRunning;

  // Sort: sources first for topological order
  for (auto* node : all_nodes_) {
    if (node->input_port_count() == 0) {
      source_nodes_.push_back(node);
    }
  }

  // Assign nodes to queues
  for (auto* node : all_nodes_) {
    AssignNodeToQueue(node);
  }

  // Activate sources — schedule Open tasks
  for (auto* source : source_nodes_) {
    active_sources_.insert(source);
    source->GetSchedulerQueue()->AddNodeForOpen(source);
  }

  return absl::OkStatus();
}

absl::Status Scheduler::WaitUntilDone() {
  std::unique_lock<std::mutex> lock(mutex_);
  cv_.wait(lock, [this] { return state_ == SchedulerState::kTerminated; });
  return absl::OkStatus();
}

void Scheduler::Shutdown() {
  stopping_ = true;
  state_ = SchedulerState::kCancelling;
  HandleIdle();
}

absl::Status Scheduler::Pause() {
  if (stopping_) {
    return absl::FailedPreconditionError("Cannot pause during shutdown");
  }
  if (state_ != SchedulerState::kRunning) {
    return absl::FailedPreconditionError("Scheduler is not running");
  }
  state_ = SchedulerState::kPaused;
  for (auto* q : all_queues_) {
    q->SetRunning(false);
  }
  return absl::OkStatus();
}

absl::Status Scheduler::Resume() {
  if (state_ != SchedulerState::kPaused) {
    return absl::FailedPreconditionError("Scheduler is not paused");
  }
  state_ = SchedulerState::kRunning;
  for (auto* q : all_queues_) {
    q->SetRunning(true);
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
  if (input_stream_handler_ && node->input_port_count() > 0) {
    std::vector<InputStreamManager*> managers;
    for (size_t i = 0; i < node->input_port_count(); ++i) {
    }
    // Schedule initial Process for sources
  }
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
  if (++handling_idle_ > 1) {
    --handling_idle_;
    return;
  }

  if (has_error_ && non_idle_queue_count_ == 0) {
    state_ = SchedulerState::kTerminated;
    cv_.notify_all();
    --handling_idle_;
    return;
  }

  // Clean up closed sources
  std::set<Node*> still_active;
  for (auto* s : active_sources_) {
    still_active.insert(s);
  }
  active_sources_ = still_active;

  if (source_nodes_.empty() && non_idle_queue_count_ == 0) {
    state_ = SchedulerState::kTerminated;
    cv_.notify_all();
    --handling_idle_;
    return;
  }

  if (!active_sources_.empty() && non_idle_queue_count_ == 0) {
    // All sources throttled or idle — unthrottle
  }

  --handling_idle_;
}

absl::Status Scheduler::AddNode(Node* node) {
  return absl::UnimplementedError("AddNode is Phase 2");
}

absl::Status Scheduler::RemoveNode(Node* node) {
  return absl::UnimplementedError("RemoveNode is Phase 2");
}

}  // namespace graph::runtime
