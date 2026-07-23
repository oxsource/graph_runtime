---

description: "Task list for Library Public API implementation"

---

# Tasks: Library Public API — Standard Consumable Bazel Library

**Input**: Design documents from `/specs/004-library-public-api/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/

**Tests**: Test tasks are included per the implementation plan (GoogleTest cc_test).

**Organization**: Tasks grouped by module dependency order, mapped to user stories.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to
- Include exact file paths in descriptions

## Path Conventions

- **Workspace root**: `graph_runtime/`
- **Public headers**: `graph_runtime/src/public/include/graph_runtime/`
- **Dep prefix**: All BUILD.bazel `deps` use `@graph_runtime//` instead of `//`
- **External consumer demo**: `graph_runtime/examples/consumer_demo/`

---

## Module Dependency Graph

```
Phase 1: Setup
├── graph_runtime_export.h (export macro)
├── public header directory structure

Phase 2: Foundational
├── Update ALL BUILD.bazel deps: // → @graph_runtime//
├── Update src/public/BUILD.bazel (strip_include_prefix)

Phase 3: US1 — Consumable Bazel Library (P1)
├── Create public headers (re-export wrappers)
├── Create umbrella header
├── Create graph_runtime_init.cc
├── public_api_test

Phase 4: US2 — Clear API Boundary (P1)
├── Verify internal headers NOT exposed
├── nm -gU symbol verification
├── runtime_shared shared library target

Phase 5: US3 — External Dep Setup (P2)
├── graph_runtime_deps.bzl refinement
├── External consumer demo (examples/consumer_demo/)
├── consumer_demo_test

Phase 6: Polish
├── Final build + test pass
├── Documentation update
```

---

## Phase 1: Setup — Public Header Infrastructure

**Purpose**: Create the export macro and public header directory structure.

- [ ] T001 Create `GRAPH_RUNTIME_API` export macro (`graph_runtime/src/public/include/graph_runtime/graph_runtime_export.h`): Atlas-style dllexport/dllimport/visibility pattern, gated by `GRAPH_RUNTIME_SHARED_LIBRARY`
- [ ] T002 Create public header directory structure (`graph_runtime/src/public/include/graph_runtime/`)
- [ ] T003 Create `types.h` public header (`graph_runtime/src/public/include/graph_runtime/types.h`): wraps `src/public/types.h` with `GRAPH_RUNTIME_API` include

---

## Phase 2: Foundational — Dep Prefix Migration & BUILD Refactor

**Purpose**: Migrate all BUILD.bazel deps to `@graph_runtime//` prefix and refactor `src/public/BUILD.bazel` with `strip_include_prefix`.

- [ ] T004 Update `graph_runtime/src/stream/BUILD.bazel` — change all `//src/` deps to `@graph_runtime//src/`
- [ ] T005 [P] Update `graph_runtime/src/node/BUILD.bazel` — change all `//src/` deps to `@graph_runtime//src/`
- [ ] T006 [P] Update `graph_runtime/src/scheduler/BUILD.bazel` — change all `//src/` deps to `@graph_runtime//src/`
- [ ] T007 [P] Update `graph_runtime/src/public/BUILD.bazel` — change all `//src/` deps to `@graph_runtime//src/` + add `strip_include_prefix = "include"` + add `copts = ["-fvisibility=hidden", "-fvisibility-inlines-hidden"]` + add `alwayslink = 1`
- [ ] T008 [P] Update `graph_runtime/src/config/BUILD.bazel` — change all `//src/` deps to `@graph_runtime//src/`
- [ ] T009 [P] Update `graph_runtime/src/tests/BUILD.bazel` — change all `//src/` deps to `@graph_runtime//src/`
- [ ] T010 [P] Update `graph_runtime/src/examples/BUILD.bazel` — change all `//src/` deps to `@graph_runtime//src/`

**Checkpoint**: `bazel build //src/...` succeeds after all dep prefix changes.

---

## Phase 3: User Story 1 — Consumable Bazel Library (Priority: P1)

**Goal**: External Bazel projects can depend on `@graph_runtime//src/public:runtime` with a clean public header layout.

**Independent Test**: A source file with `#include "graph_runtime/graph_runtime.h"` compiles against `//src/public:runtime` without referencing any internal headers.

