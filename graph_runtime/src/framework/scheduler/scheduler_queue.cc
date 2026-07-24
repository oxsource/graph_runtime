#include "src/framework/scheduler/scheduler_queue.h"
#include "src/framework/node/graph_context.h"
#include "src/framework/stream/output_stream_handler.h"

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
    --num_pending_tasks_;
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

  InputStreamShardSet inputs;
  OutputStreamShardSet outputs;
  NodeOptions opts;

  // Populate input shards from the node's InputStreamManagers.
  for (const auto& [port_name, mgr] : node->InputPorts()) {
    bool stream_is_done = false;
    Packet pkt = mgr->PopQueueHead(&stream_is_done);
    if (!pkt.IsEmpty()) {
      auto& shard = inputs.Get(port_name);
      shard.PushPacket(std::move(pkt));
    }
    if (stream_is_done) {
      inputs.Get(port_name).SetDone(true);
    }
  }

  Timestamp ts = is_open ? Timestamp::Unstarted() : Timestamp(timestamp_counter_++);
  GraphContext ctx(node->name(), reinterpret_cast<int64_t>(node),
                   "node", ts, &inputs, &outputs, &opts);

  if (is_open) {
    (void)node->Open(ctx);
    return;
  }

  // Process the node
  absl::Status status = node->Process(ctx);

  if (IsStopStatus(status)) {
    // Propagate any output packets produced by this process cycle.
    if (node->GetOutputStreamHandler()) {
      node->GetOutputStreamHandler()->PostProcess(ts, &outputs);
    }
    // Non-source returning StatusStop triggers graceful shutdown.
    if (node->input_port_count() > 0) {
      if (idle_callback_) {
        idle_callback_(true);
      }
    } else {
      // Source returning Stop — notify scheduler to remove from active_sources_.
      if (source_stopped_callback_) {
        source_stopped_callback_(node);
      }
    }
    return;
  }

  if (!status.ok()) {
    // Error during processing — propagate.
    if (node->GetOutputStreamHandler()) {
      node->GetOutputStreamHandler()->PostProcess(ts, &outputs);
    }
    return;
  }

  // Propagate outputs
  if (node->GetOutputStreamHandler()) {
    node->GetOutputStreamHandler()->PostProcess(ts, &outputs);
  }
}

void SchedulerQueue::UpdateIdleState() {
  if (idle_callback_) {
    idle_callback_(IsIdle());
  }
}

}  // namespace graph::runtime
