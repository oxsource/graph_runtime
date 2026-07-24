# Implementation Plan: Core Module Framework Restructure

**Branch**: `008-module-directory-restructure` | **Date**: 2026-07-24 | **Spec**: [spec.md](./spec.md)

## Summary

Move 7 internal modules from `src/` to `src/framework/`. Update ~92 BUILD dep paths and ~175 `#include` directives. No logic changes.

## Technical Context

**Language/Version**: N/A — Bazel build + C++ include paths only
**Scope**: 7 directories moved, ~92 BUILD labels updated, ~175 include paths updated
**Testing**: `bazel test //...` — 14 existing tests must pass unchanged
**Reference**: `specs/006-bazel-visibility-control/` (previous visibility work sets precedent for directory-level changes)

## Constitution Check

| Principle | Status | Notes |
|-----------|--------|-------|
| V. Build System Integrity | ✅ PASS | Directory restructuring does not affect build system integrity; all labels are mechanically rewritable |
| Design: Public API path | ✅ PASS | `strip_include_prefix = "include"` under `src/public/` becomes `src/framework/public/` — consumer-facing path unchanged (`graph_runtime/xxx.h`) |

## Migration Impact — Quantified

| Metric | Count |
|--------|-------|
| Directories to `git mv` | 7 |
| BUILD files to update | ~12 |
| `@graph_runtime//src/...` BUILD deps | ~92 |
| `#include "src/..."` source paths | ~175 |
| Visibility `//src:` references | ~8 |
| Doc files to update | 3 (AGENTS.md, docs/build-conventions.md, .specify/feature.json) |
| Directories staying in `src/` | 2 (examples/, tests/) |

## Target Layout

```text
src/                               src/
├── examples/         stays        ├── examples/
├── tests/            stays        ├── tests/
├── config/          ──move──→     └── framework/
├── hook/            ──move──→         ├── config/
├── log/             ──move──→         ├── hook/
├── node/            ──move──→         ├── log/
├── public/          ──move──→         ├── node/
├── scheduler/       ──move──→         ├── public/
└── stream/          ──move──→         ├── scheduler/
                                       └── stream/
```

## Phases

### Phase 1 — File Migration

1. `git mv src/config/ src/framework/config/`
2. `git mv src/hook/ src/framework/hook/`
3. `git mv src/log/ src/framework/log/`
4. `git mv src/node/ src/framework/node/`
5. `git mv src/public/ src/framework/public/`
6. `git mv src/scheduler/ src/framework/scheduler/`
7. `git mv src/stream/ src/framework/stream/`

All 7 moves use `git mv` — history preserved. At this point the source tree reflects the new layout but nothing compiles yet.

### Phase 2 — Include Path Updates

Use `sed` or `rg --replace` to update all `#include` directives in moved `.cc` and `.h` files:

```bash
rg -l '#include "src/' src/framework/ --type cc --type h | xargs sed -i '' 's|#include "src/|#include "src/framework/|g'
```

This covers ~175 occurrences across 7 directories.

### Phase 3 — BUILD Dep Updates

Update all `@graph_runtime//src/...` references in BUILD.bazel files to `@graph_runtime//src/framework/...`:

```bash
rg -l '@graph_runtime//src/' --glob '**/BUILD.bazel' | xargs sed -i '' 's|@graph_runtime//src/|@graph_runtime//src/framework/|g'
```

Then fix the two directories that stay in `src/` (examples/, tests/) — their deps should NOT be updated since they reference the moved framework modules. Actually, they should be updated since they depend on framework modules.

Then update `//src/` → `//src/framework/` in `src/examples/` and `src/tests/` BUILD files.

Also update visibility rules: `//src:__subpackages__` → `//src/framework:__subpackages__`.

### Phase 4 — Visibility Rule Fixes

In `src/framework/hook/BUILD.bazel`:
```
//src:__subpackages__  →  //src/framework:__subpackages__
//src/tests:__subpackages__  stays the same
```

In `src/framework/log/BUILD.bazel` and other internal frameworks:
```
//src:__subpackages__  →  //src/framework:__subpackages__
```

### Phase 5 — Documentation & Config

1. Update `AGENTS.md` — point plan reference to `008-core-module-framework/plan.md`; update any path references
2. Update `docs/build-conventions.md` — `src/public/` → `src/framework/public/`, `src/log/` → `src/framework/log/`, etc.
3. Update `.specify/feature.json` if needed

### Phase 6 — Validation

1. `bazel build //...` — must succeed
2. `bazel test //...` — must pass 14/14
3. `bazel build //src/framework/public:runtime_shared` — shared library build
4. Verify zero stale references:
   ```bash
   rg '#include "src/' src/framework/  # should be ZERO (all updated to src/framework/)
   rg '@graph_runtime//src/' --glob '**/BUILD.bazel'  # should be ZERO
   ```
5. `bazel clean && bazel test //...` — full rebuild verification
