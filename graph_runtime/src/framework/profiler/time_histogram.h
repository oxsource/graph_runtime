#ifndef GRAPH_RUNTIME_TIME_HISTOGRAM_H_
#define GRAPH_RUNTIME_TIME_HISTOGRAM_H_

#include <cstdint>
#include <mutex>
#include <vector>

namespace graph::runtime {

class TimeHistogram {
 public:
  TimeHistogram() = default;
  TimeHistogram(const TimeHistogram& other);
  TimeHistogram& operator=(const TimeHistogram& other);

  void Initialize(int64_t interval_size_usec, int num_intervals);
  void AddSample(int64_t start_time_usec, int64_t end_time_usec);
  void Reset();

  int64_t count() const;
  int64_t total() const;
  double mean() const;
  int64_t interval_size_usec() const;
  int num_intervals() const;
  const std::vector<int64_t>& buckets() const;

 private:
  int64_t interval_size_usec_ = 0;
  int num_intervals_ = 0;
  int64_t count_ = 0;
  int64_t total_ = 0;
  std::vector<int64_t> buckets_;
  mutable std::mutex mutex_;
};

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_TIME_HISTOGRAM_H_
