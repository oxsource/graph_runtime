#ifndef GRAPH_RUNTIME_SCHEDULER_QUEUE_H_
#define GRAPH_RUNTIME_SCHEDULER_QUEUE_H_

#include <functional>
#include <queue>
#include <string>

#include "src/framework/scheduler/executor.h"
#include "src/framework/node/node.h"

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

  void SetRunning(bool running);
  bool IsRunning() const { return running_; }
  void SubmitWaitingTasksToExecutor();
  void Reset();
  void CleanupAfterRun();

  void AddNode(Node* node);
  void AddNodeForOpen(Node* node);
  void RunNextTask() override;

  void SetPerfCounters(PerfCounters* counters) { perf_counters_ = counters; }

  bool IsIdle() const { return queue_.empty() && num_pending_tasks_ == 0; }
  int NumPendingTasks() const { return num_pending_tasks_; }

 private:
  void SubmitToExecutor();
  void RunNode(Node* node, bool is_open);
  void UpdateIdleState();

  std::string name_;
  Executor* executor_ = nullptr;
  IdleCallback idle_callback_;
  SourceStoppedCallback source_stopped_callback_;
  PerfCounters* perf_counters_ = nullptr;
  bool running_ = false;
  std::priority_queue<Item> queue_;
  int num_pending_tasks_ = 0;
  int64_t timestamp_counter_ = 0;
};

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_SCHEDULER_QUEUE_H_
