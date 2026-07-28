#ifndef GRAPH_RUNTIME_CLOCK_H_
#define GRAPH_RUNTIME_CLOCK_H_

#include <cstdint>

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

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_CLOCK_H_
