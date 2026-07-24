#pragma once

#include "graph_runtime/graph_runtime_export.h"

namespace graph::runtime {

enum HookType : int {
  kHookTypeSentinel = 0,     // NULL terminator — marks end of table
  kHookTypeLogIntercept = 1, // Log interception hook
};

struct GRAPH_RUNTIME_API GraphHookEntity {
  int type;                                    // HookType value
  bool (*hook_fn)(const void* data, int flag); // Hook function, NULL for placeholder.
                                               // flag is an extensibility parameter,
                                               // default 0 (reserved for future use).
                                               // For kHookTypeLogIntercept, data points to
                                               // a null-terminated const char* (the fully
                                               // formatted log line). Future hook types
                                               // define their own payload struct.
};

// Hook table is owned and configured on the GraphRuntime instance.
// Internal modules (Logger, etc.) read hooks from the runtime's table
// by type, using an internal accessor. External consumers set hooks
// via GraphRuntime.

// Contract for the types and methods added to class GraphRuntime:
//
//   // Set the global hook table (sentinel-terminated GraphHookEntity array).
//   // The caller must ensure the table outlives all concurrent access.
//   // Pass nullptr to clear hooks and restore default behavior.
//   void SetGlobalHook(const GraphHookEntity* table);
//
//   // Return the first GraphHookEntity entry matching `type`, or nullptr.
//   // Example: GetGlobalHook(kHookTypeLogIntercept) returns the first
//   // log interception hook registered, or nullptr if none.
//   const GraphHookEntity* GetGlobalHook(int type) const;
//
// Example usage:
//   GraphRuntime runtime;
//   static const GraphHookEntity kHooks[] = {
//     { kHookTypeLogIntercept, MyLogHook },
//     { kHookTypeSentinel, nullptr },
//   };
//   runtime.SetGlobalHook(kHooks);

}  // namespace graph::runtime
