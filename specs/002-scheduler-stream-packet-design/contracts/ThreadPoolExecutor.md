# Contract: ThreadPoolExecutor

**File**: `graph_runtime/src/scheduler/thread_pool_executor.h`

```cpp
namespace graph::runtime {

class ThreadPoolExecutor : public Executor {
 public:
  // Factory — called by executor registry during graph initialization.
  static absl::StatusOr<std::shared_ptr<Executor>> Create(
      const ExecutorOptions& options);

  // Create with explicit thread count.
  explicit ThreadPoolExecutor(int num_threads);

  ~ThreadPoolExecutor() override;

  // Schedule a task for execution on the thread pool.
  void Schedule(std::function<void()> task) override;

  int num_threads() const;

 private:
  class ThreadPool {
   public:
    ThreadPool(int num_threads);
    ~ThreadPool();
    void Schedule(std::function<void()> task);
   private:
    std::vector<std::thread> workers_;
    // internal work queue + synchronization
  };

  std::unique_ptr<ThreadPool> thread_pool_;
};

}  // namespace graph::runtime
```

**Semantics**:
- `Create()` parses options for `num_threads`, optional `stack_size`, `thread_name_prefix`.
- When `num_threads` <= 0: defaults to `min(CPUs, max(num_nodes, 1))`.
- `Schedule()` pushes the closure into the thread pool's work queue. Any idle worker thread picks it up.
- All worker threads are created during construction and joined during destruction. Threads persist across graph runs.
- Thread safety: `Schedule()` and `AddTask()` are fully thread-safe — multiple scheduler queues may submit tasks concurrently.

**Default thread count formula** (when not explicitly set):
```cpp
num_threads = std::min(
    std::thread::hardware_concurrency(),
    std::max({node_count, packet_generator_count, 1}));
```

**Registration**:
```cpp
RegisterExecutor("ThreadPoolExecutor", ThreadPoolExecutor::Create);
```
