#pragma once

// Public API — macros only. The Logger class is internal.

#ifndef GRAPHRT_LOG_TAG
#define GRAPHRT_LOG_TAG "graphrt"
#endif

#define GRAPHRT_LOGD(msg)  ::graph::runtime::LogDebug(GRAPHRT_LOG_TAG, (msg))
#define GRAPHRT_LOGI(msg)  ::graph::runtime::LogInfo(GRAPHRT_LOG_TAG, (msg))
#define GRAPHRT_LOGW(msg)  ::graph::runtime::LogWarn(GRAPHRT_LOG_TAG, (msg))
#define GRAPHRT_LOGE(msg)  ::graph::runtime::LogError(GRAPHRT_LOG_TAG, (msg))
#define GRAPHRT_LOGF(msg)  ::graph::runtime::LogFatal(GRAPHRT_LOG_TAG, (msg))
