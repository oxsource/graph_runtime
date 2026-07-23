#ifndef GRAPH_RUNTIME_NODE_OPTIONS_H_
#define GRAPH_RUNTIME_NODE_OPTIONS_H_

#include <any>
#include <map>
#include <string>
#include <vector>

#include "absl/status/statusor.h"

namespace graph::runtime {

class NodeOptions {
 public:
  NodeOptions() = default;

  template <typename T>
  void Set(const std::string& key, const T& value) {
    options_[key] = value;
  }

  template <typename T>
  const T* Get(const std::string& key) const {
    auto it = options_.find(key);
    if (it == options_.end()) return nullptr;
    try {
      return &std::any_cast<const T&>(it->second);
    } catch (const std::bad_any_cast&) {
      return nullptr;
    }
  }

  bool Has(const std::string& key) const {
    return options_.find(key) != options_.end();
  }

  std::vector<std::string> Keys() const {
    std::vector<std::string> keys;
    keys.reserve(options_.size());
    for (const auto& kv : options_) {
      keys.push_back(kv.first);
    }
    return keys;
  }

  std::any GetRaw(const std::string& key) const {
    auto it = options_.find(key);
    if (it == options_.end()) return std::any();
    return it->second;
  }

  void SetRaw(const std::string& key, const std::any& value) {
    options_[key] = value;
  }

 private:
  std::map<std::string, std::any> options_;
};

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_NODE_OPTIONS_H_
