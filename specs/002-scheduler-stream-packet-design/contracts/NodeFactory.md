# Contract: NodeFactory

**File**: `graph_runtime/src/node/node_factory.h`

```cpp
namespace graph::runtime {

using NodeCreator = std::function<std::unique_ptr<Node>(const std::string& name, const NodeOptions& options)>;

class NodeFactory {
 public:
  // Register a Node type for creation
  static void Register(const std::string& type_name, NodeCreator creator);

  // Create a Node instance by type name
  static std::unique_ptr<Node> Create(
      const std::string& type_name,
      const std::string& node_name,
      const NodeOptions& options);

  // Check if a type is registered
  static bool IsRegistered(const std::string& type_name);

  // List all registered type names
  static std::vector<std::string> RegisteredTypes();

 private:
  NodeFactory() = default;
  static std::map<std::string, NodeCreator>& Registry();
};

}  // namespace graph::runtime
```

**Semantics**:
- `Register()` stores a factory function for a given type name. Called during program initialization for built-in Node types.
- `Create()` looks up the registered factory and invokes it. Returns nullptr if type not found.
- Registry is global (static) — all registrations happen before graph construction begins.
- Type names correspond to the `"type"` field in the JSON graph config.
- Example registration: `NodeFactory::Register("StringProducer", [](auto& name, auto& opts) { return std::make_unique<StringProducerNode>(name, opts); });`
