#include "src/framework/profiler/clock.h"

#include <chrono>

namespace graph::runtime {

int64_t RealClock::TimeNowUsec() {
  return std::chrono::duration_cast<std::chrono::microseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

}  // namespace graph::runtime
