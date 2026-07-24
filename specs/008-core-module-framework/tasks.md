# Tasks: Core Module Framework Restructure

**Input**: Design documents from `/specs/008-core-module-framework/`

**Prerequisites**: plan.md (required), spec.md (required), research.md

## Path Conventions

- Workspace root: `graph_runtime/graph_runtime/`
- All paths relative to workspace root

---

## Phase 1: Setup — File Migration

**Purpose**: Move 7 internal modules to `src/framework/` via `git mv`.

- [ ] T001 `git mv src/config/ src/framework/config/`
- [ ] T002 `git mv src/hook/ src/framework/hook/`
- [ ] T003 `git mv src/log/ src/framework/log/`
- [ ] T004 `git mv src/node/ src/framework/node/`
- [ ] T005 `git mv src/public/ src/framework/public/`
- [ ] T006 `git mv src/scheduler/ src/framework/scheduler/`
- [ ] T007 `git mv src/stream/ src/framework/stream/`

**Checkpoint**: `ls src/framework/` shows 7 directories ✅

---

## Phase 2: Foundational — Include Path Updates

**Purpose**: Update all `#include "src/..."` to `#include "src/framework/..."` in moved files.

- [ ] T008 Update includes in `src/framework/config/` — all `.h` and `.cc` files
- [ ] T009 Update includes in `src/framework/hook/`
- [ ] T010 [P] Update includes in `src/framework/log/`
- [ ] T011 [P] Update includes in `src/framework/node/`
- [ ] T012 Update includes in `src/framework/public/` (only non-`include/` files — public headers keep their paths)
- [ ] T013 [P] Update includes in `src/framework/scheduler/`
- [ ] T014 [P] Update includes in `src/framework/stream/`

**Checkpoint**: `rg '#include "src/' src/framework/` returns zero matches ✅

---

## Phase 3: [US1] — BUILD Dep Updates

**Purpose**: Update all Bazel dep labels from `//src/...` to `//src/framework/...`.

- [ ] T015 [P] [US1] Update `src/framework/config/BUILD.bazel` deps
- [ ] T016 [US1] Update `src/framework/hook/BUILD.bazel` deps
- [ ] T017 [US1] Update `src/framework/log/BUILD.bazel` deps
- [ ] T018 [US1] Update `src/framework/node/BUILD.bazel` deps
- [ ] T019 [US1] Update `src/framework/public/BUILD.bazel` deps
- [ ] T020 [US1] Update `src/framework/scheduler/BUILD.bazel` deps
- [ ] T021 [US1] Update `src/framework/stream/BUILD.bazel` deps
- [ ] T022 [US1] Update `src/examples/BUILD.bazel` deps (now point to `//src/framework/...`)
- [ ] T023 [US1] Update `src/tests/BUILD.bazel` deps (now point to `//src/framework/...`)

**Checkpoint**: `rg '@graph_runtime//src/' --glob '**/BUILD.bazel'` returns zero matches ✅

---

## Phase 4: [US2] — Visibility Rule Fixes

**Purpose**: Update `//src:__subpackages__` to `//src/framework:__subpackages__` in framework BUILD files.

- [ ] T024 [US2] Update `src/framework/hook/BUILD.bazel` — `//src:__subpackages__` → `//src/framework:__subpackages__`
- [ ] T025 [US2] Update `src/framework/log/BUILD.bazel` — `//src:__subpackages__` → `//src/framework:__subpackages__`
- [ ] T026 [US2] Update `src/framework/scheduler/BUILD.bazel` — `//src:__subpackages__` → `//src/framework:__subpackages__`
- [ ] T027 [US2] Update `src/framework/stream/BUILD.bazel` — `//src:__subpackages__` → `//src/framework:__subpackages__`
- [ ] T028 [US2] Update `src/framework/node/BUILD.bazel` — `//src:__subpackages__` → `//src/framework:__subpackages__`
- [ ] T029 [US2] Update `src/framework/config/BUILD.bazel` — `//src:__subpackages__` → `//src/framework:__subpackages__`
- [ ] T030 [US2] Update `src/framework/config/json/BUILD.bazel` — `//src:__subpackages__` → `//src/framework:__subpackages__`
- [ ] T031 [US2] Update `src/framework/public/BUILD.bazel` — per-target visibility stays (runtime_internal, graph_builder already `//src:__subpackages__` → `//src/framework:__subpackages__`)

**Checkpoint**: `rg '//src:' --glob '**/BUILD.bazel' src/framework/` returns zero matches ✅

---

## Phase 5: [US3] — Documentation & Config

**Purpose**: Keep documentation referencing correct paths.

- [ ] T032 [P] [US3] Update `docs/build-conventions.md` — all `src/public/` → `src/framework/public/`, `src/log/` → `src/framework/log/`, etc.
- [ ] T033 [US3] Update `AGENTS.md` — verify SPECKIT section points to `008-core-module-framework/plan.md` (already done in plan phase, verify)
- [ ] T034 [US3] Update `.specify/feature.json` if path references need updating

**Checkpoint**: `rg 'src/(config|hook|log|node|public|scheduler|stream)/' docs/ AGENTS.md 2>/dev/null` — only `src/framework/` paths ✅

---

## Phase 6: Validation

**Purpose**: Verify everything compiles and tests pass.

- [ ] T035 Run `bazel build //...` — verify zero errors
- [ ] T036 Run `bazel test //...` — verify 14/14 tests pass
- [ ] T037 Run `bazel build //src/framework/public:runtime_shared` — shared library builds
- [ ] T038 Verify zero stale references: `rg -c '#include "src/' src/framework/` — must be 0
- [ ] T039 Verify zero stale BUILD deps: `rg -c '@graph_runtime//src/' --glob '**/BUILD.bazel'` — must be 0
- [ ] T040 Run `bazel clean && bazel test //...` — full clean rebuild verification

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
