# Implementation Plan: Bazel Visibility Control

**Branch**: `006-bazel-visibility-control` | **Date**: 2026-07-24 | **Spec**: [spec.md](./spec.md)

## Summary

Restrict Bazel target visibility so that external consumers can only depend on `//src/public:runtime`. All internal targets (`//src/scheduler/...`, `//src/stream/...`, `//src/node/...`, `//src/config/...`, and auxiliary targets under `//src/public/`) get explicit visibility lists limited to `//src/...`. Examples must only use public API. Document the strategy in `docs/build-conventions.md`.

## Technical Context

**Language/Version**: N/A (Bazel BUILD configuration only)
**Primary Dependencies**: None
**Testing**: `bazel build //...` + `bazel test //...`
**Target Platform**: All (Bazel BUILD semantics)
**Project Type**: Bazel visibility configuration

## Constitution Check

| Principle | Status | Notes |
|-----------|--------|-------|
| V. Build System Integrity | ✅ PASS | This feature directly enforces V's mandate that `//src/public:runtime` is the sole external entry point |

## Project Structure

### Affected BUILD files

```text
src/
├── public/BUILD.bazel       # Add visibility to hook_table, log_interface, runtime_internal, graph_builder, types, side_packet
├── scheduler/BUILD.bazel    # Add visibility = ["//src:__subpackages__"]
├── stream/BUILD.bazel       # Same
├── node/BUILD.bazel         # Same
├── config/BUILD.bazel       # Same
├── config/json/BUILD.bazel  # Same
├── examples/BUILD.bazel     # Verify all deps are //src/public:runtime only
└── tests/BUILD.bazel        # Already depends on internal targets (acceptable)
docs/
└── build-conventions.md     # NEW — document visibility strategy
```

## Tasks

### Phase 1: Audit & Analysis

- List all `cc_library` targets under `//src/` and classify as Public / Internal / Test
- Record current `default_visibility` settings

### Phase 2: Visibility Changes

- Change each internal BUILD's `package(default_visibility = ...)` to `["//src:__subpackages__"]`
- For `//src/public`: add explicit visibility per target
- Add `//src/public:runtime` as `//visibility:public`

### Phase 3: Example Cleanup

- Audit example deps; replace internal deps with `//src/public:runtime` where needed
- Move any required internal symbols into the public API if examples legitimately need them

### Phase 4: Documentation

- Write `docs/build-conventions.md`
- Update `AGENTS.md` to document visibility conventions

### Phase 5: Validation

- `bazel build //...` passes
- `bazel test //...` passes
- Manual test: external-style build with invalid dep fails
