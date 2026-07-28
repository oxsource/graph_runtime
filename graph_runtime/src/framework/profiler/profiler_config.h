#ifndef GRAPH_RUNTIME_PROFILER_CONFIG_H_
#define GRAPH_RUNTIME_PROFILER_CONFIG_H_

#include <cstdint>
#include <string>

#include "graph_runtime/graph_runtime_export.h"

namespace graph::runtime {

struct GRAPH_RUNTIME_API ProfilerConfig {
  bool enable_profiler = false;
  int64_t histogram_interval_size_usec = 1000000;
  int num_histogram_intervals = 5;
  std::string trace_log_path;
};

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_PROFILER_CONFIG_H_
