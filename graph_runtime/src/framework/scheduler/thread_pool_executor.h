#ifndef GRAPH_RUNTIME_THREAD_POOL_EXECUTOR_H_
#define GRAPH_RUNTIME_THREAD_POOL_EXECUTOR_H_

#include <memory>
#include <thread>
#include <vector>

#include "src/scheduler/executor.h"

namespace graph::runtime {

class ThreadPoolExecutor : public Executor {
 public:
  static absl::StatusOr<std::shared_ptr<Executor>> Create(
      const ExecutorOptions& options);

  explicit ThreadPoolExecutor(int num_threads);
  ~ThreadPoolExecutor() override;

  void Schedule(std::function<void()> task) override;
  int num_threads() const { return num_threads_; }

 private:
  class ThreadPool {
   public:
    ThreadPool(int num_threads);
    ~ThreadPool();
    void Schedule(std::function<void()> task);
   private:
    std::vector<std::thread> workers_;
    std::deque<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stopped_ = false;
  };

  int num_threads_;
  std::unique_ptr<ThreadPool> thread_pool_;
};

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_THREAD_POOL_EXECUTOR_H_
