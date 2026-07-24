# Feature Specification: Bazel Visibility Control

**Created**: 2026-07-24
**Status**: Draft (refined)

**Input**: 新建提案控制工程可见性 — 限制 Bazel target 可见性，防止外部消费者引用内部模块

## Clarifications

- `string_pipeline_json` 示例改为只依赖 `//src/public:runtime`，不直接引用内部模块
- `src/stream:timestamp` / `src/stream:packet` 通过 `include/graph_runtime/` 重新导出
- FR-005 (consumer_demo) 保留但不作为 gate — 存在预存 workspace 问题
- 4 处 dep 前缀 `//src/` 须改为 `@graph_runtime//src/`

## User Scenarios & Testing

### User Story 1 - External Consumer Uses Only Public API (Priority: P1)

As an external project integrating graph_runtime, I want Bazel to prevent me from depending on internal targets, so that I only use the documented public API surface (`//src/public:runtime`).

**Why this priority**: Constitutional principle V mandates that the sole external entry point is `//src/public:runtime`.

**Independent Test**: An external project adding `deps = ["@graph_runtime//src/scheduler:scheduler"]` must fail with a visibility error.

**Acceptance Scenarios**:
1. **Given** an external project depending on `@graph_runtime//src/public:runtime`, **When** building, **Then** it succeeds
2. **Given** an external project depending on `@graph_runtime//src/scheduler:scheduler`, **When** building, **Then** Bazel rejects with a visibility violation
3. **Given** an external project depending on `@graph_runtime//src/log:log_core`, **When** building, **Then** Bazel rejects

### User Story 2 - Internal Modules Can Still Cross-Reference (Priority: P1)

As an internal developer, I want to depend on other internal modules without restriction.

**Independent Test**: `bazel build //src/scheduler:scheduler` succeeds via internal deps.

**Acceptance Scenarios**:
1. **Given** scheduler depends on `//src/log:log_core`, **When** building `//src/scheduler:scheduler`, **Then** it succeeds
2. **Given** runtime depends on scheduler, **When** building `//src/public:runtime`, **Then** it succeeds

### User Story 3 - Examples Use Only Public API (Priority: P2)

As a CI system, I want example binaries to compile using only `//src/public:runtime`.

**Independent Test**: `bazel build //src/examples/...` succeeds with each example using only public API deps.

**Acceptance Scenarios**:
1. **Given** an example BUILD with deps to internal targets, **When** building, **Then** it fails
2. **Given** an example BUILD with `deps = ["@graph_runtime//src/public:runtime"]`, **When** building, **Then** it succeeds

### Edge Cases

- `consumer_demo` (external workspace) has pre-existing Bazel issue — not a gate for this proposal
- `src/tests/` has visibility to all internal targets for testing
- New modules added in the future must follow the same visibility convention
- `bazel query 'visible(//external:target, //src/public:runtime)'` should be in CI

### Visibility Testing

A dedicated `sh_test` target verifies visibility constraints via `bazel query`:

```python
sh_test(
    name = "visibility_test",
    srcs = ["visibility_test.sh"],
    data = ["//src/public:runtime", "//src/scheduler:scheduler"],
)
```

The script queries visibility relationships:
- `bazel query 'visible(//external:target, //src/scheduler:scheduler)'` → empty (not visible)
- `bazel query 'visible(//external:target, //src/public:runtime)'` → returns runtime (visible)

## Requirements

### Functional Requirements

- **FR-001**: All internal packages (`src/log/`, `src/hook/`, `src/scheduler/`, `src/stream/`, `src/node/`, `src/config/`, `src/config/json/`) MUST set `package(default_visibility = ["//src:__subpackages__"])`
- **FR-002**: `src/public/BUILD.bazel` MUST have per-target visibility: `runtime` as `//visibility:public`, `runtime_internal`/`graph_builder` as `["//src:__subpackages__"]`
- **FR-003**: `src/hook/BUILD.bazel` visibility is already correct — keep `["//src:__subpackages__", "//src/tests:__subpackages__"]`
- **FR-004**: `src/stream:timestamp` and `src/stream:packet` MUST be re-exported via `include/graph_runtime/timestamp.h` and `include/graph_runtime/packet.h` (already exist)
- **FR-005**: All 4 dep prefix violations (`//src/` → `@graph_runtime//src/`) MUST be fixed:
  - `src/public/BUILD.bazel` → `//src/log:log_core`
  - `src/log/BUILD.bazel` → `//src/hook:hook`
  - `src/scheduler/BUILD.bazel` → `//src/log:log_core`
  - `src/hook/BUILD.bazel` → `//src/public:hook_header`
- **FR-006**: Example `string_pipeline_json` MUST be rewritten to use only `//src/public:runtime`
- **FR-007**: `consumer_demo` FR kept but not a gate — pre-existing workspace issue documented
- **FR-008**: A BUILD convention document MUST be added to `docs/build-conventions.md`
- **FR-009**: A `visibility_test.sh` + `sh_test` target MUST be added to `src/tests/` that verifies:
  - Internal targets (`//src/scheduler:scheduler`) are NOT visible to `//external:target`
  - Public target (`//src/public:runtime`) IS visible to `//external:target`
  - Runs as part of `bazel test //src/tests/...`
- **FR-010**: `bazel build //...` and `bazel test //...` MUST pass after visibility changes

### Key Entities

- **Public API Surface** (`//src/public:runtime`): Sole external entry point; `//visibility:public`
- **Internal Package Group** (`//src:__subpackages__`): Internal targets visible to each other
- **Re-exported Headers** (`include/graph_runtime/timestamp.h`, `packet.h`): Public wrappers for internal types

## Success Criteria

- **SC-001**: External project with internal dep fails with clear visibility error
- **SC-002**: `bazel build //src/... && bazel test //src/...` both pass
- **SC-003**: All examples build after migration to `//src/public:runtime`-only deps
- **SC-004**: Dep prefix violations resolved (grep count goes from 4 to 0)
- **SC-005**: Visibility strategy documented in `docs/build-conventions.md`
- **SC-006**: `src/tests:visibility_test` passes in CI, confirming internal targets are hidden

## Assumptions

- `default_visibility` on `package()` is the primary mechanism
- `//src:__subpackages__` covers all internal cross-references
- Tests under `//src/tests/` are internal — access to internal targets is allowed
- Examples under `//src/examples/` are semi-external — should only use public API
