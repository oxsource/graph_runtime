# Contract: NodeFactory

**File**: `graph_runtime/src/node/node_factory.h`

```cpp
namespace graph::runtime {

class NodeContract;
class Node;
struct NodeOptions;

// Abstract factory base — analogous to MediaPipe's NodeFactory.
// Each registered node type has one factory instance.
class NodeFactory {
 public:
  virtual ~NodeFactory() = default;

  // Called at graph construction time to declare and validate port types.
  // The implementation calls T::GetContract(contract) internally.
  virtual absl::Status GetContract(NodeContract* contract) = 0;

  // Called at graph run time to create a Node instance.
  virtual std::unique_ptr<Node> CreateNode(
      const std::string& name, const NodeOptions& options) = 0;
};

// Template implementation — analogous to MediaPipe's NodeFactoryFor<T>.
// T must inherit from Node and define a static GetContract method.
template <typename T>
class NodeFactoryFor : public NodeFactory {
  static_assert(std::is_base_of_v<Node, T>,
                "T must inherit from Node");

  static_assert(HasGetContract<T>(nullptr),
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
```

**Semantics**:
- `NodeFactory` is the polymorphic base class. Each node type has one persistent factory instance registered in `NodeFactoryRegistry`.
- `GetContract()` calls the node type's static `GetContract(NodeContract*)` method. This is called once during graph validation to declare port types and validate type compatibility with connected nodes.
- `CreateNode()` creates a new `Node` instance each time the graph runs (via `PrepareForRun`). The Node is destroyed after `CleanupAfterRun()`. This mirrors MediaPipe's per-run `CalculatorBase` lifecycle.
- `NodeFactoryFor<T>` provides the concrete implementation. The `static_assert(HasGetContract<T>)` ensures compile-time enforcement of the static contract method — similar to MediaPipe's `static_assert(CalculatorHasGetContract<T>)`.
