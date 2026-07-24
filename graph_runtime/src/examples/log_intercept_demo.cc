// log_intercept_demo.cc
// Demonstrates injecting an external log interception hook via the
// NULL-terminated GraphHookEntity sentinel table.
//
// Build: bazel build //src/examples:log_intercept_demo
// Run:   bazel run //src/examples:log_intercept_demo

#include <cstdio>
#include <string>

#define GRAPHRT_LOG_TAG "graphrt::demo"
#include "src/public/logger.h"
#include "src/public/hook_table.h"
#include "src/public/graph_runtime.h"

namespace {

// Hook that writes all logs to a file (stderr in this demo), returns false
// so default stdout output still occurs.
bool FileHook(const void* data, int /*flag*/) {
  const char* line = static_cast<const char*>(data);
  std::fprintf(stderr, "[file hook] %s\n", line);
  return false;
}

// Hook that suppresses DEBUG-level messages by returning true.
bool FilterDebugHook(const void* data, int /*flag*/) {
  const char* line = static_cast<const char*>(data);
  // Lines contain the level abbreviation at position after TAG.
  // Our format: "graphrt::demo I YYYY-MM-DD ..."
  if (line[0] == 'g') {
    // Find the level character (after first space)
    for (int i = 0; line[i]; ++i) {
      if (line[i] == ' ' && line[i + 1] == 'D' && line[i + 2] == ' ') {
        return true;  // suppress DEBUG
      }
      if (line[i] == ' ') break;  // only check first field
    }
  }
  return false;
}

}  // namespace

int main() {
  using namespace graph::runtime;

  // --- 1. Default logging (no hooks) ---
  Logger::Info("starting demo — log with no hooks");

  // --- 2. Register a hook table with two hooks ---
  static const GraphHookEntity kMyHooks[] = {
    { kHookTypeLogIntercept, FileHook },
    { kHookTypeLogIntercept, FilterDebugHook },
    { kHookTypeSentinel, nullptr },
  };

  GraphRuntime runtime;
  runtime.SetGlobalHook(kMyHooks);

  // --- 3. Log with hooks active ---
  Logger::Info("info message — both hooks process this");
  Logger::Debug("debug message — FileHook sees it, FilterDebugHook suppresses it");
  Logger::Error("error message — both hooks process this");

  // --- 4. Clear hooks and log again ---
  runtime.SetGlobalHook(nullptr);

  Logger::Info("hooks cleared — back to default stdout only");

  return 0;
}
