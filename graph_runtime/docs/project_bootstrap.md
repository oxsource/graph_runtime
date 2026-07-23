# Graph Runtime - Project Bootstrap

> Version: 0.1
>
> Status: Draft
>
> Purpose: Initial project definition for AI-assisted specification-driven development.

---

# 1. Vision (Why)

## 1.1 Project Vision

Graph Runtime 是一个轻量级、可扩展、基于 Stream 的图运行时（Graph Runtime）框架。

项目参考 MediaPipe 的 Stream-Based Graph 设计思想，但不追求完全兼容 MediaPipe，而是结合实际业务需求，构建一个更加简单、易理解、易维护的图运行时。

MediaPipe 参考源码位于 `/Users/moks/Develop/docker/ubuntu24/codes/mediapipe`，后续实现有困难时可参考其 stream-based 调度器、Calculator 框架、Packet 传递等核心机制的设计实现。

Atlas 参考源码位于 `/Users/moks/Develop/docker/ubuntu24/codes/atlas`，其 Bazel 构建配置（atlas_deps.bzl、platforms、.bazelrc）、公共 API 导出模式（ATLAS_API 宏、umbrella header）、以及跨平台编译的设计均可作为参考。

一期目标专注于 Runtime Core，为 DVR、AVM、DMS 等视觉算法项目提供统一的数据流执行基础。

---

## 1.2 Goals

Graph Runtime 希望实现以下目标：

- 提供统一的 Stream-Based 图执行框架。
- 将图结构与业务逻辑完全解耦。
- 使用配置驱动 Graph 构建，而非硬编码流程。
- 作为独立基础库，供其他 Bazel 项目依赖使用。
- 保持模块化设计，便于未来持续扩展。

---

## 1.3 Non-Goals (Phase 1)

一期不包含以下内容：

- 可视化编辑器
- 动态 Graph（详见 §3.7 Dynamic Graph）
- Graph 优化器
- 分布式执行

这些能力将在后续版本根据需求逐步扩展。

---

# 2. Requirements (What)

## 2.1 Functional Requirements

一期需要完成以下能力：

### FR-001 Stream-Based Graph

Graph 采用 Stream-Based 数据流模型。

Node 不直接引用其他 Node。

所有数据均通过 Stream 进行传递。

---

### FR-002 Configuration Driven

Graph 必须完全由配置文件描述。

Runtime 不允许硬编码业务流程。

---

### FR-003 Graph Construction

Runtime 应支持：

- 配置解析
- Graph 构建
- Node 创建
- Stream 创建
- Graph 初始化

---

### FR-004 Runtime Scheduling

Runtime 应负责：

- Node 生命周期管理
- Stream 生命周期管理
- Node 调度
- Stream 数据传递

一期采用简单、可维护的默认调度策略。

---

### FR-005 Public Library

Graph Runtime 应作为 Bazel Library 提供。

其他项目可直接依赖：

```python
deps = [
    "@graph_runtime//src/public:runtime",
]
```

---

### FR-006 Extensible Configuration

一期默认支持：

- JSON

同时配置解析器必须采用可扩展设计。

未来允许增加：

- YAML
- Protobuf
- DSL
- XML
- 其他格式

无需修改 Runtime。

---

### FR-007 Example

项目必须提供一个完整的 String Pipeline 示例。

用于验证：

- 配置解析
- Graph 构建
- Stream 调度
- Node 执行

作为整个 Runtime 的 MVP。

---

## 2.2 Non-Functional Requirements

- 轻量
- 模块化
- 易扩展
- 易测试
- 与业务解耦
- 面向长期维护

---

# 3. Design (How)

## 3.1 Overall Architecture

```

Configuration File
│
▼
Config Parser
│
▼
GraphConfig
│
▼
Graph Builder
│
▼
Runtime Graph
│
▼
Scheduler
│
▼
Node Execution

```

Runtime 永远只依赖 GraphConfig，不依赖具体配置格式。

---

## 3.2 Core Modules

一期仅包含以下核心模块：

```

WORKSPACE
BUILD.bazel                (root alias: //:runtime → //src/public:runtime)
.bazelversion              (6.5.0)
.bazelrc

graph_runtime_deps.bzl     (外部依赖管理，参考 atlas_deps.bzl)

platforms/
├── BUILD
└── platforms.bzl          (config_setting + platform 定义 + select 宏)

src/
├── public/
│   ├── BUILD              (汇总 target，strip_include_prefix)
│   ├── include/
│   │   └── graph_runtime/
│   │       ├── graph_runtime.h          (umbrella header)
│   │       ├── graph_runtime_export.h   (GRAPH_RUNTIME_API 宏)
│   │       ├── graph.h
│   │       ├── packet.h
│   │       ├── node.h
│   │       └── types.h
│   └── graph_runtime_init.cc           (shared lib init)
│
├── config/
│   ├── parser/
│   └── json/
│
├── graph/
│
├── runtime/
│
├── scheduler/
│
├── stream/
│
├── node/
│
├── examples/
│
└── tests/

```

