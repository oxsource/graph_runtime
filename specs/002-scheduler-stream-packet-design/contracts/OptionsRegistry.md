# Contract: OptionsRegistry

**File**: `graph_runtime/src/node/options_registry.h`

```cpp
namespace graph::runtime {

// Descriptor for a single field in a typed options struct.
struct FieldDescriptor {
  std::string name;
  std::function<void(void* obj, const std::any& value)> setter;
};

// Registration helper for typed Node options.
// T is the options struct type.
template <typename T>
class OptionsRegistrar {
 public:
  OptionsRegistrar(const std::string& type_name);

  // Register a field: field_name → member pointer.
  template <typename FieldType>
  OptionsRegistrar& Field(const std::string& field_name,
                          FieldType T::*member);
};

// Global registry: type_name → vector<FieldDescriptor>.
// Used by NodeOptions::Get<T>() to deserialize key-value pairs into struct T.
class OptionsRegistry {
 public:
  template <typename T>
  static const OptionsRegistrar<T>& Register(const std::string& type_name);

  template <typename T>
  static T Deserialize(const NodeOptions& options);

  static bool IsRegistered(const std::string& type_name);
};

// Registration macro — expands to a file-scope static registration.
// Usage in MyNode.cc:
//   GRAPH_RUNTIME_REGISTER_OPTIONS("MyNode", MyNodeOptions)
//       .Field("threshold", &MyNodeOptions::threshold)
//       .Field("prefix", &MyNodeOptions::prefix);
#define GRAPH_RUNTIME_REGISTER_OPTIONS(type_name, options_class) \
  static const auto& _register_##options_class =                  \
      ::graph::runtime::OptionsRegistry::Register<options_class>(type_name)

}  // namespace graph::runtime
```

**Semantics**:
- Provides compile-time typed access to Node options without protobuf. Node developers define a plain struct with fields, register each field with its name, and the framework deserializes `GraphConfig::NodeDef::options` (a string→any map) into the struct at runtime.
- `Register<T>(type_name)` creates an `OptionsRegistrar<T>` that accumulates field descriptors. The registrar's `Field()` method stores a lambda `setter` that casts `std::any` to `FieldType` and assigns to the member pointer.
- `Deserialize<T>(options)` iterates the registered field descriptors for the type and calls each setter with the value from the options map.
- `GRAPH_RUNTIME_REGISTER_OPTIONS` is a static variable that runs at program initialization, adding the field descriptors to the global registry.
