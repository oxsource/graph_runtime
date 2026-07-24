#include "src/framework/node/node_registry.h"
#include "src/framework/node/node.h"

#include <map>
#include <mutex>

namespace graph::runtime {

namespace internal {

NodeRegistrationToken::~NodeRegistrationToken() {
  NodeFactoryRegistry::Unregister(registration_id_);
}

}  // namespace internal

namespace {

struct RegistryData {
  std::map<std::string, std::unique_ptr<NodeFactory>> factories;
  std::map<int, std::string> id_to_name;
  int next_id = 1;
  std::mutex mutex;
};

RegistryData& Data() {
  static RegistryData data;
  return data;
}

}  // namespace

int NodeFactoryRegistry::Register(const std::string& type_name,
                                   std::unique_ptr<NodeFactory> factory) {
  auto& data = Data();
  std::lock_guard<std::mutex> lock(data.mutex);
  int id = data.next_id++;
  data.factories[type_name] = std::move(factory);
  data.id_to_name[id] = type_name;
  return id;
}

bool NodeFactoryRegistry::Unregister(int registration_id) {
  auto& data = Data();
  std::lock_guard<std::mutex> lock(data.mutex);
  auto it = data.id_to_name.find(registration_id);
  if (it == data.id_to_name.end()) return false;
  data.factories.erase(it->second);
  data.id_to_name.erase(it);
  return true;
}

std::unique_ptr<Node> NodeFactoryRegistry::CreateByName(
    const std::string& type_name,
    const std::string& node_name,
    const NodeOptions& options) {
  auto* factory = GetFactory(type_name);
  if (!factory) return nullptr;
  return factory->CreateNode(node_name, options);
}

std::unique_ptr<Node> NodeFactoryRegistry::CreateByNameInNamespace(
    const std::string& ns, const std::string& type_name,
    const std::string& node_name, const NodeOptions& options) {
  std::string qualified = ns.empty() ? type_name : ns + "::" + type_name;
  return CreateByName(qualified, node_name, options);
}

NodeFactory* NodeFactoryRegistry::GetFactory(
    const std::string& type_name) {
  auto& data = Data();
  std::lock_guard<std::mutex> lock(data.mutex);
  auto it = data.factories.find(type_name);
  if (it == data.factories.end()) return nullptr;
  return it->second.get();
}

bool NodeFactoryRegistry::IsRegistered(const std::string& type_name) {
  return GetFactory(type_name) != nullptr;
}

std::vector<std::string> NodeFactoryRegistry::RegisteredTypes() {
  auto& data = Data();
  std::lock_guard<std::mutex> lock(data.mutex);
  std::vector<std::string> types;
  for (const auto& kv : data.factories) {
    types.push_back(kv.first);
  }
  return types;
}

}  // namespace graph::runtime
