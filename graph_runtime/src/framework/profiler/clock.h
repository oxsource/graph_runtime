#ifndef GRAPH_RUNTIME_CLOCK_H_
#define GRAPH_RUNTIME_CLOCK_H_

#include <cstdint>
#include <vector>

namespace graph::runtime {

class Clock {
 public:
  virtual ~Clock() = default;
  virtual int64_t TimeNowUsec() = 0;
};

class RealClock : public Clock {
 public:
  int64_t TimeNowUsec() override;
};

class MockClock : public Clock {
 public:
  explicit MockClock(const std::vector<int64_t>& timestamps)
      : timestamps_(timestamps) {}

  int64_t TimeNowUsec() override {
    if (index_ < timestamps_.size()) {
      return timestamps_[index_++];
    }
    return 0;
  }

 private:
  std::vector<int64_t> timestamps_;
  size_t index_ = 0;
};

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_CLOCK_H_
