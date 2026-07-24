#ifndef GRAPH_RUNTIME_INTERNAL_LOGGER_H_
#define GRAPH_RUNTIME_INTERNAL_LOGGER_H_

#include <cstdint>

#include "absl/synchronization/mutex.h"

namespace graph::runtime {

enum class LogLevel : int {
  kFatal = 0,
  kError = 1,
  kWarn = 2,
  kInfo = 3,
  kDebug = 4,
};

const char* LogLevelToString(LogLevel level);

struct LogMessage {
  LogLevel level;
  const char* tag;
  const char* message;
  int64_t timestamp_ms;
};

class Logger {
 public:
  static void Debug(const char* tag, const char* content);
  static void Info(const char* tag, const char* content);
  static void Warn(const char* tag, const char* content);
  static void Error(const char* tag, const char* content);
  static void Fatal(const char* tag, const char* content);

 private:
  Logger() = default;
  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;

  static Logger& Instance();
  void Log(LogLevel level, const char* tag, const char* content);

  absl::Mutex output_mutex_;
};

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_INTERNAL_LOGGER_H_
