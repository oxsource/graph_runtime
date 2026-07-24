#include "src/hook/factory.h"

#include <algorithm>
#include <atomic>
#include <vector>

namespace graph::runtime::hook {

namespace {

struct Entry {
  int type;
  HookFn fn;
};

std::vector<Entry>* g_hooks = nullptr;
std::atomic_flag g_lock = ATOMIC_FLAG_INIT;

void EnsureInit() {
  if (g_hooks) return;
  while (g_lock.test_and_set(std::memory_order_acquire)) {}
  if (!g_hooks) {
    g_hooks = new std::vector<Entry>();
  }
  g_lock.clear(std::memory_order_release);
}

}  // namespace

void HookFactory::Register(int type, HookFn fn) {
  EnsureInit();
  auto it = std::find_if(g_hooks->begin(), g_hooks->end(),
                         [type](const Entry& e) { return e.type == type; });
  if (it != g_hooks->end()) {
    it->fn = fn;
  } else {
    g_hooks->push_back({type, fn});
  }
}

bool HookFactory::ForEachAccept(int type, const void* data, int flags) {
  if (!g_hooks) return false;
  for (const auto& e : *g_hooks) {
    if (e.type == type && e.fn) {
      return e.fn(data, flags);
    }
  }
  return false;
}

void HookFactory::ClearForTesting() {
  if (g_hooks) g_hooks->clear();
}

}  // namespace graph::runtime::hook
