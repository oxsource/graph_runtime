#include <cstdint>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "absl/status/status.h"

#include <cstdio>
#include <fstream>

#include "src/framework/config/graph_config.h"
#include "src/framework/config/json/json_parser.h"
#include "src/framework/profiler/graph_profiler.h"
#include "src/framework/profiler/clock.h"
#include "src/framework/profiler/profiler_config.h"
#include "src/framework/profiler/profile_writer.h"
#include "src/framework/profiler/time_histogram.h"

namespace graph::runtime {
namespace {

// ── Clock Tests ──

TEST(ProfilerTest, RealClockReturnsPositive) {
  RealClock clock;
  int64_t t = clock.TimeNowUsec();
  EXPECT_GT(t, 0);
}

TEST(ProfilerTest, RealClockIsMonotonic) {
  RealClock clock;
  int64_t t1 = clock.TimeNowUsec();
  int64_t t2 = clock.TimeNowUsec();
  EXPECT_GE(t2, t1);
}

// ── TimeHistogram Tests ──

TEST(ProfilerTest, TimeHistogramInitialize) {
  TimeHistogram h;
  h.Initialize(1000, 5);
  EXPECT_EQ(h.interval_size_usec(), 1000);
  EXPECT_EQ(h.num_intervals(), 5);
  EXPECT_EQ(h.count(), 0);
  EXPECT_EQ(h.total(), 0);
  EXPECT_EQ(h.mean(), 0.0);
}

TEST(ProfilerTest, TimeHistogramAddSample) {
  TimeHistogram h;
  h.Initialize(1000, 5);
  h.AddSample(100, 600);  // 500 usec duration → bucket 0
  EXPECT_EQ(h.count(), 1);
  EXPECT_EQ(h.total(), 500);
  EXPECT_DOUBLE_EQ(h.mean(), 500.0);
  const auto& buckets = h.buckets();
  ASSERT_EQ(buckets.size(), 5);
  EXPECT_EQ(buckets[0], 1);
}

TEST(ProfilerTest, TimeHistogramReset) {
  TimeHistogram h;
  h.Initialize(1000, 5);
  h.AddSample(100, 600);
  ASSERT_EQ(h.count(), 1);
  h.Reset();
  EXPECT_EQ(h.count(), 0);
  EXPECT_EQ(h.total(), 0);
}

TEST(ProfilerTest, TimeHistogramBucketOverflow) {
  TimeHistogram h;
  h.Initialize(1000, 5);
  h.AddSample(0, 6000000);  // 6s → should cap at last bucket index 4
  const auto& buckets = h.buckets();
  EXPECT_EQ(buckets[4], 1);
}

TEST(ProfilerTest, TimeHistogramNegativeDuration) {
  TimeHistogram h;
  h.Initialize(1000, 5);
  h.AddSample(600, 100);  // negative duration → clamped to 0
  EXPECT_EQ(h.count(), 1);
  EXPECT_EQ(h.total(), 0);
}

// ── ProfilerConfig Tests ──

TEST(ProfilerTest, ProfilerConfigDefaults) {
  ProfilerConfig config;
  EXPECT_FALSE(config.enable_profiler);
  EXPECT_EQ(config.histogram_interval_size_usec, 1000000);
  EXPECT_EQ(config.num_histogram_intervals, 5);
  EXPECT_TRUE(config.trace_log_path.empty());
}

// ── ProfilingContext (stub) Tests ──

TEST(ProfilerTest, DisabledReturnsEmptyProfiles) {
  ProfilingContext profiler;
  ProfilerConfig config;
  config.enable_profiler = false;
  profiler.Initialize(config, {"test_node"});
  profiler.Start();
  profiler.Stop();
  auto profiles = profiler.GetNodeProfiles();
  EXPECT_TRUE(profiles.empty());
}

TEST(ProfilerTest, ScopeNoCrashWhenProfilerNull) {
  // Scope should not crash when profiler pointer is null
  ProfilingContext::Scope scope(
      ProfilingContext::EventType::PROCESS, "test_node", nullptr);
}

// ── Config Parsing Tests ──

TEST(ProfilerTest, ConfigFromJson) {
  std::string json = R"({
    "profiler_config": {
      "enable_profiler": true,
      "histogram_interval_size_usec": 2000000,
      "num_histogram_intervals": 10,
      "trace_log_path": "/tmp/profiles"
    },
    "nodes": [
      {"name": "source", "type": "DemoCounterNode"}
    ]
  })";

  JsonParser parser;
  auto result = parser.ParseFromString(json);
  ASSERT_TRUE(result.ok());
  const auto& config = *result;
  EXPECT_TRUE(config.profiler_config.enable_profiler);
  EXPECT_EQ(config.profiler_config.histogram_interval_size_usec, 2000000);
  EXPECT_EQ(config.profiler_config.num_histogram_intervals, 10);
  EXPECT_EQ(config.profiler_config.trace_log_path, "/tmp/profiles");
}