---

## 3.3 Graph Model

Graph 由以下核心对象组成：

- Node
- Input Stream
- Output Stream
- Packet

其中：

Node 负责计算。

Stream 负责数据流动。

Runtime 负责调度。

---

## 3.4 Configuration Architecture

一期默认使用 JSON。

但 Runtime 不直接依赖 JSON。

采用统一配置解析接口：

```cpp
class IGraphConfigParser {
public:
    virtual GraphConfig Parse(const std::string& file) = 0;
};
```

未来新增配置格式时，仅需实现新的 Parser。

例如：

```

JSON Parser
YAML Parser
Proto Parser
DSL Parser

↓

GraphConfig

```

Runtime 无需修改。

---

## 3.5 Runtime Responsibilities

Runtime 负责：

- Graph 生命周期
- Node 生命周期
- Stream 生命周期
- Graph 初始化
- Graph 执行
- Graph 销毁

Runtime 不负责任何业务算法。

---

## 3.6 Extension Points

一期预留以下扩展接口：

- GraphConfigParser
- Calculator Factory
- Scheduler
- Stream
- Node

后续版本允许替换默认实现。

---

## 3.7 Dynamic Graph（未来设计）

### 3.7.1 设计目标（Phase 2+）

动态 Graph 旨在解决静态拓扑无法应对运行时变更的问题，目标包括：

- **运行时动态增删 Node**：根据业务需求在 Graph 执行过程中添加或移除 Calculator
- **运行时动态增删 Stream**：修改 Node 间的数据连接拓扑
- **热加载配置**：在不重启 Graph 的情况下应用新的 GraphConfig
- **动态 Node 生命周期**：Node 可独立于 Graph 创建和销毁

### 3.7.2 Phase 1 预留的扩展点

动态 Graph 不会在一期实现，但以下接口设计已为其预留空间：

- **GraphBuilder**：接口支持增量构建（`AddNode()` / `AddStream()`），未来可通过同一接口实现运行时变更
- **Scheduler**：预留 `AddNode()` / `RemoveNode()` 虚方法（一期为空实现），后续子类可覆写
- **Node 生命周期**：`Create()` / `Destroy()` 与 Graph 生命周期解耦，便于独立启停
- **GraphConfig**：保留版本号和增量更新字段，为配置热加载提供基础

### 3.7.3 已知挑战

| 挑战 | 说明 |
|------|------|
| 数据一致性 | 拓扑变更时 in-flight Packet 的处理策略（drain / cancel / wait） |
| 线程安全 | Scheduler 工作线程与变更操作之间需要互斥同步 |
| Stream 重连 | 下游 Node 的输入 Stream 变更后需要通知机制 |
| 资源管理 | 动态 Node 的内存分配与销毁，避免泄漏 |
| 死锁预防 | 暂停调度 → 变更拓扑 → 恢复调度的流程需避免死锁 |

### 3.7.4 参考实现

Phase 2 设计时可以参考以下实现：

- **MediaPipe**：`CalculatorGraph::AddPacketCallback()` / `SetStreamCallback()` 的动态回调注册机制
- **Atlas**：`PipelineRegistry` 的动态节点注册模式，支持运行时注册/反注册 Calculator

---



# 4. MVP Deliverables

一期完成后应具备：

- Graph Runtime Library
- JSON Config Parser
- Graph Builder
- Runtime
- Default Scheduler
- String Pipeline Example
- Unit Tests
- Developer Documentation

---

# 5. Success Criteria

一期完成时，应满足以下目标：

- 能通过 JSON 描述任意 Stream Graph。
- Runtime 能正确解析配置并构建 Graph。
- Node 能通过 Stream 完成数据传递。
- String Pipeline 示例能够完整运行。
- Runtime 可作为独立 Bazel Library 被其他项目依赖。
- Runtime 与业务模块保持完全解耦。

---

# 6. Code Style & Commit Convention

## 6.1 Google C++ Coding Style

本项目严格遵守 [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)，所有 C++ 代码必须符合以下规范：

### Naming

