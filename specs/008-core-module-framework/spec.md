# Feature Specification: Module Directory Restructure

**Feature Branch**: `008-module-directory-restructure`

**Created**: 2026-07-24
**Status**: Draft

**Input**: 新建提案，将src目录下除examples及tests目录下的其他模块都迁移到src/framework目录，避免混杂，通知调整相关配置，完整编译测试验证

## User Scenarios & Testing

### User Story 1 - Module Reorganization (Priority: P1)

As a developer working on the graph_runtime library, I want all core implementation modules to live under `src/framework/` rather than directly under `src/`, so that the source tree clearly separates framework code from examples and tests.

**Why this priority**: Currently `src/` has 9 directories mixed together — 7 internal modules + examples + tests. This creates ambiguity about which directories constitute the core library.

**Independent Test**: After migration, `src/` contains exactly 3 top-level directories: `framework/`, `examples/`, `tests/`. All internal modules previously under `src/` are now under `src/framework/`.

**Acceptance Scenarios**:
1. **Given** the project source tree, **When** listing `src/`, **Then** only `framework/`, `examples/`, `tests/` exist
2. **Given** the `src/framework/` directory, **When** listing its contents, **Then** all 7 internal modules are present: `config/`, `hook/`, `log/`, `node/`, `public/`, `scheduler/`, `stream/`
3. **Given** a source file in any internal module, **When** compiled, **Then** its `#include` paths use `src/framework/` prefix instead of `src/`

---

### User Story 2 - Build Configuration Migration (Priority: P1)

As a build engineer, I want all Bazel BUILD files to reference the new directory paths, so that the project compiles correctly after the move.

**Why this priority**: Every BUILD.bazel file under `src/` and across the workspace references `//src/...` paths for internal deps. These must all be updated to `//src/framework/...`.

**Independent Test**: `bazel build //... && bazel test //...` passes after all path updates.

**Acceptance Scenarios**:
1. **Given** all source files moved to `src/framework/`, **When** `bazel build //...` is run, **Then** it succeeds
2. **Given** all tests moved, **When** `bazel test //...` is run, **Then** all 14+ tests pass
3. **Given** the shared library target, **When** `bazel build //src/framework/public:runtime_shared` is run, **Then** it succeeds

---

### User Story 3 - External Consumer Compatibility (Priority: P2)

As an external consumer of the graph_runtime library, I want the public API headers to remain accessible via the same include paths, so that my code does not break.

**Why this priority**: The public headers under `include/graph_runtime/` are consumed by external projects. Their `strip_include_prefix` paths may change.

**Independent Test**: External consumer demo (examples/consumer_demo/) builds and runs successfully after migration.

**Acceptance Scenarios**:
1. **Given** the consumer demo external project, **When** built, **Then** it succeeds with no include path changes in user code
2. **Given** the public umbrella header, **When** consumed, **Then** `#include "graph_runtime/graph_runtime.h"` resolves correctly

---

### Edge Cases

- What about the `docs/build-conventions.md` that references `src/public/` paths? Must be updated to `src/framework/public/`.
- What about `AGENTS.md` that references source paths? Must be updated to the new layout.
- What about `.specify/extensions/` scripts that reference source paths? Must be checked and updated.
- Consumer demo's `local_repository` path may need adjustment.
- Git history: `git mv` preserves file history.

## Requirements

### Functional Requirements

- **FR-001**: Move `src/config/`, `src/hook/`, `src/log/`, `src/node/`, `src/public/`, `src/scheduler/`, `src/stream/` to `src/framework/{config,hook,log,node,public,scheduler,stream}/`
- **FR-002**: Update all `#include "src/..."` paths in moved files to `#include "src/framework/..."`
- **FR-003**: Update all `//src/...` Bazel dep references to `//src/framework/...` across ALL BUILD.bazel files
- **FR-004**: Update all `@graph_runtime//src/...` dep references to `@graph_runtime//src/framework/...`
- **FR-005**: Update visibility rules — `//src:__subpackages__` → `//src/framework:__subpackages__` where applicable
- **FR-006**: Update `docs/build-conventions.md` and `AGENTS.md` with new paths
- **FR-007**: `bazel build //...` MUST pass after migration
- **FR-008**: `bazel test //...` MUST pass after migration (14+ tests)
- **FR-009**: Consumer demo (`examples/consumer_demo/`) MUST continue to build
- **FR-010**: Git history MUST be preserved via `git mv` (not copy+delete)

### Key Entities

- **Source tree layout**: The directory structure under `graph_runtime/graph_runtime/src/`
- **Bazel BUILD paths**: Workspace-relative labels used in `deps` attributes (e.g., `//src/framework/log:log_core`)
- **Include paths**: C++ preprocessor include directives in source files (e.g., `#include "src/framework/logger.h"`)
- **Workspace root**: `graph_runtime/graph_runtime/` containing the WORKSPACE file

## Success Criteria

- **SC-001**: `ls src/` shows exactly `framework/`, `examples/`, `tests/`
- **SC-002**: `bazel build //...` completes with zero errors
- **SC-003**: `bazel test //...` completes with 14/14 passing
- **SC-004**: `bazel build //src/framework/public:runtime_shared` succeeds
- **SC-005**: No stale `#include "src/..."` or `//src/...` references remain (verified by grep count)

## Assumptions

- `git mv` is used for all file moves to preserve history
- Bazel's `strip_include_prefix` settings in `src/public/` need adjustment only if the relative path from the BUILD file changes
- The AGENTS.md SPECKIT section must be updated with correct plan reference
- No source file content changes beyond include path updates — logic unchanged
- The migration can be staged as one atomic commit for clarity
