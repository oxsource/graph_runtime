// log_intercept_demo.cc
// Demonstrates registering a log interception hook via GraphRuntime.
//
// Build: bazel build //src/examples:log_intercept_demo
// Run:   bazel run //src/examples:log_intercept_demo

#include <cstdio>

#define GRAPHRT_LOG_TAG "graphrt::demo"
#include "src/framework/log/logger.h"
#include "src/framework/public/graph_runtime.h"

namespace {

bool FileHook(const void* data, int /*flags*/) {
  std::fprintf(stderr, "[file hook] %s\n", static_cast<const char*>(data));
  return false;
}

bool FilterDebugHook(const void* data, int /*flags*/) {
  const char* line = static_cast<const char*>(data);
  for (int i = 0; line[i]; ++i) {
    if (line[i] == ' ' && line[i + 1] == 'D' && line[i + 2] == ' ') {
      return true;
    }
    if (line[i] == ' ') break;
  }
  return false;
}

}  // namespace

int main() {
  graph::runtime::GraphRuntime runtime;

  // Register hooks via runtime's public API.
  // Only one hook per type is allowed — the second replaces the first.
  runtime.SetHook(graph::runtime::hook::kTypeLog, FileHook);
  runtime.SetHook(graph::runtime::hook::kTypeLog, FilterDebugHook);

  // Log with the active hook
  graph::runtime::Logger::Info("info message — FilterDebugHook processes this");
  graph::runtime::Logger::Debug("debug message — FilterDebugHook suppresses this");
  graph::runtime::Logger::Error("error message — FilterDebugHook lets this through");

  return 0;
}