- [ ] T011 [US1] Create `timestamp.h` re-export (`graph_runtime/src/public/include/graph_runtime/timestamp.h`): `#include "src/stream/timestamp.h"` wrapped with `graph_runtime_export.h` include guard
- [ ] T012 [P] [US1] Create `packet.h` re-export (`graph_runtime/src/public/include/graph_runtime/packet.h`): `#include "src/stream/packet.h"` — GRAPH_RUNTIME_API context
- [ ] T013 [P] [US1] Create `graph_config.h` re-export (`graph_runtime/src/public/include/graph_runtime/graph_config.h`): `#include "src/config/graph_config.h"` — GRAPH_RUNTIME_API context
- [ ] T014 [P] [US1] Create `side_packet.h` re-export (`graph_runtime/src/public/include/graph_runtime/side_packet.h`): `#include "src/public/side_packet.h"` — GRAPH_RUNTIME_API context
- [ ] T015 [US1] Create umbrella header (`graph_runtime/src/public/include/graph_runtime/graph_runtime.h`): includes all public sub-headers via `#include "graph_runtime/..."` paths, ORDER: export → types → timestamp → packet → graph_config → side_packet
- [ ] T016 [US1] Create linker anchor (`graph_runtime/src/public/graph_runtime_init.cc`): references `NodeFactoryRegistry::RegisteredTypes()` and `ParserRegistry::IsRegistered()` to prevent linker stripping

**Checkpoint**: `bazel build //src/public:runtime` succeeds with new header layout.

---

## Phase 4: User Story 2 — Clear Public API Boundary (Priority: P1)

**Goal**: Clear separation between public and internal headers. Only `GRAPH_RUNTIME_API`-decorated symbols are exported. Internal `src/stream/`, `src/node/`, `src/scheduler/` paths are NOT accessible from public consumers.

**Independent Test**: `nm -gU bazel-bin/src/public/libruntime_shared.dylib` shows only public symbols; `public_api_test` compiles without depending on internal modules.

- [ ] T017 [US2] Add `cc_binary(name = "runtime_shared", linkshared = True, linkstatic = True)` to `src/public/BUILD.bazel` — produces `libruntime_shared.dylib` with `-DATLAS_SHARED_LIBRARY` copts
- [ ] T018 [US2] Create `public_api_test.cc` (`graph_runtime/src/tests/public_api_test.cc`): depends ONLY on `@graph_runtime//src/public:runtime`, tests that umbrella header compiles and all public types are accessible
- [ ] T019 [US2] Add `public_api_test` target to `graph_runtime/src/tests/BUILD.bazel` — deps only `@graph_runtime//src/public:runtime` and googletest
- [ ] T020 [US2] Build `runtime_shared` and verify symbols: `nm -gU bazel-bin/src/public/libruntime_shared.dylib | grep -v '^$'` — confirm no internal symbols leaked

**Checkpoint**: `bazel test //src/tests:public_api_test` passes. `bazel build //src/public:runtime_shared` succeeds.

---

## Phase 5: User Story 3 — External Dependency Setup & Consumer Demo (Priority: P2)

**Goal**: A single `graph_runtime_setup()` call in WORKSPACE fetches all transitive deps. The embedded consumer demo proves external consumption works.

**Independent Test**: Creating a new BUILD in `examples/consumer_demo/` that depends on `@graph_runtime//src/public:runtime` builds without errors.

- [ ] T021 [US3] Refine `graph_runtime_deps.bzl` — ensure `graph_runtime_setup()` is the single entry point, add `native.existing_rule()` guards for all deps
- [ ] T022 [US3] Create embedded consumer demo directory (`graph_runtime/examples/consumer_demo/`)
- [ ] T023 [US3] Create consumer demo WORKSPACE (`graph_runtime/examples/consumer_demo/WORKSPACE`): `local_repository(name = "graph_runtime", path = "../..")` + `load("@graph_runtime//:graph_runtime_deps.bzl", "graph_runtime_setup")` + `graph_runtime_setup()`
- [ ] T024 [US3] Create consumer demo BUILD (`graph_runtime/examples/consumer_demo/BUILD.bazel`): `cc_binary(name = "demo", srcs = ["main.cc"], deps = ["@graph_runtime//src/public:runtime"])`
- [ ] T025 [US3] Create consumer demo source (`graph_runtime/examples/consumer_demo/main.cc`): `#include "graph_runtime/graph_runtime.h"` + basic type usage (`GraphConfig`, `Timestamp`, `Packet::MakePacket`)
- [ ] T026 [US3] Create consumer demo test (`graph_runtime/examples/consumer_demo/demo_test.cc`): GoogleTest with `deps = ["@graph_runtime//src/public:runtime"]` verifying public types are accessible
- [ ] T027 [US3] Build and run consumer demo from root: `cd examples/consumer_demo && bazel test //...`

