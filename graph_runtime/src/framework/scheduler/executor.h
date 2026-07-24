#ifndef GRAPH_RUNTIME_EXECUTOR_H_
#define GRAPH_RUNTIME_EXECUTOR_H_

#include <functional>
#include <memory>
#include <string>

#include "absl/status/statusor.h"

namespace graph::runtime {

class TaskQueue {
 public:
  virtual ~TaskQueue() = default;
  virtual void RunNextTask() = 0;
};

struct ExecutorOptions {
  int num_threads = 0;
};

class Executor {
 public:
  virtual ~Executor() = default;

  virtual void AddTask(TaskQueue* task_queue) {
    Schedule([task_queue] { task_queue->RunNextTask(); });
  }

  virtual void Schedule(std::function<void()> task) = 0;
};

using ExecutorFactory = std::function<absl::StatusOr<std::shared_ptr<Executor>>(const ExecutorOptions&)>;

void RegisterExecutor(const std::string& type_name, ExecutorFactory factory);

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_EXECUTOR_H_
