#ifndef GRAPH_RUNTIME_HOOK_FACTORY_H_
#define GRAPH_RUNTIME_HOOK_FACTORY_H_

#include "graph_runtime/hook.h"

namespace graph::runtime::hook {

class HookFactory {
 public:
  static void Register(int type, HookFn fn);
  static bool ForEachAccept(int type, const void* data, int flags);
  static void ClearForTesting();

 private:
  HookFactory() = default;
};

}  // namespace graph::runtime::hook

#endif  // GRAPH_RUNTIME_HOOK_FACTORY_H_
