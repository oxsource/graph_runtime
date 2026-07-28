#ifndef GRAPH_RUNTIME_GRAPH_PROFILER_H_
#define GRAPH_RUNTIME_GRAPH_PROFILER_H_

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "src/framework/profiler/clock.h"
#include "src/framework/profiler/profiler_config.h"
#include "src/framework/profiler/time_histogram.h"

namespace graph::runtime {

#ifdef GRAPH_RUNTIME_PROFILER_ENABLED

class GraphProfiler {
 public:
  enum class EventType { OPEN, PROCESS, CLOSE };

  class Scope {
   public:
    Scope(EventType type, const std::string& node_name,
          GraphProfiler* profiler);
    ~Scope();

   private:
    EventType type_;
    std::string node_name_;
    GraphProfiler* profiler_;
    int64_t start_time_usec_ = 0;
  };

  void Initialize(const ProfilerConfig& config,
                  const std::vector<std::string>& node_names);
  void SetClock(std::shared_ptr<Clock> clock);
  void Start();
  void Stop();
  void Pause();
  void Resume();
  void Reset();
  absl::Status WriteProfile(const std::string& path);

  struct NodeProfile {
    std::string node_name;
    int64_t open_runtime_usec = 0;
    int64_t close_runtime_usec = 0;
    TimeHistogram process_runtime;
  };
  std::vector<NodeProfile> GetNodeProfiles() const;
  const ProfilerConfig& profiler_config() const;

 private:
  int64_t TimeNowUsec();
  void SetOpenRuntime(const std::string& name,
                      int64_t start_usec, int64_t end_usec);
  void AddProcessSample(const std::string& name,
                        int64_t start_usec, int64_t end_usec);
  void SetCloseRuntime(const std::string& name,
                       int64_t start_usec, int64_t end_usec);

  struct PerNodeData {
    std::atomic<int64_t> open_runtime_usec{0};
    std::atomic<int64_t> close_runtime_usec{0};
    TimeHistogram process_runtime;
  };

  ProfilerConfig config_;
  std::atomic<bool> is_profiling_{false};
  std::atomic<bool> is_initialized_{false};
  std::shared_ptr<Clock> clock_;
  mutable std::mutex mutex_;
  std::map<std::string, std::unique_ptr<PerNodeData>> node_data_;
};

class ProfilingContext : public GraphProfiler {
  using GraphProfiler::GraphProfiler;
};

#else  // GRAPH_RUNTIME_PROFILER_ENABLED

class GraphProfilerStub {
 public:
  enum class EventType { OPEN, PROCESS, CLOSE };
  class Scope {
   public:
    Scope(EventType, const std::string&, GraphProfilerStub*) {}
    ~Scope() {}
  };
  void Initialize(const ProfilerConfig&, const std::vector<std::string>&) {}
  void SetClock(std::shared_ptr<Clock>) {}
  void Start() {}
  void Stop() {}
  void Pause() {}
  void Resume() {}
  void Reset() {}
  absl::Status WriteProfile(const std::string&) {
    return absl::OkStatus();
  }
  struct NodeProfile {
    std::string node_name;
    int64_t open_runtime_usec = 0;
    int64_t close_runtime_usec = 0;
    TimeHistogram process_runtime;
  };
  std::vector<NodeProfile> GetNodeProfiles() const { return {}; }
  const ProfilerConfig& profiler_config() const { return config_; }

 private:
  ProfilerConfig config_;
};

class ProfilingContext : public GraphProfilerStub {
  using GraphProfilerStub::GraphProfilerStub;
};

#endif  // GRAPH_RUNTIME_PROFILER_ENABLED

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_GRAPH_PROFILER_H_
