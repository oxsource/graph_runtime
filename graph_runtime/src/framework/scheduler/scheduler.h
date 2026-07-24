#ifndef GRAPH_RUNTIME_SCHEDULER_H_
#define GRAPH_RUNTIME_SCHEDULER_H_

#include <atomic>
#include <atomic>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "src/framework/scheduler/executor.h"
#include "src/framework/scheduler/scheduler_queue.h"
#include "src/framework/node/node.h"
#include "src/framework/node/graph_context.h"
#include "src/framework/stream/input_stream_manager.h"
#include "src/framework/stream/output_stream_handler.h"
#include "src/framework/public/types.h"
#include "src/framework/public/side_packet.h"
#include "src/framework/scheduler/counters.h"

namespace graph::runtime {

class InputStreamHandler;

enum class SchedulerState {
  kNotStarted = 0,
  kRunning = 1,
  kPaused = 2,
  kCancelling = 3,
  kTerminated = 4,
};

class Scheduler {
 public:
  Scheduler();
  virtual ~Scheduler() = default;

  void SetInputStreamHandler(std::unique_ptr<InputStreamHandler> handler);
  void SetErrorCallback(ErrorCallback cb);

  virtual absl::Status SetDefaultExecutor(std::shared_ptr<Executor> executor);
  virtual absl::Status SetNonDefaultExecutor(
      const std::string& name, std::shared_ptr<Executor> executor);

  virtual absl::Status Schedule();
  virtual absl::Status Start();
  virtual absl::Status WaitUntilDone();
  virtual absl::Status WaitForIdle();
  virtual void Shutdown();
  virtual absl::Status Pause();
  virtual absl::Status Resume();

  SchedulerState state() const { return state_; }
  bool IsTerminated() const { return state_ == SchedulerState::kTerminated; }
  bool IsPaused() const { return state_ == SchedulerState::kPaused; }
  bool HasError() const { return has_error_; }
  bool IsIdle() const {
    for (auto* q : all_queues_) {
      if (!q->IsIdle()) return false;
    }
    return true;
  }
  bool HasGraphFinished() const {
    return state_ == SchedulerState::kTerminated;
  }

  void AssignNodeToQueue(Node* node);

  void SetNodes(const std::vector<Node*>& nodes) { all_nodes_ = nodes; }

  virtual absl::Status AddNode(Node* node);
  virtual absl::Status RemoveNode(Node* node);

  void HandleIdle();
  void AddedPacketToGraphInputStream();
  void SetTotalGraphInputStreams(int n) { total_graph_input_streams_ = n; }
  void IncClosedGraphInputStreams() { ++num_closed_graph_input_streams_; }

  void SetInputSidePackets(const PacketSet& ps) { input_side_packets_ = ps; }
  const PacketSet& GetInputSidePackets() const { return input_side_packets_; }

  void SetQueuesRunning(bool running);

 protected:
  SchedulerQueue& GetQueue(const std::string& executor_name);
  void OnNodeOpened(Node* node);

  SchedulerState state_ = SchedulerState::kNotStarted;
  bool stopping_ = false;
  bool has_error_ = false;
  int non_idle_queue_count_ = 0;
  std::atomic<int> handling_idle_{0};
  std::set<Node*> active_sources_;

  SchedulerQueue default_queue_{"default"};
  std::map<std::string, std::unique_ptr<SchedulerQueue>> non_default_queues_;
  std::vector<SchedulerQueue*> all_queues_;

  std::unique_ptr<InputStreamHandler> input_stream_handler_;
  std::shared_ptr<Executor> default_executor_;
  ErrorCallback error_callback_;
  std::vector<Node*> all_nodes_;
  std::vector<Node*> source_nodes_;
  int processed_count_ = 0;

  // Async source management
  int num_closed_graph_input_streams_ = 0;
  int total_graph_input_streams_ = 0;
  int throttled_graph_input_stream_count_ = 0;

  std::mutex mutex_;
  std::condition_variable cv_;

  PacketSet input_side_packets_;

  PerfCounters perf_counters_;
};

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_SCHEDULER_H_
