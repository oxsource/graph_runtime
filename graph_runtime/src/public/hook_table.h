#ifndef GRAPH_RUNTIME_HOOK_TABLE_H_
#define GRAPH_RUNTIME_HOOK_TABLE_H_

namespace graph::runtime {

enum HookType : int {
  kHookTypeSentinel = 0,
  kHookTypeLogIntercept = 1,
};

struct GraphHookEntity {
  int type;
  bool (*hook_fn)(const void* data, int flag);
};

const GraphHookEntity* GetGlobalHookTable();

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_HOOK_TABLE_H_
