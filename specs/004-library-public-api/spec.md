# Feature Specification: Library Public API — Standard Consumable Bazel Library

**Feature Branch**: `004-library-public-api`

**Created**: 2026-07-23

**Status**: Draft

**Input**: "参考 /Users/moks/Develop/docker/ubuntu24/codes/atlas 项目作为依赖库，设计方案修正自省内部实现配置方案，以期作为一个标准的依赖库提供其他项目使用"

## User Scenarios & Testing

### User Story 1 — Consume as a Bazel Library (Priority: P1)

As a C++ developer building vision pipelines, I want to depend on Graph Runtime as a standard Bazel library so that I can build stream-based dataflow graphs from configuration without writing boilerplate scheduling code.

**Why this priority**: Core value proposition — without consumable library form, no external project can use Graph Runtime.

**Independent Test**: An external Bazel project can add `@graph_runtime//src/public:runtime` to its deps, include the umbrella header, and build successfully.

**Acceptance Scenarios**:

1. **Given** an external Bazel project with `http_archive` pulling Graph Runtime, **When** building with `deps = ["@graph_runtime//src/public:runtime"]`, **Then** compilation succeeds and the library is linkable.
2. **Given** a source file with `#include "graph_runtime/graph_runtime.h"`, **When** compiled against the public library, **Then** all public types (`GraphRuntime`, `Packet`, `Timestamp`) are accessible.
3. **Given** a non-Bazel build system (Makefile/CMake), **When** linking against the prebuilt `libruntime_shared.dylib`, **Then** the shared library exports all public symbols.

---

### User Story 2 — Clear Public API Boundary (Priority: P1)

As a framework developer integrating Graph Runtime, I want a clear separation between public API headers and internal implementation headers so that I know exactly which APIs are stable and which may change.

**Why this priority**: Without a clear boundary, every internal refactor risks breaking external consumers.

**Independent Test**: All public API symbols are decorated with `GRAPH_RUNTIME_API` and live under `src/public/include/graph_runtime/`. No internal header path leaks into the public include directory.

**Acceptance Scenarios**:

1. **Given** the public header directory `src/public/include/graph_runtime/`, **When** inspected, **Then** it contains only stable, documented public API headers.
2. **Given** the library built with `-fvisibility=hidden`, **When** the symbol table is inspected, **Then** only `GRAPH_RUNTIME_API`-decorated symbols are exported.
3. **Given** an internal header under `src/stream/` or `src/scheduler/`, **When** the public build target is compiled, **Then** those internal paths are not accessible to consumers.

---

### User Story 3 — External Dependency Setup (Priority: P2)

As a build engineer integrating Graph Runtime into a larger project, I want a single `graph_runtime_setup()` call in my WORKSPACE to fetch all transitive dependencies, so that I don't need to manually manage nested deps.

**Why this priority**: Reduces friction for adoption; important for developer experience but not blocking for MVP.

**Independent Test**: A fresh WORKSPACE calling `graph_runtime_setup()` resolves all external dependencies without errors.

**Acceptance Scenarios**:

1. **Given** a fresh Bazel workspace, **When** `graph_runtime_setup()` is called from WORKSPACE, **Then** all dependencies (nlohmann/json, abseil, googletest) are fetched successfully.
2. **Given** a project that already loads abseil, **When** `graph_runtime_setup()` is called, **Then** it does NOT redefine existing repositories (graceful `native.existing_rule()` guard).

---

### Edge Cases

- **Duplicate dependency definitions**: If a consumer already defines a dependency (e.g., `com_google_absl`), the setup macro must skip redefinition, not fail.
- **Non-Bazel consumption**: The shared library must be buildable standalone with `bazel build //src/public:runtime_shared` and produce a working `.dylib`/`.so`.
- **Windows cross-compilation**: The `GRAPH_RUNTIME_API` macro must produce correct `dllexport`/`dllimport` when `_WIN32` is defined, even if cross-compilation is not tested in Phase 1.
- **Missing init anchor**: Without `graph_runtime_init.cc`, static registrations (NodeFactory, ParserRegistry) may be stripped by the linker in `linkshared=True` builds.

