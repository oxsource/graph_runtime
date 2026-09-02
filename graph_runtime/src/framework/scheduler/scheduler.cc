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

Scheduler::~Scheduler() {
  // Drop the executors (joining their worker threads and draining any queued
  // tasks) BEFORE the SchedulerQueues they reference are destroyed. The
  // ThreadPool destructor runs remaining tasks after `stopped_`; if the queues
  // were already freed, a worker would call RunNextTask on a destroyed queue
  // (SEGV). Nodes are still alive here (GraphRuntime resets the scheduler
  // before freeing all_nodes_), so in-flight RunNode tasks complete safely.
  non_default_executors_.clear();
  default_executor_.reset();
}

void Scheduler::SetInputStreamHandler(
    std::unique_ptr<InputStreamHandler> handler) {
  input_stream_handler_ = std::move(handler);
}

void Scheduler::SetErrorCallback(ErrorCallback cb) {
  error_callback_ = std::move(cb);
  // Propagate node Process errors (async path) to the scheduler: mark the
  // graph as failed and forward the original status to the registered
  // callback (so Close() sees the failure before nodes are closed).
  default_queue_.SetErrorCallback([this](const absl::Status& status) {
    has_error_ = true;
    if (error_callback_) error_callback_(status);
  });
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
  // Retain the shared_ptr so the queue's bare executor_ pointer stays valid.
  non_default_executors_[name] = std::move(executor);
  return absl::OkStatus();
}

void Scheduler::HandleIdle() {
  if (++handling_idle_ > 1) { --handling_idle_; return; }

  // When cancelled, force-terminate regardless of idle state.
  if (state_ == SchedulerState::kCancelling && has_error_) {
    for (auto* q : all_queues_) {
      q->CleanupAfterRun();
    }
    state_ = SchedulerState::kTerminated;
    cv_.notify_all();
    --handling_idle_;
    return;
  }

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

    bool no_more_sources = active_sources_.empty();

    // Deterministic drain (MediaPipe parity): a source-free graph may still
    // hold packets mid-flight — an encoder's done-triggered Flush, a batch
    // buffered in a downstream input queue, or a stream whose done signal has
    // not yet propagated from its upstream producer. Such a graph must not
    // terminate. "Input empty" alone is not sufficient: under parallel
    // execution a consumer can drain ahead of its producer, leaving its stream
    // empty but not yet done. Only when every non-source node's input streams
    // are both done and empty is the graph truly finished. Packets in an input
    // queue are drained by scheduling the owning node; a not-yet-done stream is
    // driven by its upstream producer, so here we simply keep waiting.
    if (no_more_sources && !has_error_ && !inputs_remaining &&
        state_ != SchedulerState::kCancelling) {
      switch (DrainInputQueues()) {
        case DrainStatus::kScheduled:
          // Buffered packets found and owning nodes rescheduled: defer
          // termination and await the next pass.
          --handling_idle_;
          return;
        case DrainStatus::kNotDone:
          // All input queues empty but not every stream done yet: the done
          // signal is still propagating from upstream producers. Those
          // consumers will be scheduled by their streams' arrival callbacks
          // when done arrives, so do not busy-wait here — break and await the
          // next event-driven HandleIdle.
          break;
        case DrainStatus::kDrained:
          // All input streams done and empty: the graph has fully drained.
          // Fall through to the normal termination check below.
          break;
      }
    }

    bool should_quit;
    if (has_error_) {
      should_quit = true;
    } else if (no_more_sources) {
      // MediaPipe parity (scheduler.cc HandleIdle): quit only when no source
      // node remains and every graph input stream is closed. Draining of
      // buffered downstream packets is event-driven (input-stream arrival
      // callbacks + post-process re-scheduling in SchedulerQueue::RunNode), so
      // idle queues imply drained input streams. The deterministic drain guard
      // above already guarantees every input is done and empty at this point.
      should_quit = !inputs_remaining;
    } else {
      should_quit = false;
    }

    if (should_quit) {
      if (error_callback_ && has_error_) {
        error_callback_(absl::InternalError("Graph execution error"));
      }
      CloseAllNodes();
      if (profiler_) profiler_->Stop();
      state_ = SchedulerState::kTerminated;
      cv_.notify_all();
      --handling_idle_;
      return;
    }

    if (!active_sources_.empty()) {
      for (auto* source : active_sources_) {
        // Use the source's own queue (respects executor binding); a source
        // also re-schedules itself after each Process (MediaPipe
        // EndScheduling), so this path is a safety net when a source was
        // back-pressured and parked.
        source->GetSchedulerQueue()->AddNode(source);
      }
      --handling_idle_;
      return;
    }

    if (inputs_remaining) {
      --handling_idle_;
      return;
    }

    // Nothing left to do (sources exhausted, inputs closed, queues idle):
    // the loop re-enters only when new work is added elsewhere.
    break;
  }
  --handling_idle_;
}

