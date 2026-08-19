#ifndef GRAPH_RUNTIME_PUBLIC_UMBRELLA_H_
#define GRAPH_RUNTIME_PUBLIC_UMBRELLA_H_

// Public umbrella for graph_runtime. External projects include this single
// header (or individual "graph_runtime/..." headers) and NEVER the internal
// src/framework/... paths. This keeps the on-disk layout free to evolve and
// avoids clashing with a consumer's own src/... tree.

#include "graph_runtime/graph_runtime_export.h"
#include "graph_runtime/types.h"
#include "graph_runtime/timestamp.h"
#include "graph_runtime/packet.h"
#include "graph_runtime/graph_config.h"
#include "graph_runtime/side_packet.h"
#include "graph_runtime/logger.h"
#include "graph_runtime/hook.h"
#include "graph_runtime/profiler.h"

// GraphRuntime driver class (build/run a graph) + Runtime::Options:
#include "graph_runtime/runtime.h"

// Node implementation surface (used by node libraries):
#include "graph_runtime/node.h"
#include "graph_runtime/node_options.h"
#include "graph_runtime/node_contract.h"
#include "graph_runtime/node_registry.h"
#include "graph_runtime/graph_context.h"

// Config parsing / validation surface (used by drivers):
#include "graph_runtime/json_parser.h"
#include "graph_runtime/config_validator.h"

#endif  // GRAPH_RUNTIME_PUBLIC_UMBRELLA_H_
