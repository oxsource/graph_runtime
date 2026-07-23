#include "src/node/options_registry.h"

namespace graph::runtime {

std::map<std::string, OptionsRegistry::RegistryEntry>*
OptionsRegistry::Registry() {
  static auto* registry =
      new std::map<std::string, OptionsRegistry::RegistryEntry>();
  return registry;
}

bool OptionsRegistry::IsRegistered(const std::string& type_name) {
  return Registry()->find(type_name) != Registry()->end();
}

const std::vector<FieldDescriptor>* OptionsRegistry::GetDescriptors(
    const std::string& type_name) {
  auto it = Registry()->find(type_name);
  if (it == Registry()->end()) return nullptr;
  return &it->second.descriptors;
}

}  // namespace graph::runtime
