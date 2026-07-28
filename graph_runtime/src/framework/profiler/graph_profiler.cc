#include "src/framework/profiler/graph_profiler.h"

#include <algorithm>
#include <utility>

#include "src/framework/profiler/profile_writer.h"

namespace graph::runtime {

#ifdef GRAPH_RUNTIME_PROFILER_ENABLED

GraphProfiler::Scope::Scope(EventType type, const std::string& node_name,
                            GraphProfiler* profiler)
    : type_(type),
      node_name_(node_name),
      profiler_(profiler),
      start_time_usec_(0) {
  if (profiler_ && profiler_->is_profiling_.load(std::memory_order_relaxed)) {
    start_time_usec_ = profiler_->TimeNowUsec();
  }
}

GraphProfiler::Scope::~Scope() {
  if (!profiler_ || !profiler_->is_profiling_.load(std::memory_order_relaxed)) {
    return;
  }
  int64_t end_time_usec = profiler_->TimeNowUsec();
  switch (type_) {
    case EventType::OPEN:
      profiler_->SetOpenRuntime(node_name_, start_time_usec_, end_time_usec);
      break;
    case EventType::PROCESS:
      profiler_->AddProcessSample(node_name_, start_time_usec_, end_time_usec);
      break;
    case EventType::CLOSE:
      profiler_->SetCloseRuntime(node_name_, start_time_usec_, end_time_usec);
      break;
  }
}

void GraphProfiler::Initialize(
    const ProfilerConfig& config,
    const std::vector<std::string>& node_names) {
  config_ = config;
  if (!config_.enable_profiler) {
    return;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  node_data_.clear();
  for (const auto& name : node_names) {
    auto data = std::make_unique<PerNodeData>();
    data->process_runtime.Initialize(
        config_.histogram_interval_size_usec,
        config_.num_histogram_intervals);
    node_data_[name] = std::move(data);
  }
  if (!clock_) {
    clock_ = std::make_shared<RealClock>();
  }
  is_initialized_.store(true, std::memory_order_release);
}

void GraphProfiler::SetClock(std::shared_ptr<Clock> clock) {
  clock_ = std::move(clock);
}

void GraphProfiler::Start() {
  if (config_.enable_profiler &&
      is_initialized_.load(std::memory_order_acquire)) {
    is_profiling_.store(true, std::memory_order_release);
  }
}

void GraphProfiler::Stop() {
  is_profiling_.store(false, std::memory_order_release);
}

void GraphProfiler::Pause() {
  is_profiling_.store(false, std::memory_order_release);
}

void GraphProfiler::Resume() {
  if (config_.enable_profiler &&
      is_initialized_.load(std::memory_order_acquire)) {
    is_profiling_.store(true, std::memory_order_release);
  }
}

void GraphProfiler::Reset() {
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto& [name, data] : node_data_) {
    if (data) {
      data->open_runtime_usec.store(0, std::memory_order_relaxed);
      data->close_runtime_usec.store(0, std::memory_order_relaxed);
      data->process_runtime.Reset();
    }
  }
}

absl::Status GraphProfiler::WriteProfile(const std::string& path) {
  std::string output_path = path;
  if (output_path.empty() && !config_.trace_log_path.empty()) {
    absl::Time now = absl::Now();
    std::string ts = absl::FormatTime(
        absl::UnixEpoch() + absl::Seconds(
            absl::ToUnixSeconds(now)), absl::UTCTimeZone());
    output_path = config_.trace_log_path + "/profile_" + ts + ".json";
  }
  if (output_path.empty()) {
    return absl::InvalidArgumentError(
        "WriteProfile requires an explicit path or trace_log_path");
  }
  auto internal_profiles = GetNodeProfiles();
  std::vector<ProfileWriterNodeData> nodes;
  nodes.reserve(internal_profiles.size());
  for (const auto& ip : internal_profiles) {
    ProfileWriterNodeData node;
    node.node_name = ip.node_name;
    node.open_runtime_usec = ip.open_runtime_usec;
    node.close_runtime_usec = ip.close_runtime_usec;
    node.process_count = ip.process_runtime.count();
    node.process_time_total_usec = ip.process_runtime.total();
    node.process_time_mean_usec = ip.process_runtime.mean();
    node.histogram_interval_size_usec = ip.process_runtime.interval_size_usec();
    node.histogram_num_intervals = ip.process_runtime.num_intervals();
    node.histogram_buckets = ip.process_runtime.buckets();
    nodes.push_back(std::move(node));
  }
  return ::graph::runtime::WriteProfile(output_path, config_, nodes);
}

std::vector<GraphProfiler::NodeProfile> GraphProfiler::GetNodeProfiles() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<NodeProfile> result;
  result.reserve(node_data_.size());
  for (const auto& [name, data] : node_data_) {
    if (!data) continue;
    NodeProfile profile;
    profile.node_name = name;
    profile.open_runtime_usec =
        data->open_runtime_usec.load(std::memory_order_relaxed);
    profile.close_runtime_usec =
        data->close_runtime_usec.load(std::memory_order_relaxed);
    profile.process_runtime = data->process_runtime;
    result.push_back(std::move(profile));
  }
  return result;
}

const ProfilerConfig& GraphProfiler::profiler_config() const {
  return config_;
}

int64_t GraphProfiler::TimeNowUsec() {
  if (clock_) return clock_->TimeNowUsec();
  return RealClock().TimeNowUsec();
}

void GraphProfiler::SetOpenRuntime(const std::string& name,
                                   int64_t start_usec, int64_t end_usec) {
  int64_t duration = std::max(int64_t{0}, end_usec - start_usec);
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = node_data_.find(name);
  if (it != node_data_.end() && it->second) {
    it->second->open_runtime_usec.store(duration, std::memory_order_relaxed);
  }
}

void GraphProfiler::AddProcessSample(const std::string& name,
                                     int64_t start_usec, int64_t end_usec) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = node_data_.find(name);
  if (it != node_data_.end() && it->second) {
    it->second->process_runtime.AddSample(start_usec, end_usec);
  }
}

void GraphProfiler::SetCloseRuntime(const std::string& name,
                                    int64_t start_usec, int64_t end_usec) {
  int64_t duration = std::max(int64_t{0}, end_usec - start_usec);
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = node_data_.find(name);
  if (it != node_data_.end() && it->second) {
    it->second->close_runtime_usec.store(duration, std::memory_order_relaxed);
  }
}

#endif  // GRAPH_RUNTIME_PROFILER_ENABLED

}  // namespace graph::runtime
