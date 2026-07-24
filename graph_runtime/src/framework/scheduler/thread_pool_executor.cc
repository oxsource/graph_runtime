#include "src/framework/scheduler/thread_pool_executor.h"

#include <algorithm>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

namespace graph::runtime {

ThreadPoolExecutor::ThreadPool::ThreadPool(int num_threads) {
  for (int i = 0; i < num_threads; ++i) {
    workers_.emplace_back([this] {
      while (true) {
        std::function<void()> task;
        {
          std::unique_lock<std::mutex> lock(mutex_);
          cv_.wait(lock, [this] { return stopped_ || !tasks_.empty(); });
          if (stopped_ && tasks_.empty()) return;
          task = std::move(tasks_.front());
          tasks_.pop_front();
        }
        task();
      }
    });
  }
}

ThreadPoolExecutor::ThreadPool::~ThreadPool() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stopped_ = true;
  }
  cv_.notify_all();
  for (auto& t : workers_) {
    if (t.joinable()) t.join();
  }
}

void ThreadPoolExecutor::ThreadPool::Schedule(std::function<void()> task) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    tasks_.push_back(std::move(task));
  }
  cv_.notify_one();
}

absl::StatusOr<std::shared_ptr<Executor>> ThreadPoolExecutor::Create(
    const ExecutorOptions& options) {
  int n = options.num_threads;
  if (n <= 0) {
    n = std::min(static_cast<int>(std::thread::hardware_concurrency()),
                 static_cast<int>(1));
    if (n < 1) n = 1;
  }
  return std::make_shared<ThreadPoolExecutor>(n);
}

ThreadPoolExecutor::ThreadPoolExecutor(int num_threads)
    : num_threads_(num_threads > 0 ? num_threads : 1),
      thread_pool_(std::make_unique<ThreadPool>(num_threads_)) {}

ThreadPoolExecutor::~ThreadPoolExecutor() = default;

void ThreadPoolExecutor::Schedule(std::function<void()> task) {
  thread_pool_->Schedule(std::move(task));
}

}  // namespace graph::runtime
