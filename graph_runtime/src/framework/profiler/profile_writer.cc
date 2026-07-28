#include "src/framework/profiler/profile_writer.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>

#include "absl/time/clock.h"
#include "absl/time/time.h"

namespace graph::runtime {
namespace {

void WriteJsonString(std::ostream& os, const std::string& s) {
  os << '"';
  for (char c : s) {
    switch (c) {
      case '"': os << "\\\""; break;
      case '\\': os << "\\\\"; break;
      case '\n': os << "\\n"; break;
      case '\r': os << "\\r"; break;
      case '\t': os << "\\t"; break;
      default: os << c;
    }
  }
  os << '"';
}

void WriteProfilerConfigJson(std::ostream& os, const ProfilerConfig& config) {
  os << "{\n";
  os << "    \"enable_profiler\": "
     << (config.enable_profiler ? "true" : "false") << ",\n";
  os << "    \"histogram_interval_size_usec\": "
     << config.histogram_interval_size_usec << ",\n";
  os << "    \"num_histogram_intervals\": "
     << config.num_histogram_intervals << ",\n";
  os << "    \"trace_log_path\": ";
  WriteJsonString(os, config.trace_log_path);
  os << "\n  }";
}

void WriteNodeDataJson(std::ostream& os,
                       const ProfileWriterNodeData& node) {
  os << "  {\n";
  os << "    \"node_name\": ";
  WriteJsonString(os, node.node_name);
  os << ",\n";
  os << "    \"open_runtime_usec\": " << node.open_runtime_usec << ",\n";
  os << "    \"close_runtime_usec\": " << node.close_runtime_usec << ",\n";
  os << "    \"process_count\": " << node.process_count << ",\n";
  os << "    \"process_time_total_usec\": " << node.process_time_total_usec
     << ",\n";
  os << "    \"process_time_mean_usec\": " << node.process_time_mean_usec
     << ",\n";
  os << "    \"process_runtime\": {\n";
  os << "      \"interval_size_usec\": "
     << node.histogram_interval_size_usec << ",\n";
  os << "      \"num_intervals\": " << node.histogram_num_intervals << ",\n";
  os << "      \"count\": " << node.process_count << ",\n";
  os << "      \"total_usec\": " << node.process_time_total_usec << ",\n";
  os << "      \"buckets\": [";
  for (size_t i = 0; i < node.histogram_buckets.size(); ++i) {
    if (i > 0) os << ", ";
    os << node.histogram_buckets[i];
  }
  os << "]\n    }\n  }";
}

}  // namespace

absl::Status WriteProfile(const std::string& path,
                          const ProfilerConfig& config,
                          const std::vector<ProfileWriterNodeData>& nodes) {
  std::ofstream file(path);
  if (!file.is_open()) {
    return absl::InternalError("Failed to open file for writing: " + path);
  }

  absl::Time now = absl::Now();
  std::string capture_time = absl::FormatTime(now, absl::UTCTimeZone());

  file << "{\n";
  file << "  \"capture_time\": ";
  WriteJsonString(file, capture_time);
  file << ",\n";
  file << "  \"node_count\": " << nodes.size() << ",\n";
  file << "  \"profiler_config\": ";
  WriteProfilerConfigJson(file, config);
  file << ",\n";
  file << "  \"nodes\": [\n";
  for (size_t i = 0; i < nodes.size(); ++i) {
    if (i > 0) file << ",\n";
    WriteNodeDataJson(file, nodes[i]);
  }
  file << "\n  ]\n";
  file << "}\n";

  if (!file.good()) {
    return absl::InternalError("Failed to write profile to: " + path);
  }

  return absl::OkStatus();
}

}  // namespace graph::runtime