| Category            | Style              | Example                  |
|---------------------|--------------------|--------------------------|
| File names          | `lowercase`        | `graph_builder.cc`       |
| Type/Class          | `PascalCase`       | `class GraphBuilder`     |
| Function            | `PascalCase`       | `void BuildGraph()`      |
| Variable            | `snake_case`       | `int stream_count`       |
| Member variable     | `snake_case_`      | `int stream_count_`      |
| Constant            | `kPascalCase`      | `const int kMaxStreams`  |
| Enum                | `kPascalCase`      | `kOk, kError`            |
| Namespace           | `snake_case`       | `namespace graph::runtime`|
| Macro               | `UPPER_SNAKE_CASE` | `RETURN_IF_ERROR`        |

### Formatting

- Indentation: 2 spaces (no tabs)
- Line length: 80 characters
- Use `nullptr`, not `NULL` or `0`
- Use `auto` sparingly, only when type is obvious
- Include order: related header, C++ standard library, third-party, project headers
- Use `//` for comments; `/* */` only for documentation blocks

### Ownership & Pointers

- Prefer `std::unique_ptr` over raw pointers
- Use raw pointers only for non-owning references
- Avoid `std::shared_ptr` unless ownership is truly shared

---

## 6.2 Commit Convention

本项目采用 [Conventional Commits](https://www.conventionalcommits.org/) 规范：

```
<type>(<scope>): <description>

[optional body]

[optional footer]
```

### Types

| Type       | Usage                                    |
|------------|------------------------------------------|
| `feat`     | New feature                              |
| `fix`      | Bug fix                                  |
| `docs`     | Documentation only                       |
| `style`    | Code style, formatting (no logic change) |
| `refactor` | Code restructuring (no bug fix, no feature) |
| `perf`     | Performance improvement                  |
| `test`     | Adding or fixing tests                   |
| `build`    | Build system (Bazel, etc.)               |
| `ci`       | CI/CD changes                            |
| `chore`    | Maintenance, dependencies, etc.          |

### Scope

| Scope           | Area                          |
|-----------------|-------------------------------|
| `config`        | Config parser module          |
| `graph`         | Graph model & builder         |
| `runtime`       | Runtime execution engine      |
| `scheduler`     | Scheduler module              |
| `stream`        | Stream & Packet               |
| `node`          | Calculator node               |
| `public`        | Public API (src/public/)             |
| `example`       | Example pipeline              |
| `build`         | Bazel build files             |
| `docs`          | Documentation                 |

### Examples

```
feat(config): add JSON config parser
fix(runtime): resolve deadlock on graph shutdown
docs(public): add Packet API usage example
build(bazel): upgrade to Bazel 6.5
```

### Commit Guidelines

- Description must be lowercase, imperative mood, no period
- Body explains what and why, not how
- Footer may reference issues: `Closes #123`, `Fixes #456`

---

## 6.3 Public API Export Macro

参考 atlas 的 `atlas_export.h` 模式，所有公开接口使用 `GRAPH_RUNTIME_API` 宏控制符号可见性：

```cpp
// graph_runtime_export.h
#pragma once

#if defined(_WIN32)
  #if defined(GRAPH_RUNTIME_SHARED_LIBRARY)
    #define GRAPH_RUNTIME_API __declspec(dllexport)
  #else
    #define GRAPH_RUNTIME_API __declspec(dllimport)
  #endif
#else
  #if defined(GRAPH_RUNTIME_SHARED_LIBRARY)
    #define GRAPH_RUNTIME_API __attribute__((visibility("default")))
  #else
    #define GRAPH_RUNTIME_API
  #endif
#endif
```

- 编译单元统一使用 `-fvisibility=hidden`
- 仅 `GRAPH_RUNTIME_API` 修饰的符号被导出
- `cc_binary(linkshared=True, linkstatic=True)` 构建共享库时定义 `GRAPH_RUNTIME_SHARED_LIBRARY`

### Public Header Layout

```
src/public/include/graph_runtime/
├── graph_runtime.h             (umbrella header)
├── graph_runtime_export.h      (export macro)
├── graph.h
├── packet.h
├── node.h
└── types.h
```

Umbrella header 模式（参考 `atlas.h`）：

```cpp
// graph_runtime.h
#pragma once

#include "graph_runtime/graph_runtime_export.h"
#include "graph_runtime/graph.h"
#include "graph_runtime/packet.h"
#include "graph_runtime/node.h"
#include "graph_runtime/types.h"
```

外部消费者只需 `#include "graph_runtime/graph_runtime.h"`。

---

# 7. Build System (Bazel 6.5)

## 7.1 Version Requirement

本项目要求 Bazel 版本 **6.5.x**，`.bazelversion` 文件内容：

```
6.5.0
```

## 7.2 .bazelrc

参考 atlas 的 `.bazelrc` 设计，按平台分层配置：

```text
# .bazelrc

# Concurrency
build --jobs 128
build --enable_platform_specific_config

# C++ standard (per-platform)
build:linux --cxxopt=-std=c++17
build:linux --host_cxxopt=-std=c++17

build:macos --cxxopt=-std=c++17
build:macos --host_cxxopt=-std=c++17

# Platform shortcuts (use: bazel build //... --config=<name>)
build:macos_arm64    --platforms=//platforms:macos_arm64_platform
build:macos_x86_64   --platforms=//platforms:macos_x86_64_platform
build:linux_x86_64   --platforms=//platforms:linux_x86_64_platform
build:linux_aarch64  --platforms=//platforms:linux_aarch64_platform
```

## 7.3 Repository Layout

```
WORKSPACE                     (workspace(name = "graph_runtime"))
BUILD.bazel                   (root — alias //:runtime)
.bazelversion                 (6.5.0)
.bazelrc

graph_runtime_deps.bzl        (外部依赖 bootstrap 宏)

platforms/
├── BUILD                     (config_setting + platform 定义)
└── platforms.bzl             (config_setting_and_platform + graph_runtime_select)

src/
├── public/
│   ├── BUILD                 (汇总 target, 外部消费入口)
│   ├── include/
│   │   └── graph_runtime/
│   │       ├── graph_runtime.h
│   │       ├── graph_runtime_export.h
│   │       ├── graph.h
│   │       ├── packet.h
│   │       ├── node.h
│   │       └── types.h
│   └── graph_runtime_init.cc
├── config/
│   ├── BUILD.bazel
│   ├── parser/
│   └── json/
├── graph/
│   └── BUILD.bazel
├── runtime/
│   └── BUILD.bazel
├── scheduler/
│   └── BUILD.bazel
├── stream/
│   └── BUILD.bazel
├── node/
│   └── BUILD.bazel
├── examples/
│   └── BUILD.bazel
└── tests/
    └── BUILD.bazel
```

## 7.4 WORKSPACE

```python
workspace(name = "graph_runtime")

load("//:graph_runtime_deps.bzl", "graph_runtime_deps")

graph_runtime_deps()
```

## 7.5 Root BUILD.bazel

```python
# Root BUILD.bazel — top-level alias for external consumers.

package(default_visibility = ["//visibility:public"])

alias(
    name = "runtime",
    actual = "//src/public:runtime",
)
```

## 7.6 graph_runtime_deps.bzl

参考 atlas 的 `atlas_deps.bzl`，统一管理所有第三方依赖：

```python
"""Dependency bootstrap for external Bazel projects consuming Graph Runtime.

Usage in external project's WORKSPACE:

    http_archive(
        name = "graph_runtime",
        url = "https://github.com/<org>/graph_runtime/archive/v1.0.0.tar.gz",
        sha256 = "<sha256>",
        strip_prefix = "graph_runtime-1.0.0",
    )

    load("@graph_runtime//:graph_runtime_deps.bzl", "graph_runtime_setup")
    graph_runtime_setup()
"""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def graph_runtime_deps():
    """Fetches all third-party deps required by Graph Runtime (internal)."""
    if not native.existing_rule("googletest"):
        http_archive(
            name = "googletest",
            url = "https://github.com/google/googletest/archive/refs/tags/release-1.12.1.tar.gz",
            sha256 = "81964fe578e9bd7c94dfdb09c8e4d6e6759e19967e397dbea48d1c10e45d0df2",
            strip_prefix = "googletest-release-1.12.1",
        )

def graph_runtime_setup():
    """One-call setup for external projects consuming Graph Runtime."""
    graph_runtime_deps()
```

## 7.7 platforms/

### platforms/BUILD

参考 atlas 的 `platforms/BUILD`，使用宏批量定义 `config_setting` + `platform`：

```python
load(":platforms.bzl", "config_setting_and_platform")

package(default_visibility = ["//visibility:public"])

config_setting_and_platform(
    name = "macos_arm64",
    constraint_values = [
        "@platforms//os:macos",
        "@platforms//cpu:arm64",
    ],
)

config_setting_and_platform(
    name = "macos_x86_64",
    constraint_values = [
        "@platforms//os:macos",
        "@platforms//cpu:x86_64",
    ],
)

config_setting_and_platform(
    name = "linux_x86_64",
    constraint_values = [
        "@platforms//os:linux",
        "@platforms//cpu:x86_64",
    ],
)

config_setting_and_platform(
    name = "linux_aarch64",
    constraint_values = [
        "@platforms//os:linux",
        "@platforms//cpu:aarch64",
    ],
)
```

### platforms/platforms.bzl

```python
"""Platform build helpers.

Provides:
  - config_setting_and_platform(): create config_setting + platform pairs.
  - graph_runtime_select(): convenience select() for consumers.
"""

def config_setting_and_platform(name, constraint_values):
    native.config_setting(
        name = name,
        constraint_values = constraint_values,
        visibility = ["//visibility:public"],
    )
    native.platform(
        name = name + "_platform",
        constraint_values = constraint_values,
        visibility = ["//visibility:public"],
    )

def graph_runtime_select(
        macos_arm64 = None,
        macos_x86_64 = None,
        linux_aarch64 = None,
        linux_x86_64 = None,
        macos = None,
        linux = None,
        default = None):
    """Platform-aware select() for Graph Runtime consumers."""
    return select({
        "//platforms:macos_arm64":    macos_arm64 or macos or default,
        "//platforms:macos_x86_64":   macos_x86_64 or macos or default,
        "//platforms:linux_aarch64":  linux_aarch64 or linux or default,
        "//platforms:linux_x86_64":   linux_x86_64 or linux or default,
        "//conditions:default": default,
    })
```

## 7.8 src/public/BUILD (Public Library Target)

参考 atlas 的 `src/public/BUILD`，汇总所有内部模块对外暴露：

```python
cc_library(
    name = "runtime",
    hdrs = glob(["include/graph_runtime/*.h"]),
    strip_include_prefix = "include",
    copts = [
        "-fvisibility=hidden",
        "-fvisibility-inlines-hidden",
        "-DGRAPH_RUNTIME_SHARED_LIBRARY",
    ],
    alwayslink = 1,
    deps = [
        "//src/config:config",
        "//src/graph:graph",
        "//src/runtime:runtime",
        "//src/scheduler:scheduler",
        "//src/stream:stream",
        "//src/node:node",
    ],
    visibility = ["//visibility:public"],
)

# Shared library for non-Bazel consumers
cc_binary(
    name = "runtime_shared",
    srcs = ["graph_runtime_init.cc"],
    linkshared = True,
    linkstatic = True,
    copts = [
        "-fvisibility=hidden",
        "-fvisibility-inlines-hidden",
        "-DGRAPH_RUNTIME_SHARED_LIBRARY",
    ],
    deps = [":runtime"],
    visibility = ["//visibility:public"],
)
```

### Strip Include Prefix 说明

`strip_include_prefix = "include"` 使得外部消费者通过 `#include "graph_runtime/graph_runtime.h"` 引入头文件，无需感知内部路径 `src/public/include`。

## 7.9 Internal Module BUILD Rules

每个内部模块遵循以下模式：

```python
# src/config/BUILD.bazel

cc_library(
    name = "config",
    srcs = glob(["*.cc"]),
    hdrs = glob(["*.h"]),
    deps = [
        "//src/graph:graph",
    ],
    visibility = ["//visibility:public"],
)
```

### Rules

- `hdrs` 使用 `glob(["*.h"])` 自动匹配
- `srcs` 使用 `glob(["*.cc"])` 自动匹配
- 通过 `deps` 明确声明依赖关系
- 内部模块默认 `visibility = ["//visibility:public"]`，仅对 `src/` 内其他模块可见
- 禁止使用 `copts = ["-std=c++17"]`，统一在 `.bazelrc` 中配置
- 仅 `src/public/BUILD` 中使用 `alwayslink = 1`（汇总 target）

## 7.10 Example BUILD Rules

```python
# src/examples/BUILD.bazel

cc_binary(
    name = "string_pipeline",
    srcs = ["string_pipeline.cc"],
    deps = [
        "//src/public:runtime",
    ],
)
```

外部示例统一依赖 `//src/public:runtime`，禁止直接引用内部模块。

## 7.11 Test BUILD Rules

```python
# src/tests/BUILD.bazel

cc_test(
    name = "graph_builder_test",
    srcs = ["graph_builder_test.cc"],
    deps = [
        "//src/public:runtime",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "config_parser_test",
    srcs = ["config_parser_test.cc"],
    deps = [
        "//src/config:config",
        "@googletest//:gtest_main",
    ],
)
```

- 集成测试依赖 `//src/public:runtime`
- 单元测试可按需依赖具体内部模块