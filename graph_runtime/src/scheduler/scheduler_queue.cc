#include "src/scheduler/scheduler_queue.h"

namespace graph::runtime {

SchedulerQueue::SchedulerQueue(std::string name) : name_(std::move(name)) {}

void SchedulerQueue::SetRunning(bool running) {
  running_ = running;
  if (running_) {
    SubmitToExecutor();
  }
}

void SchedulerQueue::Reset() {
  queue_ = {};
  num_pending_tasks_ = 0;
}

void SchedulerQueue::CleanupAfterRun() {
  Reset();
  num_pending_tasks_ = 0;
  running_ = false;
}

void SchedulerQueue::AddNode(Node* node) {
  Item item;
  item.node = node;
  item.is_open_node = false;
  item.source_layer = node ? node->SourceLayer() : 0;
  item.node_id = reinterpret_cast<int64_t>(node);
  queue_.push(item);
  if (running_) {
    SubmitToExecutor();
  }
}

void SchedulerQueue::AddNodeForOpen(Node* node) {
  Item item;
  item.node = node;
  item.is_open_node = true;
  item.source_layer = node ? node->SourceLayer() : 0;
  item.node_id = reinterpret_cast<int64_t>(node);
  queue_.push(item);
  if (running_) {
    SubmitToExecutor();
  }
}

void SchedulerQueue::RunNextTask() {
  if (queue_.empty()) {
    UpdateIdleState();
    return;
  }
  Item item = queue_.top();
  queue_.pop();
  RunNode(item.node, item.is_open_node);
  --num_pending_tasks_;
  UpdateIdleState();
  if (!queue_.empty() && running_) {
    SubmitToExecutor();
  }
}

void SchedulerQueue::SubmitToExecutor() {
  if (!executor_) return;
  ++num_pending_tasks_;
  executor_->AddTask(this);
}

void SchedulerQueue::RunNode(Node* node, bool is_open) {
  if (!node) return;
  (void)is_open;
}

void SchedulerQueue::UpdateIdleState() {
  if (idle_callback_) {
    idle_callback_(IsIdle());
  }
}

}  // namespace graph::runtime
