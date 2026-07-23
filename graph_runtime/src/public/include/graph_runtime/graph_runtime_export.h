#ifndef GRAPH_RUNTIME_GRAPH_RUNTIME_EXPORT_H_
#define GRAPH_RUNTIME_GRAPH_RUNTIME_EXPORT_H_

#if defined(_WIN32)
  #if defined(GRAPH_RUNTIME_SHARED_LIBRARY)
    #define GRAPH_RUNTIME_API __declspec(dllexport)
  #else
    #define GRAPH_RUNTIME_API __declspec(dllimport)
  #endif
#else
  #if defined(GRAPH_RUNTIME_SHARED_LIBRARY)
    #define GRAPH_RUNTIME_API __attribute__((visibility("default")))
  #else
    #define GRAPH_RUNTIME_API
  #endif
#endif

#endif  // GRAPH_RUNTIME_GRAPH_RUNTIME_EXPORT_H_
