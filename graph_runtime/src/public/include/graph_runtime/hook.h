#ifndef GRAPH_RUNTIME_PUBLIC_HOOK_H_
#define GRAPH_RUNTIME_PUBLIC_HOOK_H_

namespace graph::runtime::hook {

using HookFn = bool (*)(const void* data, int flags);

inline constexpr int kTypeLog = 1;

}  // namespace graph::runtime::hook

#endif  // GRAPH_RUNTIME_PUBLIC_HOOK_H_
