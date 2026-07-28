#ifndef GRAPH_RUNTIME_REPORTER_H_
#define GRAPH_RUNTIME_REPORTER_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/status.h"

namespace graph::runtime {

struct ProfileReport {
  struct NodeStats {
    std::string node_name;
    int64_t open_runtime_usec = 0;
    int64_t close_runtime_usec = 0;
    int64_t process_count = 0;
    int64_t process_time_total_usec = 0;
    double process_time_mean_usec = 0.0;
    int64_t process_time_min_usec = 0;
    int64_t process_time_max_usec = 0;
  };

  std::vector<NodeStats> nodes;
  int64_t total_process_count = 0;
  int64_t total_process_time_usec = 0;
};

class Reporter {
 public:
  absl::Status Accumulate(const std::string& json_path);

  ProfileReport Report() const;

  struct Delta {
    std::string node_name;
    double process_mean_delta_usec;
    double process_mean_delta_pct;
  };
  std::vector<Delta> Compare(const ProfileReport& baseline) const;

  void Clear();

 private:
  struct PerNodeAccum {
    std::string node_name;
    int64_t open_runtime_usec = 0;
    int64_t close_runtime_usec = 0;
    int64_t process_count = 0;
    int64_t process_time_total_usec = 0;
    int64_t process_time_min_usec = INT64_MAX;
    int64_t process_time_max_usec = 0;
    int samples = 0;
  };

  std::vector<PerNodeAccum> node_data_;
};

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_REPORTER_H_
