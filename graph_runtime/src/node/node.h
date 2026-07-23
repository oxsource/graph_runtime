#ifndef GRAPH_RUNTIME_NODE_H_
#define GRAPH_RUNTIME_NODE_H_

#include <map>
#include <memory>
#include <string>

#include "absl/status/status.h"
#include "graph_runtime/src/stream/input_stream_manager.h"
#include "graph_runtime/src/stream/output_stream.h"
#include "graph_runtime/src/node/node_contract.h"
#include "graph_runtime/src/node/node_options.h"

namespace graph::runtime {

class SchedulerQueue;
class GraphContext;

class Node {
 public:
  virtual ~Node() = default;

  const std::string& name() const { return name_; }

  virtual absl::Status Open(GraphContext& context) = 0;
  virtual absl::Status Process(GraphContext& context) = 0;
  virtual absl::Status Close(GraphContext& context) = 0;

  void SetInputPort(const std::string& name, InputStreamManager* mgr) {
    input_ports_[name] = mgr;
  }
  void SetOutputPort(const std::string& name, OutputStream* stream) {
    output_ports_[name] = stream;
  }

  InputStreamManager* GetInputPort(const std::string& name) const {
    auto it = input_ports_.find(name);
    return it != input_ports_.end() ? it->second : nullptr;
  }
  OutputStream* GetOutputPort(const std::string& name) const {
    auto it = output_ports_.find(name);
    return it != output_ports_.end() ? it->second : nullptr;
  }

  size_t input_port_count() const { return input_ports_.size(); }
  size_t output_port_count() const { return output_ports_.size(); }

  void SetExecutorName(const std::string& executor_name) {
    executor_name_ = executor_name;
  }
  const std::string& ExecutorName() const { return executor_name_; }

  void SetSchedulerQueue(SchedulerQueue* queue) { scheduler_queue_ = queue; }
  SchedulerQueue* GetSchedulerQueue() const { return scheduler_queue_; }

  void SetSourceLayer(int layer) { source_layer_ = layer; }
  int SourceLayer() const { return source_layer_; }

  virtual Timestamp SourceProcessOrder(const GraphContext& context) const {
    return Timestamp::Min();
  }

 protected:
  explicit Node(std::string name) : name_(std::move(name)) {}

  std::string name_;
  std::map<std::string, InputStreamManager*> input_ports_;
  std::map<std::string, OutputStream*> output_ports_;
  std::string executor_name_;
  SchedulerQueue* scheduler_queue_ = nullptr;
  int source_layer_ = 0;
};

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_NODE_H_
