#include "src/framework/profiler/time_histogram.h"

#include <algorithm>
#include <cmath>

namespace graph::runtime {

TimeHistogram::TimeHistogram(const TimeHistogram& other) {
  std::lock_guard<std::mutex> lock(other.mutex_);
  interval_size_usec_ = other.interval_size_usec_;
  num_intervals_ = other.num_intervals_;
  count_ = other.count_;
  total_ = other.total_;
  buckets_ = other.buckets_;
}

TimeHistogram& TimeHistogram::operator=(const TimeHistogram& other) {
  if (this == &other) return *this;
  std::lock_guard<std::mutex> lhs_lock(mutex_);
  std::lock_guard<std::mutex> rhs_lock(other.mutex_);
  interval_size_usec_ = other.interval_size_usec_;
  num_intervals_ = other.num_intervals_;
  count_ = other.count_;
  total_ = other.total_;
  buckets_ = other.buckets_;
  return *this;
}

void TimeHistogram::Initialize(int64_t interval_size_usec, int num_intervals) {
  std::lock_guard<std::mutex> lock(mutex_);
  interval_size_usec_ = interval_size_usec;
  num_intervals_ = num_intervals;
  count_ = 0;
  total_ = 0;
  buckets_.assign(num_intervals, 0);
}

void TimeHistogram::AddSample(int64_t start_time_usec, int64_t end_time_usec) {
  int64_t duration = end_time_usec - start_time_usec;
  if (duration < 0) duration = 0;

  std::lock_guard<std::mutex> lock(mutex_);
  ++count_;
  total_ += duration;
  int index = static_cast<int>(duration / interval_size_usec_);
  if (index >= num_intervals_) index = num_intervals_ - 1;
  if (index < 0) index = 0;
  ++buckets_[index];
}

void TimeHistogram::Reset() {
  std::lock_guard<std::mutex> lock(mutex_);
  count_ = 0;
  total_ = 0;
  std::fill(buckets_.begin(), buckets_.end(), 0);
}

int64_t TimeHistogram::count() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return count_;
}

int64_t TimeHistogram::total() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return total_;
}

double TimeHistogram::mean() const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (count_ == 0) return 0.0;
  return static_cast<double>(total_) / count_;
}

int64_t TimeHistogram::interval_size_usec() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return interval_size_usec_;
}

int TimeHistogram::num_intervals() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return num_intervals_;
}

const std::vector<int64_t>& TimeHistogram::buckets() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return buckets_;
}

}  // namespace graph::runtime