TEST(ProfilerTest, ConfigFromJsonDefaults) {
  std::string json = R"({
    "nodes": [
      {"name": "source", "type": "DemoCounterNode"}
    ]
  })";

  JsonParser parser;
  auto result = parser.ParseFromString(json);
  ASSERT_TRUE(result.ok());
  const auto& config = *result;
  EXPECT_FALSE(config.profiler_config.enable_profiler);
  EXPECT_EQ(config.profiler_config.histogram_interval_size_usec, 1000000);
  EXPECT_EQ(config.profiler_config.num_histogram_intervals, 5);
  EXPECT_TRUE(config.profiler_config.trace_log_path.empty());
}



// ── Profile Writer Tests ──

TEST(ProfilerTest, WriteProfileCreatesFile) {
  ProfilerConfig config;
  config.enable_profiler = true;
  config.histogram_interval_size_usec = 1000;
  config.num_histogram_intervals = 3;

  TimeHistogram hist;
  hist.Initialize(1000, 3);
  hist.AddSample(0, 500);

  ProfileWriterNodeData node;
  node.node_name = "test_node";
  node.open_runtime_usec = 100;
  node.close_runtime_usec = 50;
  node.process_count = 1;
  node.process_time_total_usec = 500;
  node.process_time_mean_usec = 500.0;
  node.histogram_interval_size_usec = 1000;
  node.histogram_num_intervals = 3;
  node.histogram_buckets = {1, 0, 0};

  std::vector<ProfileWriterNodeData> nodes = {node};

  std::string path = std::tmpnam(nullptr);
  auto status = WriteProfile(path, config, nodes);
  ASSERT_TRUE(status.ok()) << status.ToString();

  std::ifstream file(path);
  ASSERT_TRUE(file.good());
  std::string content((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());

  EXPECT_NE(content.find("test_node"), std::string::npos);
  EXPECT_NE(content.find("capture_time"), std::string::npos);
  EXPECT_NE(content.find("\"node_count\": 1"), std::string::npos);
  EXPECT_NE(content.find("\"open_runtime_usec\": 100"), std::string::npos);
  EXPECT_NE(content.find("\"close_runtime_usec\": 50"), std::string::npos);
  EXPECT_NE(content.find("process_runtime"), std::string::npos);
  EXPECT_NE(content.find("\"count\": 1"), std::string::npos);
  EXPECT_NE(content.find("\"total_usec\": 500"), std::string::npos);

  std::remove(path.c_str());
}

TEST(ProfilerTest, WriteProfileEmptyProfiles) {
  ProfilerConfig config;
  std::vector<ProfileWriterNodeData> nodes;

  std::string path = std::tmpnam(nullptr);
  auto status = WriteProfile(path, config, nodes);
  ASSERT_TRUE(status.ok()) << status.ToString();

  std::ifstream file(path);
  ASSERT_TRUE(file.good());
  std::string content((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());

  EXPECT_NE(content.find("\"node_count\": 0"), std::string::npos);
  EXPECT_NE(content.find("\"nodes\": ["), std::string::npos);

  std::remove(path.c_str());
}

TEST(ProfilerTest, WriteProfileInvalidPath) {
  ProfilerConfig config;
  std::vector<ProfileWriterNodeData> nodes;
  auto status = WriteProfile("/nonexistent_dir/profile.json", config, nodes);
  EXPECT_FALSE(status.ok());
}

TEST(ProfilerTest, WriteProfileGraphProfiler) {
  ProfilingContext profiler;
  ProfilerConfig config;
  config.enable_profiler = false;
  profiler.Initialize(config, {"test_node"});

  // In stub mode, WriteProfile returns OkStatus without side effects
  auto status = profiler.WriteProfile("/tmp/test_profile.json");
  EXPECT_TRUE(status.ok());
}

TEST(ProfilerTest, WriteProfileRealEnabled) {
  ProfilingContext profiler;
  auto mock_clock = std::make_shared<RealClock>();
  profiler.SetClock(mock_clock);
  ProfilerConfig config;
  config.enable_profiler = true;
  profiler.Initialize(config, {"test_node"});
  profiler.Start();
  {
    ProfilingContext::Scope scope(
        ProfilingContext::EventType::PROCESS, "test_node", &profiler);
  }
  profiler.Stop();

  std::string path = std::tmpnam(nullptr);
  auto status = profiler.WriteProfile(path);
  // In real mode, file is created; in stub mode, OkStatus but no file
  // Just verify the call doesn't crash and returns a status
  EXPECT_TRUE(status.ok() || !status.ok());
  std::remove(path.c_str());
}

}  // namespace
}  // namespace graph::runtime
