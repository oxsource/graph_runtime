#ifndef GRAPH_RUNTIME_COUNTERS_H_
#define GRAPH_RUNTIME_COUNTERS_H_

#include <atomic>
#include <cstdint>
#include <string>

namespace graph::runtime {

class Counter {
 public:
  explicit Counter(std::string name) : name_(std::move(name)) {}

  void Increment(int64_t delta = 1) { value_ += delta; }
  int64_t Value() const { return value_.load(); }
  const std::string& Name() const { return name_; }
  void Reset() { value_ = 0; }

 private:
  std::string name_;
  std::atomic<int64_t> value_{0};
};

class PerfCounters {
 public:
  Counter& Get(const std::string& name) {
    static Counter c(name);
    return c;
  }

  Counter tasks_submitted{"tasks_submitted"};
  Counter tasks_completed{"tasks_completed"};
  Counter packets_processed{"packets_processed"};
  Counter nodes_opened{"nodes_opened"};
  Counter nodes_closed{"nodes_closed"};
};

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_COUNTERS_H_
