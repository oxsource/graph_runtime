# Contract: NodeOptions

**File**: `graph_runtime/src/node/node_options.h`

```cpp
namespace graph::runtime {

// Simple key-value options container for node configuration.
// Phase 1: flat map of string → Packet (serialized options).
// OptionsRegistry provides typed deserialization for compile-time field access.
class NodeOptions {
 public:
  template <typename T>
  void Set(const std::string& key, const T& value);

  template <typename T>
  const T* Get(const std::string& key) const;

  bool Has(const std::string& key) const;
  std::vector<std::string> Keys() const;

  // Typed deserialization via OptionsRegistry.
  // Returns a reference to a cached instance keyed by {type_name, this}.
  // Example:
  //   const auto& opts = node_options.Get<MyNodeOptions>();
  //   int threshold = opts.threshold;  // compile-time field access
  template <typename T>
  const T& Deserialize() const;

 private:
  std::map<std::string, std::any> options_;
};

}  // namespace graph::runtime
```

**Semantics**:
- `Set<T>(key, value)` stores a value. `Get<T>(key)` retrieves it — returns `nullptr` if key missing or type mismatch.
- `Deserialize<T>()` uses `OptionsRegistry::Deserialize<T>(*this)` to convert key-value pairs into a typed struct. The result is cached per-options-instance. Throws if the type is not registered.
- Node options are defined in `GraphConfig::NodeDef::options` and passed to `NodeFactory::CreateNode(name, options)`. The Node accesses them via `GraphContext::Options<T>()` during Open/Process/Close.
- Registration example:
  ```cpp
  // MyNode.cc
  struct MyNodeOptions { int threshold; std::string prefix; };
  GRAPH_RUNTIME_REGISTER_OPTIONS("MyNode", MyNodeOptions)
      .Field("threshold", &MyNodeOptions::threshold)
      .Field("prefix", &MyNodeOptions::prefix);
  ```