Scheduler::DrainStatus Scheduler::DrainInputQueues() {
  // Single pass: determine whether any non-source input stream still holds
  // packets and whether every stream has received its done signal. If any
  // queue is non-empty, reschedule its owning node for another pass so the
  // buffered packets are deterministically consumed; if some stream is not yet
  // done, do nothing here and rely on the upstream producer (or the arrival
  // callback) to schedule the consumer once done arrives.
  bool all_inputs_done = true;
  bool any_input_nonempty = false;
  for (auto* node : all_nodes_) {
    if (node->input_port_count() == 0) continue;
    for (const auto& [name, mgr] : node->InputPorts()) {
      if (!mgr->IsDone()) all_inputs_done = false;
      if (!mgr->IsEmpty()) any_input_nonempty = true;
    }
  }

  if (any_input_nonempty) {
    for (auto* node : all_nodes_) {
      if (node->input_port_count() == 0) continue;
      bool has_input = false;
      for (const auto& [name, mgr] : node->InputPorts()) {
        if (!mgr->IsEmpty()) {
          has_input = true;
          break;
        }
      }
      if (has_input) {
        auto* q = node->GetSchedulerQueue();
        if (q && q->IsRunning()) q->AddNode(node);
      }
    }
    return DrainStatus::kScheduled;
  }

  // All input queues are empty. A consumer may still not have OBSERVED the
  // done signal: the done-signal arrival can be dropped by the MaxInFlight
  // gate while its last packet-drain run was in flight, leaving the node idle
  // with done+empty inputs but unfinalized. The graph must not terminate
  // before every such node runs its finalize Process (observes input-done), so
  // schedule it and defer termination (MediaPipe kReadyForClose semantics).
  for (auto* node : all_nodes_) {
    if (node->input_port_count() == 0) continue;
    bool all_done = true;
    for (const auto& [name, mgr] : node->InputPorts()) {
      if (!mgr->IsDone()) {
        all_done = false;
        break;
      }
    }
    if (all_done) {
      auto* q = node->GetSchedulerQueue();
      if (q && !q->IsFinalized(node)) {
        q->AddNode(node);
        return DrainStatus::kScheduled;
      }
    }
  }

  return all_inputs_done ? DrainStatus::kDrained : DrainStatus::kNotDone;
}

absl::Status Scheduler::Schedule() {
  state_ = SchedulerState::kRunning;
  if (profiler_) profiler_->Start();

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
    ProfilingContext::Scope scope(
        ProfilingContext::EventType::OPEN, node->name(), profiler_);
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
      ProfilingContext::Scope src_scope(
          ProfilingContext::EventType::PROCESS, source->name(), profiler_);
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
        ProfilingContext::Scope ds_scope(
            ProfilingContext::EventType::PROCESS, downstream->name(), profiler_);
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
  CloseAllNodes();

  if (profiler_) profiler_->Stop();
  state_ = SchedulerState::kTerminated;
  cv_.notify_all();
  return absl::OkStatus();
}

void Scheduler::CloseAllNodes() {
  for (auto* node : all_nodes_) {
    if (node == nullptr) continue;
    ProfilingContext::Scope scope(
        ProfilingContext::EventType::CLOSE, node->name(), profiler_);
    InputStreamShardSet input_shards;
    OutputStreamShardSet output_shards;
    NodeOptions opts;
    GraphContext ctx(node->name(), reinterpret_cast<int64_t>(node),
                     "node", Timestamp::Done(),
                     &input_shards, &output_shards, &opts);
    ctx.SetInputSidePackets(input_side_packets_);
    auto status = node->Close(ctx);
    if (!status.ok()) {
      Logger::Error(std::string("Close error for " + node->name() + ": " +
                                std::string(status.ToString())).c_str());
    }
    Logger::Info(std::string("Closed " + node->name()).c_str());
    perf_counters_.nodes_closed.Increment();
  }
}

absl::Status Scheduler::Start() {
  if (profiler_) profiler_->Start();
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
    ProfilingContext::Scope scope(
        ProfilingContext::EventType::OPEN, node->name(), profiler_);
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
      node->GetSchedulerQueue()->AddNode(node);
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
    q->SetPerfCounters(&perf_counters_);
    q->SetProfiler(profiler_);
    q->SetRunning(true);
  }

  HandleIdle();
  return absl::OkStatus();
}

void Scheduler::AddedPacketToInputStream() {
  // Scheduling on packet arrival is handled by the per-stream arrival
  // callbacks wired in GraphRuntime::Initialize; here we only re-check graph
  // termination, mirroring MediaPipe's AddedPacketToGraphInputStream.
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

void Scheduler::Cancel() {
  if (has_error_) return;
  has_error_ = true;
  state_ = SchedulerState::kCancelling;
  if (state_ == SchedulerState::kPaused) {
    SetQueuesRunning(true);
  }
  HandleIdle();
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
