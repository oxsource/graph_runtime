#ifndef GRAPH_RUNTIME_NODE_FACTORY_H_
#define GRAPH_RUNTIME_NODE_FACTORY_H_

#include <memory>
#include <string>
#include <type_traits>

#include "absl/status/status.h"
#include "src/framework/node/node_contract.h"
#include "src/framework/node/node_options.h"

namespace graph::runtime {

class Node;

namespace internal {

template <typename T>
auto HasGetContractImpl(int)
    -> decltype(T::GetContract(static_cast<NodeContract*>(nullptr)),
                std::true_type{});

template <typename T>
std::false_type HasGetContractImpl(...);

template <typename T>
constexpr bool HasGetContract =
    decltype(HasGetContractImpl<T>(0))::value;

}  // namespace internal

class NodeFactory {
 public:
  virtual ~NodeFactory() = default;

  virtual absl::Status GetContract(NodeContract* contract) = 0;
  virtual std::unique_ptr<Node> CreateNode(
      const std::string& name, const NodeOptions& options) = 0;
};

template <typename T>
class NodeFactoryFor : public NodeFactory {
  static_assert(std::is_base_of_v<Node, T>,
                "T must inherit from Node");
  static_assert(internal::HasGetContract<T>,
                "Node must define a static GetContract(NodeContract*) method");

  absl::Status GetContract(NodeContract* contract) override {
    return T::GetContract(contract);
  }

  std::unique_ptr<Node> CreateNode(
      const std::string& name, const NodeOptions& options) override {
    return std::make_unique<T>(name, options);
  }
};

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_NODE_FACTORY_H_
