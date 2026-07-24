#ifndef GRAPH_RUNTIME_NODE_REGISTRY_H_
#define GRAPH_RUNTIME_NODE_REGISTRY_H_

#include <memory>
#include <string>
#include <vector>

#include "src/node/node_factory.h"
#include "src/node/node_options.h"

namespace graph::runtime {

#define GRAPH_RUNTIME_REGISTER_NODE(type_name, node_class)              \
  static ::graph::runtime::internal::NodeRegistrationToken               \
      GRAPH_RUNTIME_NODE_REGISTRATION_##node_class##__ =                 \
          ::graph::runtime::internal::NodeRegistrationToken(              \
              ::graph::runtime::NodeFactoryRegistry::Register(            \
                  type_name,                                              \
                  std::make_unique<                                        \
                      ::graph::runtime::NodeFactoryFor<                    \
                          node_class>>()))

namespace internal {

class NodeRegistrationToken {
 public:
  explicit NodeRegistrationToken(int registration_id)
      : registration_id_(registration_id) {}
  ~NodeRegistrationToken();
  NodeRegistrationToken(const NodeRegistrationToken&) = delete;
  NodeRegistrationToken& operator=(const NodeRegistrationToken&) = delete;

 private:
  int registration_id_;
};

}  // namespace internal

class NodeFactoryRegistry {
 public:
  static int Register(const std::string& type_name,
                      std::unique_ptr<NodeFactory> factory);
  static bool Unregister(int registration_id);

  static std::unique_ptr<Node> CreateByName(
      const std::string& type_name,
      const std::string& node_name,
      const NodeOptions& options);

  static std::unique_ptr<Node> CreateByNameInNamespace(
      const std::string& ns, const std::string& type_name,
      const std::string& node_name, const NodeOptions& options);

  static NodeFactory* GetFactory(const std::string& type_name);
  static bool IsRegistered(const std::string& type_name);
  static std::vector<std::string> RegisteredTypes();

 private:
  NodeFactoryRegistry() = default;
};

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_NODE_REGISTRY_H_
