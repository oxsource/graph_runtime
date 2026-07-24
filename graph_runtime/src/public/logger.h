#ifndef GRAPH_RUNTIME_INTERNAL_LOGGER_H_
#define GRAPH_RUNTIME_INTERNAL_LOGGER_H_

#include "absl/synchronization/mutex.h"

namespace graph::runtime {

enum class LogLevel : int {
  kFatal = 0,
  kError = 1,
  kWarn = 2,
  kInfo = 3,
  kDebug = 4,
};

inline const char* LogLevelToString(LogLevel level) {
  switch (level) {
    case LogLevel::kFatal: return "F";
    case LogLevel::kError: return "E";
    case LogLevel::kWarn:  return "W";
    case LogLevel::kInfo:  return "I";
    case LogLevel::kDebug: return "D";
    default:               return "U";
  }
}

// Modules define GRAPHRT_LOG_TAG before including this header to set
// a default tag for no-tag overloads. Example:
//   #define GRAPHRT_LOG_TAG "graphrt::scheduler"
//   #include "src/public/logger.h"

class Logger {
 public:
  static void Debug(const char* tag, const char* content);
  static void Info(const char* tag, const char* content);
  static void Warn(const char* tag, const char* content);
  static void Error(const char* tag, const char* content);
  static void Fatal(const char* tag, const char* content);

  // No-tag overloads — use GRAPHRT_LOG_TAG (default "graphrt").
  static void Debug(const char* content);
  static void Info(const char* content);
  static void Warn(const char* content);
  static void Error(const char* content);
  static void Fatal(const char* content);

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
