#ifndef GRAPH_RUNTIME_PROFILE_WRITER_H_
#define GRAPH_RUNTIME_PROFILE_WRITER_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "src/framework/profiler/profiler_config.h"

namespace graph::runtime {

struct ProfileWriterNodeData {
  std::string node_name;
  int64_t open_runtime_usec = 0;
  int64_t close_runtime_usec = 0;
  int64_t process_count = 0;
  int64_t process_time_total_usec = 0;
  double process_time_mean_usec = 0.0;
  int64_t histogram_interval_size_usec = 0;
  int histogram_num_intervals = 0;
  std::vector<int64_t> histogram_buckets;
};

absl::Status WriteProfile(const std::string& path,
                          const ProfilerConfig& config,
                          const std::vector<ProfileWriterNodeData>& nodes);

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_PROFILE_WRITER_H_
