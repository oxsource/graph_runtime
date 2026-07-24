#ifndef GRAPH_RUNTIME_PUBLIC_LOGGER_SHIM_H_
#define GRAPH_RUNTIME_PUBLIC_LOGGER_SHIM_H_

// Shim for backward compatibility. Internal modules include this
// to get the Logger class. The real header moved to src/log/.
#include "src/log/logger.h"

#endif  // GRAPH_RUNTIME_PUBLIC_LOGGER_SHIM_H_
