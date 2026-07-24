#pragma once

namespace graph::runtime::hook {

using HookFn = bool (*)(const void* data, int flags);

inline constexpr int kTypeLog = 1;

}  // namespace graph::runtime::hook