## Requirements

### Functional Requirements

- **FR-001**: System MUST provide a public header directory at `src/public/include/graph_runtime/` with `strip_include_prefix = "include"`, enabling `#include "graph_runtime/some_header.h"` from consumer code.
- **FR-002**: System MUST provide `graph_runtime_export.h` defining `GRAPH_RUNTIME_API` macro matching the Atlas pattern — `__attribute__((visibility("default")))` on non-Windows, `__declspec` on Windows, gated by `GRAPH_RUNTIME_SHARED_LIBRARY`.
- **FR-003**: All public API symbols (classes, functions) MUST be decorated with `GRAPH_RUNTIME_API`.
- **FR-004**: All translation units MUST be compiled with `-fvisibility=hidden`; only `GRAPH_RUNTIME_API`-decorated symbols are exported.
- **FR-005**: System MUST provide a `cc_library(name = "runtime")` in `src/public/BUILD` aggregating all public headers with `alwayslink = 1`.
- **FR-006**: System MUST provide a `cc_binary(name = "runtime_shared", linkshared = True, linkstatic = True)` producing a self-contained shared library for non-Bazel consumers.
- **FR-007**: System MUST provide `graph_runtime_init.cc` as a linker anchor to prevent static registration stripping.
- **FR-008**: The umbrella header `graph_runtime.h` MUST include all public sub-headers via `#include "graph_runtime/..."` paths.
- **FR-009**: The `graph_runtime_deps.bzl` MUST provide a single `graph_runtime_setup()` entry point using `native.existing_rule()` guards to prevent redefinition.
- **FR-010**: Public API tests MUST depend only on `//src/public:runtime`, not on internal modules, to validate the public surface is self-contained.

### Key Entities

- **graph_runtime_export.h**: Export macro definition (`GRAPH_RUNTIME_API`), analogous to Atlas's `atlas_export.h`.
- **Umbrella header (graph_runtime.h)**: Top-level include file including all public sub-headers (`types.h`, `packet.h`, `timestamp.h`, `graph_runtime.h`, `graph_config.h`, `side_packet.h`).
- **graph_runtime_init.cc**: Linker anchor translation unit — references registration symbols to prevent linker from stripping them.
- **runtime_shared**: `cc_binary(linkshared=True, linkstatic=True)` producing `libruntime_shared.dylib`.
- **graph_runtime_deps.bzl**: Two-layer setup — `graph_runtime_setup()` → `_graph_runtime_deps()` with `native.existing_rule()` guards.

## Success Criteria

### Measurable Outcomes

- **SC-001**: `bazel build //src/public:runtime` succeeds, producing a static library that aggregates all internal modules.
- **SC-002**: `bazel build //src/public:runtime_shared` succeeds, producing a self-contained `.dylib`/`.so` with public symbols exported.
- **SC-003**: `bazel test //src/tests:public_api_test` passes, confirming all public types are accessible via the umbrella header without depending on internal modules.
- **SC-004**: A minimal external project can depend on `@graph_runtime//src/public:runtime` and build a trivial binary (`#include "graph_runtime/graph_runtime.h"`, `GraphRuntime rt;`).
- **SC-005**: `nm -gU bazel-bin/src/public/libruntime_shared.dylib` shows only `GRAPH_RUNTIME_API`-decorated symbols (no internal implementation details leaked).

## Assumptions

- The project source code structure (`src/stream/`, `src/node/`, `src/scheduler/`) remains unchanged — only the public API layer is refactored.
- Public headers are thin re-exports or relocations of existing internal headers, not rewrites.
- Atlas's `src/public/include/atlas/` + `strip_include_prefix` pattern is the canonical approach.
- Non-Bazel consumption is a Phase 2 concern; Phase 1 ensures `bazel build //src/public:runtime_shared` compiles.
- Windows support is compile-tested only via the macro path, not via CI.
