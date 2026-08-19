#ifndef GRAPH_RUNTIME_PUBLIC_RUNTIME_H_
#define GRAPH_RUNTIME_PUBLIC_RUNTIME_H_

// Public entry for the GraphRuntime async driver class. External projects that
// build and run a graph use "graph_runtime/runtime.h" (never the internal
// src/framework/public/graph_runtime.h path). Runtime::Options (per-node-type
// node-option overrides) is defined here too via the internal header.

#include "graph_runtime/graph_runtime_export.h"
#include "src/framework/public/graph_runtime.h"

#endif  // GRAPH_RUNTIME_PUBLIC_RUNTIME_H_
