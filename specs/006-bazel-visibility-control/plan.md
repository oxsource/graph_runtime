# Implementation Plan: Bazel Visibility Control

**Date**: 2026-07-24 | **Spec**: [spec.md](./spec.md)

## Summary

Restrict Bazel target visibility so external consumers can only depend on `//src/public:runtime`. Internal packages get `//src:__subpackages__` visibility. Fix 4 dep prefix violations. Rewrite `string_pipeline_json` to use only public API. Re-export `timestamp`/`packet` through public headers. Document in `docs/build-conventions.md`.

## Constitution Check

| Principle | Status |
|-----------|--------|
| V. Build System Integrity | ✅ PASS — directly enforces sole external entry point mandate |

## Affected BUILD Files

### Phase 1 — Dep Prefix Fixes (4 files)

| File | Line | Change |
|------|------|--------|
| `src/framework/public/BUILD.bazel` | `runtime` deps | `"//src/framework/utils:logger"` → `"@graph_runtime//src/framework/utils:logger"` |
| `src/framework/utils/BUILD.bazel` | `logger` deps | `"//src/framework/utils:hook"` → `"@graph_runtime//src/framework/utils:hook"` |
| `src/framework/scheduler/BUILD.bazel` | `scheduler` deps | `"//src/framework/utils:logger"` → `"@graph_runtime//src/framework/utils:logger"` |
| `src/hook/BUILD.bazel` | `hook` deps | `"//src/public:hook_header"` → `"@graph_runtime//src/public:hook_header"` |

### Phase 2 — Visibility Changes (7 files)

| File | Current | New |
|------|---------|-----|
| `src/framework/utils/BUILD.bazel` | `//visibility:public` | `["//src:__subpackages__"]` |
| `src/framework/scheduler/BUILD.bazel` | `//visibility:public` | `["//src:__subpackages__"]` |
| `src/stream/BUILD.bazel` | `//visibility:public` | `["//src:__subpackages__"]` |
| `src/node/BUILD.bazel` | `//visibility:public` | `["//src:__subpackages__"]` |
| `src/config/BUILD.bazel` | `//visibility:public` | `["//src:__subpackages__"]` |
| `src/config/json/BUILD.bazel` | `//visibility:public` | `["//src:__subpackages__"]` |
| `src/framework/public/BUILD.bazel` | per-target: add explicit visibility for `runtime_internal` and `graph_builder` as `["//src:__subpackages__"]` |

### Phase 3 — Example Rewrite

- Rewrite `string_pipeline_json.cc` to not inline Node subclasses; instead use only `//src/public:runtime` API
- Verify `custom_parser.cc` uses only `//src/public:runtime` and `//src/config:config` via re-export

### Phase 4 — Re-export Headers (if needed)

- Verify `include/graph_runtime/timestamp.h` and `include/graph_runtime/packet.h` exist and include from `src/stream/`
- If missing, create them

### Phase 5 — Documentation

- Write `docs/build-conventions.md`
- Include: visibility strategy, how to add new modules, how to expose new public API

### Phase 6 — Visibility Test

- Write `src/tests/visibility_test.sh` — shell script using `bazel query`:
  ```bash
  # Internal targets should NOT be visible to external
  if bazel query 'visible(//external:target, //src/scheduler:scheduler)' 2>/dev/null \
     | grep -q .; then echo "FAIL: scheduler visible externally"; exit 1; fi
  # Public target SHOULD be visible to external
  if ! bazel query 'visible(//external:target, //src/public:runtime)' 2>/dev/null \
     | grep -q "//src/public:runtime"; then echo "FAIL: runtime not visible"; exit 1; fi
  ```
- Add `sh_test` target to `src/tests/BUILD.bazel`

### Phase 7 — Validation

- `bazel build //src/...` passes
- `bazel test //src/tests/...` passes (13 old + 1 new visibility test)
- `grep '//src/' src/**/BUILD.bazel` — 0 prefix violations
- All examples build successfully with public API only

## Pre-checks Before Code Changes

All design decisions confirmed with user:
- [x] string_pipeline_json → only src/public:runtime
- [x] timestamp/packet → re-exported through public headers
- [x] consumer_demo → keep FR but not a gate
- [x] 4 dep prefix violations must be fixed
- [x] graph_builder → internal visibility
