# BUILD Conventions

## Visibility Strategy

The project enforces Bazel visibility to ensure external consumers only depend on the public API surface.

### Public API

Only `//src/public:runtime` is visible to external consumers. This is the sole entry point.

```python
# External project BUILD
deps = [
    "@graph_runtime//src/public:runtime",
]
```

### Internal Visibility

All internal packages use `//src:__subpackages__` visibility:

| Package | Visibility |
|---------|-----------|
| `src/log/` | `//src:__subpackages__` |
| `src/hook/` | `//src:__subpackages__` (+ `//src/tests:__subpackages__`) |
| `src/scheduler/` | `//src:__subpackages__` |
| `src/stream/` | `//src:__subpackages__` |
| `src/node/` | `//src:__subpackages__` |
| `src/config/` | `//src:__subpackages__` |
| `src/config/json/` | `//src:__subpackages__` |
| `src/public:runtime_internal` | `//src:__subpackages__` |
| `src/public:graph_builder` | `//src:__subpackages__` |

### Dep Prefix Convention

All internal deps MUST use the `@graph_runtime//` prefix:

```python
# Correct
deps = ["@graph_runtime//src/log:log_core"]

# Wrong
deps = ["//src/log:log_core"]
```

### Adding a New Module

1. Create `src/your_module/BUILD.bazel` with:
   - `package(default_visibility = ["//src:__subpackages__"])`
   - All deps using `@graph_runtime//` prefix
2. If the module should be public, add explicit `visibility = ["//visibility:public"]` to the specific target
3. Add the public target to the umbrella header in `src/public/include/graph_runtime/`

### Testing

Tests under `src/tests/` can access any internal target. Examples under `src/examples/` should only use `//src/public:runtime`.

### Checking Visibility

```bash
# Verify internal target is hidden from external
bazel query 'visible(//external:target, //src/scheduler:scheduler)'

# Verify public target is accessible
bazel query 'visible(//external:target, //src/public:runtime)'
```
