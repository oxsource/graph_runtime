#pragma once

#include "graph_runtime/graph_runtime_export.h"

namespace graph::runtime {

// Public Logger API. All methods are static — call directly via Logger::Info(...).
// Each internally maps to the corresponding LogLevel. The full Log(level, ...)
// method and LogLevel enum are internal (see src/public/logger.h).
class GRAPH_RUNTIME_API Logger {
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
};

}  // namespace graph::runtime
