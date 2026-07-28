#ifndef GRAPH_RUNTIME_PUBLIC_PROFILER_H_
#define GRAPH_RUNTIME_PUBLIC_PROFILER_H_

#include <cstdint>
#include <string>
#include <vector>

#include "graph_runtime/graph_runtime_export.h"
#include "src/framework/profiler/profiler_config.h"

namespace graph::runtime {

class ProfilingContext;

struct GRAPH_RUNTIME_API NodeProfile {
  std::string node_name;
  int64_t open_runtime_usec = 0;
  int64_t close_runtime_usec = 0;
  int64_t process_count = 0;
  int64_t process_time_total_usec = 0;
  double process_time_mean_usec = 0.0;
};

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_PUBLIC_PROFILER_H_
