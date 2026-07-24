#ifndef GRAPH_RUNTIME_PUBLIC_LOGGER_H_
#define GRAPH_RUNTIME_PUBLIC_LOGGER_H_

#include "graph_runtime/graph_runtime_export.h"

namespace graph::runtime {

// Modules define GRAPHRT_LOG_TAG before including this header to set
// a default tag for no-tag overloads.
// Example:
//   #define GRAPHRT_LOG_TAG "graphrt::scheduler"
//   #include "graph_runtime/graph_runtime.h"

class GRAPH_RUNTIME_API Logger {
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
};

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_PUBLIC_LOGGER_H_
