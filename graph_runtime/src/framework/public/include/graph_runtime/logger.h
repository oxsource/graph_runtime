#ifndef GRAPH_RUNTIME_PUBLIC_LOGGER_H_
#define GRAPH_RUNTIME_PUBLIC_LOGGER_H_

#include "graph_runtime/graph_runtime_export.h"

// Default TAG when module does not define its own.
#ifndef GRAPHRT_LOG_TAG
#define GRAPHRT_LOG_TAG "graphrt"
#endif

// Thin free-function wrappers consumed by the macros below.
// The Logger class is an internal implementation detail.
namespace graph::runtime {
  GRAPH_RUNTIME_API void LogDebug(const char* tag, const char* content);
  GRAPH_RUNTIME_API void LogInfo(const char* tag, const char* content);
  GRAPH_RUNTIME_API void LogWarn(const char* tag, const char* content);
  GRAPH_RUNTIME_API void LogError(const char* tag, const char* content);
  GRAPH_RUNTIME_API void LogFatal(const char* tag, const char* content);
}

// Public logging macros.
// Each macro uses GRAPHRT_LOG_TAG as the tag — modules define this before
// including graph_runtime.h to set their own default.
//
// Example:
//   #define GRAPHRT_LOG_TAG "graphrt::scheduler"
//   #include <graph_runtime/graph_runtime.h>
//
//   GRAPHRT_LOGI("pipeline started");
//   GRAPHRT_LOGE("failed: %s", err);

#define GRAPHRT_LOGD(msg)  ::graph::runtime::LogDebug(GRAPHRT_LOG_TAG, (msg))
#define GRAPHRT_LOGI(msg)  ::graph::runtime::LogInfo(GRAPHRT_LOG_TAG, (msg))
#define GRAPHRT_LOGW(msg)  ::graph::runtime::LogWarn(GRAPHRT_LOG_TAG, (msg))
#define GRAPHRT_LOGE(msg)  ::graph::runtime::LogError(GRAPHRT_LOG_TAG, (msg))
#define GRAPHRT_LOGF(msg)  ::graph::runtime::LogFatal(GRAPHRT_LOG_TAG, (msg))

#endif  // GRAPH_RUNTIME_PUBLIC_LOGGER_H_
