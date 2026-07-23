<!--
  Sync Impact Report — v0.0.0 → v1.0.0
  Version change: 0.0.0 → 1.0.0 (initial constitution)
  Modified principles: N/A (new file)
  Added sections: Core Principles (5 principles), Design Constraints, Development Workflow, Governance
  Removed sections: N/A
  Templates requiring updates:
    - .specify/templates/constitution-template.md ✅ updated (source template, no change needed)
    - .specify/templates/plan-template.md ✅ updated (no principle references to update)
    - .specify/templates/spec-template.md ✅ updated (no principle references to update)
    - .specify/templates/tasks-template.md ✅ updated (no principle references to update)
  Follow-up TODOs: None
-->

# Graph Runtime Constitution

## Core Principles

### I. Stream-Based Graph Architecture

Graph MUST adopt a Stream-Based dataflow model.

- Node MUST NOT directly reference other Nodes — all data passes through Streams.
- Stream handles data flow; Node handles computation; Runtime handles scheduling.
- Runtime MUST NOT implement any business logic; business algorithms reside in Calculator Nodes.
- Graph lifecycle (Init → Execute → Destroy) is the Runtime's sole responsibility.

### II. Configuration Driven

Graph topology MUST be fully described by configuration files.

- Runtime MUST NOT hardcode any business flow or pipeline topology.
- Configuration parser MUST implement a unified `IGraphConfigParser` interface.
- Phase 1 defaults to JSON; additional formats (YAML, Protobuf, DSL) MUST be addable without modifying Runtime.
- Runtime MUST depend only on `GraphConfig` — never on a specific config format.

### III. Modularity & Extensibility

Every core module MUST define a replaceable interface.

- Extension points reserved in Phase 1: `GraphConfigParser`, `Calculator Factory`, `Scheduler`, `Stream`, `Node`.
- Default implementations MAY be replaced in later phases without breaking existing code.
- Graph structure and business logic MUST remain fully decoupled.

### IV. Google C++ Code Style (NON-NEGOTIABLE)

All C++ code MUST strictly conform to the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html).

- Naming: `snake_case` for variables/files, `PascalCase` for types/functions, `kPascalCase` for constants.
- Indentation: 2 spaces, no tabs. Line length: 80 characters.
- Ownership: Prefer `std::unique_ptr`; raw pointers for non-owning references only.
- Public API symbols MUST be exported via the `GRAPH_RUNTIME_API` macro.
- All translation units MUST be compiled with `-fvisibility=hidden`; only `GRAPH_RUNTIME_API`-decorated symbols are exported.
- Commits MUST follow Conventional Commits format: `<type>(<scope>): <description>`.

### V. Build System Integrity

The project MUST build exclusively with Bazel 6.5.x.

- All third-party dependencies MUST be declared in `graph_runtime_deps.bzl` at the repository root.
- Platform definitions reside in `platforms/` — each platform gets a `config_setting` + `platform` pair via the `config_setting_and_platform` macro.
- Cross-platform selects MUST use `graph_runtime_select()` from `platforms/platforms.bzl`.
- The sole external entry point is `//src/public:runtime` — internal modules MUST NOT be referenced by external consumers.
- A shared library (`runtime_shared`) MAY be built via `cc_binary(linkshared=True, linkstatic=True)` for non-Bazel consumers.
- `alwayslink = 1` is permitted ONLY in `src/public/BUILD`.

## Design Constraints

- **Phase 1 Scope**: No visual editor, no dynamic Graph (see §3.7 in project_bootstrap.md), no Graph optimizer, no distributed execution.
- **Dynamic Graph**: Explicitly deferred to Phase 2+; Phase 1 MUST reserve extension points (Scheduler `AddNode()`/`RemoveNode()`, GraphBuilder incremental API, Node lifecycle decoupling).
- **Public API**: All public headers live under `src/public/include/graph_runtime/`. The umbrella header `graph_runtime.h` includes all public types.
- **Internal Visibility**: Internal modules (config/, graph/, runtime/, scheduler/, stream/, node/) use `visibility = ["//visibility:public"]` to allow cross-module access within `src/`, but MUST NOT be listed in external projects' `deps`.
- **Strip Include Prefix**: `src/public/BUILD` uses `strip_include_prefix = "include"` so consumers write `#include "graph_runtime/graph_runtime.h"`.
- **Reference Implementations**: MediaPipe (`/Users/moks/Develop/docker/ubuntu24/codes/mediapipe`) for stream-based scheduling; Atlas (`/Users/moks/Develop/docker/ubuntu24/codes/atlas`) for Bazel build conventions and public API export patterns.

## Development Workflow

- **Commit Format**: `<type>(<scope>): <description>` per Conventional Commits. Types include `feat`, `fix`, `docs`, `style`, `refactor`, `perf`, `test`, `build`, `ci`, `chore`.
- **Scope Mapping**: `config` | `graph` | `runtime` | `scheduler` | `stream` | `node` | `public` | `build` | `docs`.
- **Testing**: Unit tests MAY depend on specific internal modules; integration tests MUST depend on `//src/public:runtime`.
- **MVP Delivery**: Phase 1 deliverables are: Graph Runtime Library, JSON Config Parser, Graph Builder, Runtime, Default Scheduler, String Pipeline Example, Unit Tests, Developer Documentation.
- **Success Gate**: Runtime MUST be consumable as a standalone Bazel Library by external projects via `@graph_runtime//src/public:runtime`.

## Governance

This Constitution defines the non-negotiable principles and constraints of the Graph Runtime project. It supersedes all ad-hoc practices.

- **Amendments**: Any amendment MUST be documented in a pull request with clear rationale, reviewed, and approved before adoption.
- **Versioning**: Constitution version follows SemVer. MAJOR for principle removals/redefinitions; MINOR for new principles or materially expanded guidance; PATCH for clarifications and typo fixes.
- **Compliance Review**: Every specification and implementation plan MUST pass a Constitution Check before proceeding to development.
- **Violations**: If a plan violates a constitutional principle, the violation MUST be documented in the Complexity Tracking section of the plan with justification and a rejected simpler alternative.

**Version**: 1.0.0 | **Ratified**: 2026-07-23 | **Last Amended**: 2026-07-23
