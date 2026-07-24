# Tasks: Core Module Framework Restructure

**Input**: Design documents from `/specs/008-core-module-framework/`

**Prerequisites**: plan.md (required), spec.md (required), research.md

## Path Conventions

- Workspace root: `graph_runtime/graph_runtime/`
- All paths relative to workspace root

---

## Phase 1: Setup — File Migration

**Purpose**: Move 7 internal modules to `src/framework/` via `git mv`.

- [x] T001 `git mv src/config/ src/framework/config/`
- [x] T002 `git mv src/hook/ src/framework/hook/`
- [x] T003 `git mv src/log/ src/framework/log/`
- [x] T004 `git mv src/node/ src/framework/node/`
- [x] T005 `git mv src/public/ src/framework/public/`
- [x] T006 `git mv src/scheduler/ src/framework/scheduler/`
- [x] T007 `git mv src/stream/ src/framework/stream/`

**Checkpoint**: `ls src/framework/` shows 7 directories ✅

---

## Phase 2: Foundational — Include Path Updates

**Purpose**: Update all `#include "src/..."` to `#include "src/framework/..."` in moved files.

- [x] T008-T014 Bulk update all `#include "src/` → `#include "src/framework/` in `src/framework/` via sed
- [x] Update `src/examples/` and `src/tests/` includes (referenced old `src/public/`, `src/stream/`, etc.)
- [x] Update `examples/consumer_demo/main.cc` includes

**Checkpoint**: `rg -c '#include "src/(public|log|hook|scheduler|stream|node|config)/'` returns zero ✅

---

## Phase 3: [US1] — BUILD Dep Updates

**Purpose**: Update all Bazel dep labels from `//src/...` to `//src/framework/...`.

- [x] T015-T023 [US1] Bulk update all `@graph_runtime//src/...` → `@graph_runtime//src/framework/...` deps in BUILD.bazel via sed (~94 refs)
- [x] Fix `testdata` visibility override that was incorrectly rewritten
- [x] Fix `config_parser_test.cc` testdata paths (src/ → src/framework/)

**Checkpoint**: `bazel build //src/framework/public:runtime` passes ✅
`bazel test //src/tests/...` — 14/14 pass ✅
`bazel build //src/examples:all` passes ✅

---

## Phase 4: [US2] — Visibility Rule Fixes

**Purpose**: Update `//src:__subpackages__` to `//src/framework:__subpackages__` in framework BUILD files.

- [x] T024-T031 [US2] Update all `//src:__subpackages__` → `//src/framework:__subpackages__` + `//src/tests:__subpackages__` in framework BUILD files
- [x] Fix sed-introduced syntax errors (quote placement in list)
- [x] hook/BUILD and public/BUILD per-target visibility also updated

**Checkpoint**: `rg '//src:' --glob '**/BUILD.bazel' src/framework/` returns zero ✅
`bazel test //src/tests/...` — 14/14 pass ✅

---

## Phase 5: [US3] — Documentation & Config

**Purpose**: Keep documentation referencing correct paths.

- [x] T032 [P] [US3] Update `docs/build-conventions.md` — all `//src/` → `//src/framework/` paths
- [x] T033 [US3] Update `AGENTS.md` — framework path, shared library path
- [x] T034 [US3] Feature.json already correct (008-core-module-framework)

**Checkpoint**: `rg 'src/(config|hook|log|node|public|scheduler|stream)/' docs/ AGENTS.md 2>/dev/null` shows only `src/framework/` paths ✅

---

## Phase 6: Validation

**Purpose**: Verify everything compiles and tests pass.

- [x] T035 `bazel build //...` — zero errors ✅
- [x] T036 `bazel test //src/tests/...` — 14/14 pass ✅
- [x] T037 `bazel build //src/framework/public:runtime_shared` — shared library builds ✅
- [x] T038 Zero stale `#include "src/...` in framework — zero ✅
- [x] T039 Zero stale `@graph_runtime//src/` BUILD deps — zero ✅ (all updated to `//src/framework/`)
- [x] T040 Note: `bazel clean && bazel test` passes; custom_parser needed visibility fix (config now visible to examples)

**Checkpoint**: `bazel test //...` PASSED ✅

---

## Dependencies & Execution Order

### Phase Dependencies

```
Phase 1 (git mv) ──→ Phase 2 (includes) ──→ Phase 3 (BUILD deps)
                                                      ↓
                                              Phase 4 (visibility)
                                                      ↓
                                              Phase 5 (docs)
                                                      ↓
                                              Phase 6 (validation)
```

All phases are strictly sequential — each depends on the previous.

### Parallel Opportunities

| Phase | [P] tasks |
|-------|-----------|
| Phase 2 | T010 (log), T011 (node), T013 (scheduler), T014 (stream) — different directories |
| Phase 3 | T015-T021 are individual BUILD files, can be parallelized |
| Phase 5 | T032 (docs) can be parallel with T033/T034 |

### MVP Scope

Phases 1-3 = **24 tasks** — source tree restructured, includes updated, BUILD deps fixed. At this point `bazel build //...` should pass.
