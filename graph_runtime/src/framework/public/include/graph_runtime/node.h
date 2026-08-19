#ifndef GRAPH_RUNTIME_PUBLIC_NODE_H_
#define GRAPH_RUNTIME_PUBLIC_NODE_H_

// Public umbrella for graph_runtime's Node base class. External projects
// include "graph_runtime/node.h" (never the internal src/framework/node/node.h
// path) so the on-disk layout can evolve freely. NodeOptions/GraphContext and
// friends live in their own public headers below.

#include "graph_runtime/graph_runtime_export.h"
#include "src/framework/node/node.h"

#endif  // GRAPH_RUNTIME_PUBLIC_NODE_H_
