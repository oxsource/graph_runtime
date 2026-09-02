#ifndef GRAPH_RUNTIME_SCHEDULER_QUEUE_H_
#define GRAPH_RUNTIME_SCHEDULER_QUEUE_H_

#include <functional>
#include <mutex>
#include <queue>
#include <set>
#include <string>

#include "src/framework/scheduler/executor.h"
#include "src/framework/node/node.h"

#include "src/framework/profiler/graph_profiler.h"
#include "src/framework/public/types.h"

namespace graph::runtime {

class PerfCounters;


class SchedulerQueue : public TaskQueue {
 public:
  using IdleCallback = std::function<void(bool idle)>;
  using SourceStoppedCallback = std::function<void(Node*)>;

  struct Item {
    Node* node;
    bool is_open_node = false;
    int source_layer = 0;
    int64_t node_id = 0;

    bool operator<(const Item& other) const {
      // OpenNode tasks have highest priority
      if (is_open_node != other.is_open_node)
        return other.is_open_node;
      // Non-sources before sources
      bool is_source = (node && node->input_port_count() == 0);
      bool other_is_source = (other.node && other.node->input_port_count() == 0);
      if (is_source != other_is_source)
        return other_is_source;
      // Sources: lower layer first, then lower node_id
      if (is_source) {
        if (source_layer != other.source_layer)
          return source_layer > other.source_layer;
        return node_id > other.node_id;
      }
      // Non-sources: higher node_id first (leaf nodes)
      return node_id < other.node_id;
    }
  };

  explicit SchedulerQueue(std::string name);
  const std::string& name() const { return name_; }

  void SetExecutor(Executor* executor) { executor_ = executor; }
  void SetIdleCallback(IdleCallback cb) { idle_callback_ = std::move(cb); }
  void SetSourceStoppedCallback(SourceStoppedCallback cb) { source_stopped_callback_ = std::move(cb); }
  void SetErrorCallback(ErrorCallback cb) { error_callback_ = std::move(cb); }

  void SetRunning(bool running);
  bool IsRunning() const { return running_; }
  void SubmitWaitingTasksToExecutor();
  void Reset();
  void CleanupAfterRun();

  void AddNode(Node* node);
  void AddNodeForOpen(Node* node);
  void RunNextTask() override;

  // Pop the highest-priority pending item, or nullptr if none. Thread-safe.
  // Used by the executor worker to dequeue without racing producers.
  bool TryPop(Item* item);
  void OnTaskFinished();

  void SetPerfCounters(PerfCounters* counters) { perf_counters_ = counters; }
  void SetProfiler(ProfilingContext* profiler) { profiler_ = profiler; }

  bool IsIdle() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty() && num_pending_tasks_ == 0;
  }
  int NumPendingTasks() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return num_pending_tasks_;
  }

  // True if the node has completed its done-observation/finalize+close
  // sequence (see finalized_nodes_). Used by the scheduler to guarantee the
  // graph never terminates while a consumer has done+empty inputs but has not
  // yet observed input-done.
  bool IsFinalized(Node* node) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return finalized_nodes_.count(node) > 0;
  }

 private:
  void SubmitToExecutor();
  void RunNode(Node* node, bool is_open);
  void UpdateIdleState();

  std::string name_;
  Executor* executor_ = nullptr;
  IdleCallback idle_callback_;
  SourceStoppedCallback source_stopped_callback_;
  ErrorCallback error_callback_;
  PerfCounters* perf_counters_ = nullptr;
  ProfilingContext* profiler_ = nullptr;
  bool running_ = false;
  mutable std::mutex mutex_;
  std::priority_queue<Item> queue_;
  // Nodes whose inputs are all done but which have not yet run their finalize
  // Process (e.g. an encoder Flush). They are scheduled once more with outputs
  // still open, then their output streams are closed (see RunNode).
  std::set<Node*> close_pending_;
  // Nodes that have completed their done-observation/finalize sequence. They
  // must not be re-scheduled for finalize again (prevents a runaway loop when
  // the done-signal arrival was dropped by the MaxInFlight gate mid-run).
  std::set<Node*> finalized_nodes_;
  int num_pending_tasks_ = 0;
  int64_t timestamp_counter_ = 0;
};

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_SCHEDULER_QUEUE_H_
