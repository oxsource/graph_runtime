# Feature Specification: Bazel Visibility Control

**Created**: 2026-07-24
**Status**: Draft

**Input**: 新建提案控制工程可见性 — 限制 Bazel target 可见性，防止外部消费者引用内部模块

## User Scenarios & Testing

### User Story 1 - External Consumer Uses Only Public API (Priority: P1)

As an external project integrating graph_runtime, I want Bazel to prevent me from depending on internal targets (`//src/scheduler`, `//src/public:log_interface`, etc.), so that I only use the documented public API surface (`//src/public:runtime`).

**Why this priority**: Constitutional principle V mandates that the sole external entry point is `//src/public:runtime`. Current flat visibility makes violations undetectable at build time.

**Independent Test**: An external project that adds `deps = ["@graph_runtime//src/scheduler:scheduler"]` must fail to build with a visibility error.

**Acceptance Scenarios**:
1. **Given** an external Bazel project depending on `@graph_runtime//src/public:runtime`, **When** building, **Then** it succeeds
2. **Given** an external Bazel project depending on `@graph_runtime//src/scheduler:scheduler`, **When** building, **Then** Bazel rejects with a visibility violation
3. **Given** an external Bazel project depending on `@graph_runtime//src/public:log_interface`, **When** building, **Then** Bazel rejects with a visibility violation

---

### User Story 2 - Internal Modules Can Still Cross-Reference (Priority: P1)

As an internal module developer (e.g., scheduler), I want to depend on other internal modules (e.g., `//src/public:log_interface`) without visibility restrictions, so that development velocity is not impacted.

**Why this priority**: Internal cross-references must continue to work for the library to function.

**Independent Test**: `bazel build //src/scheduler:scheduler` must succeed, pulling in logger via internal deps.

**Acceptance Scenarios**:
1. **Given** the scheduler target depends on `//src/public:log_interface`, **When** building `//src/scheduler:scheduler`, **Then** it succeeds
2. **Given** the runtime target depends on `//src/scheduler:scheduler`, **When** building `//src/public:runtime`, **Then** it succeeds

---

### User Story 3 - Examples Build Against Public API Only (Priority: P2)

As a CI system, I want all example binaries to compile using only `//src/public:runtime` (not internal targets), so that the examples serve as valid consumer references.

**Why this priority**: Examples currently may pull internal targets, setting a bad pattern for external consumers.

**Independent Test**: `bazel build //src/examples/...` must succeed with each example using only public API deps.

**Acceptance Scenarios**:
1. **Given** an example BUILD file with `deps = ["@graph_runtime//src/scheduler:scheduler"]`, **When** building, **Then** it fails
2. **Given** an example BUILD file with `deps = ["@graph_runtime//src/public:runtime"]`, **When** building, **Then** it succeeds

---

### Edge Cases

- What about third_party packages that need internal access? They should go through the same public API.
- How do unit tests access internal headers? Tests under `//src/tests/` are considered internal — they already depend on internal targets and that's acceptable.
- What happens when a new internal module is added but its targets default to `//visibility:public`? The default must be changed project-wide.
- How does `bazel query` visibility analysis work? A `bazel query 'visible(//external:target, //src/public:runtime)'` style check should be documented.

## Requirements

### Functional Requirements

- **FR-001**: All targets under `//src/` MUST use explicit visibility lists instead of `package(default_visibility = ["//visibility:public"])`
- **FR-002**: The target `//src/public:runtime` MUST remain visible to external consumers (`//visibility:public`)
- **FR-003**: Internal targets (`//src/scheduler/...`, `//src/stream/...`, `//src/node/...`, `//src/config/...`, `//src/public:log_interface`, `//src/public:hook_table`, `//src/public:runtime_internal`) MUST be visible only to `//src/...` (internal package group)
- **FR-004**: Example targets under `//src/examples/...` MUST depend only on `//src/public:runtime`. If an example needs internal symbols, those symbols MUST be exposed through the public API first.
- **FR-005**: The external consumer demo (`//examples/consumer_demo/...`) MUST continue to compile with only `@graph_runtime//src/public:runtime` deps
- **FR-006**: Tests under `//src/tests/...` MUST have visibility to all internal targets for testing purposes
- **FR-007**: A BUILD convention document MUST be added to `docs/` explaining the visibility strategy and how to add new modules
- **FR-008**: `bazel build //...` and `bazel test //...` MUST continue to pass after visibility changes

### Key Entities

- **Visibility Group** (`//visibility:public`, `//visibility:private`, package list): Bazel's access control mechanism for cc_library targets
- **Internal Package Group** (`//src/...`): The set of packages that constitute the internal implementation. Internal targets are visible to each other but not to external consumers.
- **Public API Surface** (`//src/public:runtime`): The sole target that external consumers may depend on. All public symbols are exported here.

## Success Criteria

### Measurable Outcomes

- **SC-001**: An external project with `deps = ["@graph_runtime//src/scheduler:scheduler"]` fails to build with a clear visibility error message
- **SC-002**: `bazel build //src/... --//:enforce_visibility` (or equivalent check) passes with zero errors
- **SC-003**: All example binaries compile successfully after migration to `//src/public:runtime`-only deps
- **SC-004**: `bazel test //...` passes with zero regressions after visibility changes
- **SC-005**: The visibility strategy is documented in `docs/build-conventions.md` with examples for adding new modules

## Assumptions

- Bazel's `default_visibility` on `package()` is the primary mechanism; individual target override only where necessary
- Internal modules that need to be shared across internal packages use `visibility = ["//src:__subpackages__"]` pattern
- External consumers always depend on the workspace-level `@graph_runtime//` alias
- Tests are considered internal — they exist within the same repo and can access internal targets
- Examples are considered semi-external — they should mirror external consumer behavior
