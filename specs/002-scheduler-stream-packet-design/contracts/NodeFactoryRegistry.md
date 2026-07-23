# Contract: NodeFactoryRegistry

**File**: `graph_runtime/src/node/node_registry.h`

```cpp
namespace graph::runtime {

// --- Registration macro (prefix with GRAPH_RUNTIME_ to avoid symbol conflicts) ---

#define GRAPH_RUNTIME_REGISTER_NODE(type_name, node_class)              \
  static ::graph::runtime::internal::NodeRegistrationToken               \
      GRAPH_RUNTIME_NODE_REGISTRATION_##node_class##__ =                 \
          ::graph::runtime::internal::NodeRegistrationToken(              \
              ::graph::runtime::NodeFactoryRegistry::Register(            \
                  type_name,                                              \
                  std::make_unique<                                        \
                      ::graph::runtime::internal::NodeFactoryFor<          \
                          node_class>>()))

// --- Registry ---

namespace internal {

// RAII token: unregisters the factory on destruction (used for testing).
class NodeRegistrationToken {
 public:
  NodeRegistrationToken(int registration_id);
  ~NodeRegistrationToken();

  NodeRegistrationToken(const NodeRegistrationToken&) = delete;
  NodeRegistrationToken& operator=(const NodeRegistrationToken&) = delete;

 private:
  int registration_id_;
};

}  // namespace internal

// Global factory registry for Node types.
// Maps type-name strings to unique_ptr<NodeFactory> instances.
class NodeFactoryRegistry {
 public:
  // Register a factory. Returns a registration ID (for token-based cleanup).
  static int Register(const std::string& type_name,
                      std::unique_ptr<NodeFactory> factory);

  // Unregister by ID (called by ~NodeRegistrationToken).
  static bool Unregister(int registration_id);

  // Look up and invoke factory by name.
  static std::unique_ptr<Node> CreateByName(const std::string& type_name,
                                             const std::string& node_name,
                                             const NodeOptions& options);

  // Look up and invoke factory by namespace + name.
  static std::unique_ptr<Node> CreateByNameInNamespace(
      const std::string& ns, const std::string& type_name,
      const std::string& node_name, const NodeOptions& options);

  // Look up factory without creating.
  static NodeFactory* GetFactory(const std::string& type_name);

  // Type introspection.
  static bool IsRegistered(const std::string& type_name);
  static std::vector<std::string> RegisteredTypes();

 private:
  NodeFactoryRegistry() = default;
  // Singleton internal registry
};

}  // namespace graph::runtime
```

**Semantics**:
- `GRAPH_RUNTIME_REGISTER_NODE(type_name, node_class)` creates a file-scope static `NodeRegistrationToken` that registers a `NodeFactoryFor<T>` into the global registry at program initialization (before `main()`). This mirrors MediaPipe's `REGISTER_CALCULATOR` macro.
- The `GRAPH_RUNTIME_` prefix ensures the macro does not conflict with other libraries' `REGISTER_NODE` or similar macros.
- `Register()` inserts a `unique_ptr<NodeFactory>` keyed by `type_name`. Returns an integer `registration_id` for unregistration.
- `Unregister(id)` removes the factory. Called from `~NodeRegistrationToken()`. Enables clean teardown in tests without process restart.
- `CreateByName(name, ...)` looks up the factory by flat name, calls `factory->CreateNode(node_name, options)`, and returns the new `Node` instance.
- `CreateByNameInNamespace(ns, name, ...)` prepends the namespace for lookup. Used when multiple packages independently define node types with the same short name.
- `GetFactory(name)` returns the factory without creating — used during graph validation to call `factory->GetContract(&contract)`.
- `IsRegistered(name)` / `RegisteredTypes()` provide introspection for tooling and debugging.
