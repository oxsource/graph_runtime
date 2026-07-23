#ifndef GRAPH_RUNTIME_OPTIONS_REGISTRY_H_
#define GRAPH_RUNTIME_OPTIONS_REGISTRY_H_

#include <any>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "graph_runtime/src/node/node_options.h"

namespace graph::runtime {

struct FieldDescriptor {
  std::string name;
  std::function<void(void* obj, const std::any& value)> setter;
};

template <typename T>
class OptionsRegistrar {
 public:
  explicit OptionsRegistrar(const std::string& type_name)
      : type_name_(type_name) {}

  template <typename FieldType>
  OptionsRegistrar& Field(const std::string& field_name,
                          FieldType T::*member) {
    descriptors_.push_back(
        {field_name, [member](void* obj, const std::any& value) {
           auto* typed = static_cast<T*>(obj);
           typed->*member = std::any_cast<FieldType>(value);
         }});
    return *this;
  }

  const std::string& type_name() const { return type_name_; }
  const std::vector<FieldDescriptor>& descriptors() const {
    return descriptors_;
  }

 private:
  std::string type_name_;
  std::vector<FieldDescriptor> descriptors_;
};

class OptionsRegistry {
 public:
  template <typename T>
  static const OptionsRegistrar<T>& Register(const std::string& type_name);

  template <typename T>
  static T Deserialize(const NodeOptions& options);

  static bool IsRegistered(const std::string& type_name);
  static const std::vector<FieldDescriptor>* GetDescriptors(
      const std::string& type_name);

 private:
  struct RegistryEntry {
    std::vector<FieldDescriptor> descriptors;
  };
  static std::map<std::string, RegistryEntry>* Registry();
};

template <typename T>
const OptionsRegistrar<T>& OptionsRegistry::Register(
    const std::string& type_name) {
  static OptionsRegistrar<T> registrar(type_name);
  auto& entry = (*Registry())[type_name];
  entry.descriptors = registrar.descriptors();
  return registrar;
}

template <typename T>
T OptionsRegistry::Deserialize(const NodeOptions& options) {
  T obj{};
  auto* descs = GetDescriptors(
      // Use type name as fallback; callers must register.
      "");
  if (!descs) return obj;
  for (const auto& fd : *descs) {
    auto raw = options.GetRaw(fd.name);
    if (raw.has_value()) {
      fd.setter(&obj, raw);
    }
  }
  return obj;
}

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_OPTIONS_REGISTRY_H_
