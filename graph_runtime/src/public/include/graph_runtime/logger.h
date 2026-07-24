#ifndef GRAPH_RUNTIME_PUBLIC_LOGGER_H_
#define GRAPH_RUNTIME_PUBLIC_LOGGER_H_

#include "graph_runtime/graph_runtime_export.h"

namespace graph::runtime {

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
};

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_PUBLIC_LOGGER_H_
