#include "src/framework/scheduler/scheduler_queue.h"

#include "absl/strings/str_cat.h"

#include "src/framework/node/graph_context.h"
#include "src/framework/scheduler/counters.h"
#include "src/framework/scheduler/input_stream_handler.h"
#include "src/framework/stream/output_stream_handler.h"

#define GRAPHRT_LOG_TAG "graphrt::scheduler_queue"
#include "src/framework/utils/logger.h"

namespace graph::runtime {

namespace {

// True when at least one input manager of the node still holds an unconsumed
// packet. Used to keep a node scheduled while its buffered input drains.
bool HasPendingInput(const Node* node) {
  for (const auto& [name, mgr] : node->InputPorts()) {
    if (mgr && !mgr->IsEmpty()) return true;
  }
  return false;
}

}  // namespace

SchedulerQueue::SchedulerQueue(std::string name) : name_(std::move(name)) {}

void SchedulerQueue::SetRunning(bool running) {
  bool should_submit = false;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    running_ = running;
    should_submit = running_ && !queue_.empty();
  }
  if (should_submit) {
    SubmitToExecutor();
  }
}

void SchedulerQueue::Reset() {
  std::lock_guard<std::mutex> lock(mutex_);
  queue_ = {};
  num_pending_tasks_ = 0;
}

void SchedulerQueue::CleanupAfterRun() {
  std::lock_guard<std::mutex> lock(mutex_);
  queue_ = {};
  num_pending_tasks_ = 0;
  running_ = false;
}

void SchedulerQueue::AddNode(Node* node) {
  bool should_submit = false;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    // Enforce MaxInFlight atomically: pending_count tracks in-flight Process
    // invocations (queued + running). Checking it here and incrementing under
    // the same lock prevents a second worker from dequeuing an item while the
    // first is still running — which would race node member state (e.g. an
    // encoder's bsf/packet buffers) and reorder its output. The previous ">"
    // (and the separate IncrementPending in RunNode) left a window where two
    // invocations of a MaxInFlight=1 node could overlap.
    int max_in_flight = node ? node->GetContract().MaxInFlight() : 1;
    if (node && node->pending_count() >= max_in_flight) {
      // Node is at its concurrency limit; defer scheduling.
      return;
    }
    if (node) node->IncrementPending();
    Item item;
    item.node = node;
    item.is_open_node = false;
    item.source_layer = node ? node->SourceLayer() : 0;
    item.node_id = reinterpret_cast<int64_t>(node);
    queue_.push(item);
    should_submit = running_;
  }
  if (should_submit) {
    SubmitToExecutor();
  }
}

void SchedulerQueue::AddNodeForOpen(Node* node) {
  bool should_submit = false;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    Item item;
    item.node = node;
    item.is_open_node = true;
    item.source_layer = node ? node->SourceLayer() : 0;
    item.node_id = reinterpret_cast<int64_t>(node);
    queue_.push(item);
    should_submit = running_;
  }
  if (should_submit) {
    SubmitToExecutor();
  }
}

bool SchedulerQueue::TryPop(Item* item) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (queue_.empty()) return false;
  *item = queue_.top();
  queue_.pop();
  return true;
}

void SchedulerQueue::OnTaskFinished() {
  bool should_submit = false;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (num_pending_tasks_ > 0) --num_pending_tasks_;
    should_submit = running_ && !queue_.empty();
  }
  if (should_submit) {
    SubmitToExecutor();
  }
  UpdateIdleState();
}

void SchedulerQueue::RunNextTask() {
  Item item;
  if (!TryPop(&item)) {
    // No work item: account for the pending-task slot this run consumed
    // (it was incremented by SubmitToExecutor) and report idle.
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (num_pending_tasks_ > 0) --num_pending_tasks_;
    }
    UpdateIdleState();
    return;
  }

  // Execute the node without holding the queue lock so producers can enqueue
  // and other workers can proceed.
  RunNode(item.node, item.is_open_node);

  OnTaskFinished();
}