**Checkpoint**: `bazel test //...` from within `examples/consumer_demo/` succeeds, proving external consumption works.

---

## Phase 6: Polish & Cross-Cutting

**Purpose**: Final build verification, documentation updates, edge cases.

- [ ] T028 Run full build: `bazel build //src/...` — verify no BUILD.bazel uses `//src/` (all migrated to `@graph_runtime//`)
- [ ] T029 Run all tests: `bazel test //src/tests/...` — verify all existing tests still pass after dep prefix migration
- [ ] T030 Update `AGENTS.md` and any remaining doc references to reflect `@graph_runtime//` dep prefix convention

---

## Dependencies & Execution Order

```
Phase 1: Setup (T001–T003)
    │
Phase 2: Dep Migration (T004–T010)
    │  [P] T004–T010 — all BUILD files independent
    │
Phase 3: US1 (T011–T016)
    │
    ├──────────────────────┐
    │                      │
Phase 4: US2 (T017–T020)   │
    │                      │
    └──────────────────────┘
              │
        Phase 5: US3 (T021–T027)
              │
        Phase 6: Polish (T028–T030)
```

### Within Each Phase

```
Phase 3: US1
  ├── T011: timestamp.h re-export
  ├── T012–T014: packet.h, graph_config.h, side_packet.h [P — all independent]
  ├── T015: umbrella header (depends on T011–T014)
  └── T016: graph_runtime_init.cc

Phase 5: US3
  ├── T021: graph_runtime_deps.bzl
  ├── T022: consumer_demo directory
  ├── T023–T026: WORKSPACE, BUILD, main.cc, test [sequential]
  └── T027: build and run consumer demo
```

---

## Parallel Opportunities

| Phase | Parallel Tasks | Rationale |
|-------|---------------|-----------|
| Phase 2 | T004, T005, T006, T007, T008, T009, T010 | All BUILD.bazel files — independent per module |
| Phase 3 | T012, T013, T014 | packet.h, graph_config.h, side_packet.h — independent |
| Phase 4 | T017, T018 | runtime_shared target and public_api_test — different files |

---

## Implementation Strategy

### MVP Scope (US1 + US2 — both P1)

1. **Phase 1 + 2**: Export macro + dep prefix migration + BUILD refactor
2. **Phase 3**: Public re-export headers + umbrella + linker anchor
3. **Phase 4**: runtime_shared + public_api_test + symbol verification
4. **STOP and VALIDATE**: `bazel test //src/tests:public_api_test` + `bazel build //src/public:runtime_shared`

### Incremental Delivery

1. **Phase 1 + 2**: Build infrastructure ready, dep prefix migrated
2. **+ US1 (Phase 3)**: Public header layer complete — consumable via `@graph_runtime//src/public:runtime`
3. **+ US2 (Phase 4)**: API boundary verified, shared library working
4. **+ US3 (Phase 5)**: External consumer demo proves end-to-end consumption
5. **+ Phase 6**: Final validation and documentation

### External Consumer Demo

The embedded `examples/consumer_demo/` directory simulates an external project:

```
graph_runtime/examples/consumer_demo/
├── WORKSPACE              — local_repository pointing to ../..
├── BUILD.bazel            — deps = ["@graph_runtime//src/public:runtime"]
├── main.cc                — #include "graph_runtime/graph_runtime.h"
└── demo_test.cc           — GoogleTest verifying public API
```

Run from root: `cd graph_runtime/examples/consumer_demo && bazel test //...`
