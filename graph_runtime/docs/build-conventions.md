# BUILD Conventions

## Visibility Strategy

The project enforces Bazel visibility to ensure external consumers only depend on the public API surface.

### Public API

Only `//src/framework/public:runtime` is visible to external consumers. This is the sole entry point.

```python
# External project BUILD
deps = [
    "@graph_runtime//src/framework/public:runtime",
]
```

### Internal Visibility

All internal packages under `src/framework/` use `//src/framework:__subpackages__` + `//src/tests:__subpackages__` visibility:

| Package | Visibility |
|---------|-----------|
| `src/framework/utils:hook` | `//src/framework:__subpackages__`, `//src/tests:__subpackages__` |
| `src/framework/utils:logger` | `//src/framework:__subpackages__`, `//src/tests:__subpackages__` |
| `src/framework/scheduler/` | `//src/framework:__subpackages__`, `//src/tests:__subpackages__` |
| `src/framework/stream/` | `//src/framework:__subpackages__`, `//src/tests:__subpackages__` |
| `src/framework/node/` | `//src/framework:__subpackages__`, `//src/tests:__subpackages__` |
| `src/framework/config/` | `//src/framework:__subpackages__`, `//src/tests:__subpackages__` |
| `src/framework/config/json/` | `//src/framework:__subpackages__`, `//src/tests:__subpackages__` |
| `src/framework/public:runtime_internal` | `//src/framework:__subpackages__`, `//src/tests:__subpackages__` |
| `src/framework/public:graph_builder` | `//src/framework:__subpackages__`, `//src/tests:__subpackages__` |

### Dep Prefix Convention

All internal deps MUST use the `@graph_runtime//` prefix:

```python
# Correct
deps = ["@graph_runtime//src/framework/utils:logger"]

# Wrong
deps = ["//src/framework/utils:logger"]
```

### Adding a New Framework Module

1. Create `src/framework/your_module/BUILD.bazel` with:
   - `package(default_visibility = ["//src/framework:__subpackages__", "//src/tests:__subpackages__"])`
   - All deps using `@graph_runtime//src/framework/...` prefix
2. If the module should be public, add explicit `visibility = ["//visibility:public"]` to the specific target
3. Add the public target to the umbrella header in `src/framework/public/include/graph_runtime/`

### Testing

Tests under `src/tests/` can access any internal framework target. Examples under `src/examples/` should only use `//src/framework/public:runtime`.

### Execution Modes

The runtime supports two execution paths (see `docs/project_bootstrap.md` §3.5.1):

| Mode | Method | Threading | External Input |
|------|--------|-----------|---------------|
| **Sync** | `Schedule()` | Caller's thread | ❌ |
| **Async** | `Start() + WaitUntilDone()` | ThreadPoolExecutor | ✅ |

### Checking Visibility

```bash
# Verify internal target is hidden from external
bazel query 'visible(//external:target, //src/framework/scheduler:scheduler)'

# Verify public target is accessible
bazel query 'visible(//external:target, //src/framework/public:runtime)'
```