void SchedulerQueue::SubmitToExecutor() {
  if (!executor_) return;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    ++num_pending_tasks_;
  }
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
    ProfilingContext::Scope scope(
        ProfilingContext::EventType::OPEN, node->name(), profiler_);
    (void)node->Open(ctx);
    if (perf_counters_) perf_counters_->nodes_opened.Increment();
    return;
  }

  // Reset output shards for this invocation so PostProcess always has one
  // shard per output stream (even when the node writes nothing this round).
  if (node->GetOutputStreamHandler()) {
    node->GetOutputStreamHandler()->PrepareOutputs(ts, &outputs);
  }

  ProfilingContext::Scope scope(
      ProfilingContext::EventType::PROCESS, node->name(), profiler_);

  // Process the node
  absl::Status status = node->Process(ctx);

  if (IsStopStatus(status)) {
    if (node->GetOutputStreamHandler()) {
      // Propagate any in-flight packets, then close the output streams so
      // downstream nodes observe Timestamp::Done() and can finalize (e.g.
      // encoder Flush, muxer finalize) before the graph is torn down.
      node->GetOutputStreamHandler()->PostProcess(ts, &outputs);
      node->GetOutputStreamHandler()->Close(&outputs);
    }
    node->DecrementPending();
    if (node->input_port_count() > 0) {
      // Drain any packets still buffered after the stop so downstream does
      // not observe a premature close (MediaPipe: non-source nodes keep
      // processing until their inputs are consumed).
      if (HasPendingInput(node)) {
        auto* q = node->GetSchedulerQueue();
        if (q) q->AddNode(node);
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
    if (error_callback_) {
      // Keep the original error code and prefix the failing node name so the
      // caller can locate the node that aborted the run.
      error_callback_(absl::Status(
          status.code(),
          absl::StrCat(node->name(), ": ", status.message())));
    }
    return;
  }

  // Propagate outputs
  if (node->GetOutputStreamHandler()) {
    node->GetOutputStreamHandler()->PostProcess(ts, &outputs);
  }

  // When every input stream is done, the node must run ONE more Process to
  // observe the done state and finalize (e.g. an encoder Flush draining
  // codec-buffered frames) BEFORE its output streams are closed — otherwise
  // the finalize output (Flush packets) is dropped by the closed shard. This
  // mirrors MediaPipe: a node finalizes (kReadyForClose -> EndScheduling) and
  // only then are its output streams closed.
  bool all_inputs_done = false;
  if (node->input_port_count() > 0 && node->GetOutputStreamHandler()) {
    all_inputs_done = true;
    for (const auto& [name, mgr] : node->InputPorts()) {
      if (!mgr->IsDone()) {
        all_inputs_done = false;
        break;
      }
    }
  }

  // Decide close_pending_ transitions UNDER THE LOCK, but do not call AddNode
  // here (it checks pending_count, which is still 1 until DecrementPending
  // below). Store the action; apply it after DecrementPending.
  enum class FinalizeAction { kNone, kSchedule, kClose };
  FinalizeAction action = FinalizeAction::kNone;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (all_inputs_done && close_pending_.count(node) == 0) {
      // First time observing done: schedule the node once more so it can
      // finalize (Flush) while its outputs are still open, then close later.
      close_pending_.insert(node);
      action = FinalizeAction::kSchedule;
    } else if (close_pending_.erase(node) > 0) {
      // The node has finalized (its done Process ran); now it is safe to close
      // its output streams so downstream sees Done and can also finalize.
      action = FinalizeAction::kClose;
    }
  }

  node->DecrementPending();
  if (perf_counters_) {
    perf_counters_->tasks_completed.Increment();
    perf_counters_->packets_processed.Increment();
  }

  if (action == FinalizeAction::kClose) {
    node->GetOutputStreamHandler()->Close(&outputs);
  } else if (action == FinalizeAction::kSchedule) {
    auto* q = node->GetSchedulerQueue();
    if (q) q->AddNode(node);
  }

  // MediaPipe-style drain: re-schedule the node while any input packet is
  // still buffered (EndScheduling -> SchedulingLoop pattern). This keeps a
  // batch produced downstream of a stopping source (e.g. packets emitted by
  // an encoder during Flush) flowing through the graph without the scheduler
  // polling input queues in HandleIdle. When all inputs are empty, no
  // re-scheduling happens, so the queue becomes idle and HandleIdle can
  // terminate the graph.
  if (node->input_port_count() > 0 && HasPendingInput(node)) {
    auto* q = node->GetSchedulerQueue();
    if (q) q->AddNode(node);
  }

  // MediaPipe EndScheduling for SOURCE nodes: a source re-schedules itself
  // immediately after a successful Process() so it keeps producing without
  // waiting for the whole graph to go idle (previously sources were driven
  // only from HandleIdle, serializing source production behind every
  // downstream node and killing pipeline overlap). The source's MaxInFlight
  // (default 1) throttles the re-schedule naturally: AddNode() skips a node
  // already at its concurrency limit, so a slow downstream back-pressures the
  // source through the input stream's MaxInFlight + pending_count. A source
  // that returned Stop or an error never reaches here (handled above).
  if (node->input_port_count() == 0) {
    auto* q = node->GetSchedulerQueue();
    if (q) q->AddNode(node);
  }
}

void SchedulerQueue::UpdateIdleState() {
  if (idle_callback_) {
    idle_callback_(IsIdle());
  }
}

void SchedulerQueue::SubmitWaitingTasksToExecutor() {
  bool should_submit = false;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    should_submit = !queue_.empty() && running_;
  }
  if (should_submit) {
    SubmitToExecutor();
  }
}

}  // namespace graph::runtime
