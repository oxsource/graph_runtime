#include "src/public/logger.h"
#include "src/public/hook_table.h"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <cstring>

namespace graph::runtime {

namespace {

int64_t NowMs() {
  auto now = std::chrono::system_clock::now();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      now.time_since_epoch());
  return ms.count();
}

}  // namespace

Logger& Logger::Instance() {
  static Logger* instance = new Logger();
  return *instance;
}

void Logger::Log(LogLevel level, const char* tag, const char* content) {
  int64_t ts_ms = NowMs();
  std::time_t sec = ts_ms / 1000;
  int msec = static_cast<int>(ts_ms % 1000);

  std::tm tm;
  localtime_r(&sec, &tm);

  char timestamp[24];
  std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm);

  char formatted[2048];
  int n = std::snprintf(formatted, sizeof(formatted),
                        "%s %s %s.%03d %s",
                        tag, LogLevelToString(level),
                        timestamp, msec, content);

  // Dispatch to registered hooks before acquiring the output mutex.
  // Iterate ALL kHookTypeLogIntercept entries. If any returns true,
  // skip the default write.
  bool suppressed = false;
  const GraphHookEntity* table = GetGlobalHookTable();
  if (table) {
    for (const GraphHookEntity* e = table; e->type != kHookTypeSentinel; ++e) {
      if (e->type == kHookTypeLogIntercept && e->hook_fn) {
        try {
          suppressed = e->hook_fn(formatted, 0) || suppressed;
        } catch (...) {
          // Hook threw — log a warning to stderr and fall back to default.
          std::fprintf(stderr, "graphrt W Hook threw exception, falling back to default output\n");
        }
      }
    }
  }
  if (suppressed) return;

  FILE* out = (level <= LogLevel::kError) ? stderr : stdout;
  absl::MutexLock lock(&output_mutex_);
  std::fwrite(formatted, 1, n < static_cast<int>(sizeof(formatted)) ? n : sizeof(formatted), out);
  std::fputc('\n', out);
  std::fflush(out);
}

#ifndef GRAPHRT_LOG_TAG
#define GRAPHRT_LOG_TAG "graphrt"
#endif

void Logger::Debug(const char* tag, const char* content) {
  Instance().Log(LogLevel::kDebug, tag, content);
}

void Logger::Debug(const char* content) {
  Instance().Log(LogLevel::kDebug, GRAPHRT_LOG_TAG, content);
}

void Logger::Info(const char* tag, const char* content) {
  Instance().Log(LogLevel::kInfo, tag, content);
}

void Logger::Info(const char* content) {
  Instance().Log(LogLevel::kInfo, GRAPHRT_LOG_TAG, content);
}

void Logger::Warn(const char* tag, const char* content) {
  Instance().Log(LogLevel::kWarn, tag, content);
}

void Logger::Warn(const char* content) {
  Instance().Log(LogLevel::kWarn, GRAPHRT_LOG_TAG, content);
}

void Logger::Error(const char* tag, const char* content) {
  Instance().Log(LogLevel::kError, tag, content);
}

void Logger::Error(const char* content) {
  Instance().Log(LogLevel::kError, GRAPHRT_LOG_TAG, content);
}

void Logger::Fatal(const char* tag, const char* content) {
  Instance().Log(LogLevel::kFatal, tag, content);
}

void Logger::Fatal(const char* content) {
  Instance().Log(LogLevel::kFatal, GRAPHRT_LOG_TAG, content);
}

}  // namespace graph::runtime
