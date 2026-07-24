# Research: Core Module Framework Restructure

## Design Decisions

### Decision 1: git mv vs copy+delete

- **Decision**: Use `git mv` for all 7 directory moves.
- **Rationale**: Preserves file history. Bazel identifies targets by path, so the move is transparent to the build system after label updates.
- **Alternatives considered**: Copy+delete (loses history), symlinks (not compatible with Bazel).

### Decision 2: Batch sed for include paths vs manual

- **Decision**: Use `rg | xargs sed` for bulk update of ~175 include paths.
- **Rationale**: The pattern `#include "src/...` → `#include "src/framework/...` is purely mechanical. Manual editing would be error-prone for 175 occurrences.
- **Risk**: `sed` may accidentally match non-path strings. Mitigation: preview with `rg` first, then verify with `rg` afterward.

### Decision 3: Order of moves and updates

- **Decision**: Move all directories FIRST (Phase 1), then update paths (Phase 2-3).
- **Rationale**: Moving files doesn't break `git` tracking; `sed` updates only succeed if files are in their new location.
- **Alternatives considered**: Move-and-update per directory (slower, more context switches).

### Decision 4: Handling `//src:__subpackages__` visibility

- **Decision**: Update to `//src/framework:__subpackages__` for all internal packages.
- **Rationale**: The Bazel package group `//src:__subpackages__` covered everything under `src/`. After the move, framework modules should only be visible to other framework modules + tests.
- **Note**: `//src/framework/hook/BUILD.bazel` has special visibility `["//src/framework:__subpackages__", "//src/tests:__subpackages__"]`.

## Scope Summary

| Item | Count | Method |
|------|-------|--------|
| Directories moved | 7 | `git mv` |
| Include paths updated | ~175 | `sed` |
| BUILD dep labels updated | ~92 | `sed` |
| Visibility rules updated | ~8 | Manual |
| Doc files updated | 3 | Manual |
