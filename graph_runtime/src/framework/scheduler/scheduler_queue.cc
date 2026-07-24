#include "src/framework/scheduler/scheduler_queue.h"
#include "src/framework/node/graph_context.h"
#include "src/framework/scheduler/input_stream_handler.h"
#include "src/framework/stream/output_stream_handler.h"
#include "src/framework/scheduler/counters.h"

#define GRAPHRT_LOG_TAG "graphrt::scheduler_queue"
#include "src/framework/utils/logger.h"

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
  // Check MaxInFlight constraint. pending_count includes the current
  // execution, so compare > not >= to allow scheduling the next
  // invocation before the current one completes.
  int max_in_flight = node ? node->GetContract().MaxInFlight() : 1;
  if (node && node->pending_count() > max_in_flight) {
    // Node has reached its concurrency limit; defer scheduling.
    return;
  }
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
  int before_pending = num_pending_tasks_;
  RunNode(item.node, item.is_open_node);
  --num_pending_tasks_;
  UpdateIdleState();
  // Only submit a new task if RunNode didn't already schedule one
  // (e.g., via callback → AddedPacketToInputStream).
  if (!queue_.empty() && running_ && num_pending_tasks_ == 0) {
    SubmitToExecutor();
  }
}

void SchedulerQueue::SubmitToExecutor() {
  if (!executor_) return;
  ++num_pending_tasks_;
  if (perf_counters_) perf_counters_->tasks_submitted.Increment();
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
      // Best-effort runtime type check against NodeContract.
      if (!pkt.DebugTypeName().empty()) {
        const auto& expected = node->GetContract().Inputs().Get(port_name);
        if (expected.IsSet() && expected.TypeName() != pkt.DebugTypeName()) {
          Logger::Warn(std::string("Type mismatch on '").append(port_name)
              .append("': expected ").append(expected.TypeName())
              .append(", got ").append(pkt.DebugTypeName()).c_str());
        }
      }
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
    if (perf_counters_) perf_counters_->nodes_opened.Increment();
    return;
  }

  // Mark node as in-flight (MaxInFlight tracking).
  node->IncrementPending();

  // Process the node
  absl::Status status = node->Process(ctx);

  if (IsStopStatus(status)) {
    if (node->GetOutputStreamHandler()) {
      node->GetOutputStreamHandler()->PostProcess(ts, &outputs);
    }
    node->DecrementPending();
    if (node->input_port_count() > 0) {
      if (node->GetInputStreamHandler()) {
        Timestamp input_bound;
        int ma = std::max(1, node->GetContract().MaxInFlight());
        node->GetInputStreamHandler()->ScheduleInvocations(ma, &input_bound, *node, ctx);
      }
      if (idle_callback_) idle_callback_(true);
    } else {
      if (source_stopped_callback_) source_stopped_callback_(node);
    }
    return;
  }

  if (!status.ok()) {
    if (node->GetOutputStreamHandler()) {
      node->GetOutputStreamHandler()->PostProcess(ts, &outputs);
    }
    node->DecrementPending();
    return;
  }

  // Propagate outputs
  if (node->GetOutputStreamHandler()) {
    node->GetOutputStreamHandler()->PostProcess(ts, &outputs);
  }
  node->DecrementPending();
  if (perf_counters_) {
    perf_counters_->tasks_completed.Increment();
    perf_counters_->packets_processed.Increment();
  }
}

void SchedulerQueue::UpdateIdleState() {
  if (idle_callback_) {
    idle_callback_(IsIdle());
  }
}

void SchedulerQueue::SubmitWaitingTasksToExecutor() {
  if (!queue_.empty() && running_) {
    SubmitToExecutor();
  }
}

}  // namespace graph::runtime
