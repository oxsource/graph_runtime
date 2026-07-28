#include "src/framework/profiler/graph_profiler.h"

namespace graph::runtime {

#ifdef GRAPH_RUNTIME_PROFILER_ENABLED

GraphProfiler::Scope::Scope(EventType type, const std::string& node_name,
                            GraphProfiler* profiler)
    : type_(type), node_name_(node_name), profiler_(profiler) {}

GraphProfiler::Scope::~Scope() {}

void GraphProfiler::Initialize(
    const ProfilerConfig& config,
    const std::vector<std::string>& node_names) {
  config_ = config;
}

void GraphProfiler::SetClock(std::shared_ptr<Clock> clock) {}

void GraphProfiler::Start() {}
void GraphProfiler::Stop() {}
void GraphProfiler::Pause() {}
void GraphProfiler::Resume() {}
void GraphProfiler::Reset() {}

absl::Status GraphProfiler::WriteProfile(const std::string& path) {
  return absl::OkStatus();
}

std::vector<GraphProfiler::NodeProfile> GraphProfiler::GetNodeProfiles() const {
  return {};
}

const ProfilerConfig& GraphProfiler::profiler_config() const {
  return config_;
}

int64_t GraphProfiler::TimeNowUsec() { return 0; }

void GraphProfiler::SetOpenRuntime(const std::string& name,
                                   int64_t start_usec, int64_t end_usec) {}
void GraphProfiler::AddProcessSample(const std::string& name,
                                     int64_t start_usec, int64_t end_usec) {}
void GraphProfiler::SetCloseRuntime(const std::string& name,
                                    int64_t start_usec, int64_t end_usec) {}

#endif  // GRAPH_RUNTIME_PROFILER_ENABLED

}  // namespace graph::runtime
