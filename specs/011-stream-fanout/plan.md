# Implementation Plan: 流扇出支持（Stream Fan-Out / Multi-Consumer）

**Branch**: `011-stream-fanout` | **Date**: 2026-09-02 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/011-stream-fanout/spec.md`

**Note**: This template is filled in by the `/speckit.plan` command. See `.specify/templates/plan-template.md` for the execution workflow.

## Summary

让 graph_runtime 支持一路输出流被多个消费者（1→N fan-out）独立消费，数据模型按方案 B 对齐 MediaPipe：**输入侧 per-consumer-edge 独立 `InputStreamManager`**（消除按完整 `"port:stream"` 名全图去重导致的共享/折叠），**图输入（Graph Input Stream）建模为虚拟 source 的 `OutputStreamManager`**（`AddPacketToInputStream` 注入虚拟输出 manager、经 `AddMirror` 扇出到所有声明消费它的节点）。输出侧 `OutputStreamManager::AddMirror` + `PropagateUpdatesToMirrors`（copy 前 N-1、move 最后）既有实现与 MediaPipe 等价，保留复用。目的：修复 media_record 004 record+push 死锁，并让图输入与 node-to-node 边共用同一套 mirror/manager 机制（sub-MediaPipe 定位，避免后续返工）。

**执行模型**：graph_runtime 自身的 async 运行时（`Start`/`Schedule` + scheduler + stream manager），改动集中在 `GraphRuntime::Initialize` 接线与 GraphInput 注入/关闭路径；不新增线程模型/执行器。

## Technical Context

**Language/Version**: C++17（`.bazelrc` 强制 `-std=c++17`）

**Primary Dependencies**: 无新增外部依赖。改动涉及 graph_runtime 内部模块：
- `src/framework/stream/input_stream_manager.{h,cc}` — per-edge 队列（复用，语义不变）
- `src/framework/stream/output_stream_manager.{h,cc}` — `AddMirror`/`PropagateUpdatesToMirrors`（复用）
- `src/framework/scheduler/input_stream_handler.{h,cc}` — per-node handler 路由（复用）
- `src/framework/public/graph_runtime.{h,cc}` — `Initialize` 接线 + `AddPacketToInputStream`/`CloseInputStream`/图输入管理（主改动）
- `src/framework/config/...` — `ConfigValidator`/`GraphConfig`（复核，fan-out 语义下规则不变）
- 对照参考：`/Users/moks/Develop/docker/ubuntu24/codes/mediapipe/framework/{calculator_graph.cc,calculator_node.cc,input_stream_manager.*,output_stream_manager.*,input_stream_handler.*}`

**Storage**: 无持久化；纯内存流/队列。

**Testing**: Bazel `cc_test`（googletest），位于 `src/tests/`：
- 新增 fan-out 单测（1→N、图输入→N、Done/error 在 fan-out 下）
- 全量回归 `bazel test //...`

**Target Platform**: 与项目一致（macOS 开发宿主为主，跨平台 C++17）。

**Project Type**: library（graph runtime 框架）。

**Performance Goals**: fan-out 写路径与 MediaPipe 对齐：每包 1 次输出写 + N-1 copy + 1 move；无额外全量拷贝、无新增全局锁热点。fan-out 前后单消费者路径开销不变。

**Constraints**:
- 每消费方独立队列，不共享、不互相抢队。
- 图完成计数语义保持：仅 `config.input_streams` 计入完成。
- 单消费者既有行为（同步 `Schedule()` 与异步 `Start()`、`AddPacketToInputStream`/`CloseInputStream` 错误语义）不得回归。
- 环/back-edge、子图不在本期。

**Scale/Scope**: 单 Bazel workspace `graph_runtime/graph_runtime/`；改动集中于 `GraphRuntime::Initialize` + GraphInput 注入/关闭 + 测试。不新增仓库。

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

（graph_runtime 的 `.specify/memory/constitution.md` 沿用项目模板；本 feature 为纯框架内数据模型/接线对齐，无公共 API 破坏面之外的边界改动——新增能力而非重构现有单消费者路径。）

## Project Structure

### Documentation (this feature)

```text
specs/011-stream-fanout/
├── plan.md              # This file
├── research.md          # MediaPipe 对照 + 根因（方案 B 依据）
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/           # Phase 1 output
│   ├── public-api.md
│   └── execution-contract.md
├── checklists/
└── tasks.md             # Phase 2 output
```

### Source Code (repository root)

```text
graph_runtime/
└── graph_runtime/                                  (Bazel workspace)
    └── src/
        ├── framework/
        │   ├── public/
        │   │   ├── graph_runtime.h                 (图输入句柄/新增内部结构成员)
        │   │   └── graph_runtime.cc                (主改动：Initialize 接线 + 注入/关闭)
        │   ├── stream/
        │   │   ├── input_stream_manager.{h,cc}     (复核/小改：图输入虚拟 manager 归属)
        │   │   └── output_stream_manager.{h,cc}    (基本不动，必要时暴露 manager 列表)
        │   └── config/
        │       ├── config_validator.{h,cc}         (复核 fan-out 校验规则)
        │       └── graph_config.h                  (复核图输入字段语义)
        └── tests/
            ├── BUILD.bazel                          (新增 fanout_test 等)
            ├── fanout_graph_test.cc                 (新增：1→N / 图输入→N / Done/error)
            └── (既有 scheduler_test/stream_io_test/... 保持)
```

**Structure Decision**: 在既有模块内做增量接线改动。不新增目录/仓库；核心在 `GraphRuntime::Initialize`（把"按流名建共享输入 manager"改为"按（节点, 输入流）建 per-edge manager + 把图输入声明建为虚拟输出 manager + mirror 扇出"）。测试沿用 `src/tests/`。

## Complexity Tracking

> **Fill ONLY if Constitution Check has violations that must be justified**

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| 图输入需要内部"虚拟 source 输出 manager"对象（方案 B） | 让 `AddPacketToInputStream` 注入的流像内部边一样经 mirror 扇出到多消费者，且完成计数只计图输入 | 直接给每个消费者注册 manager 并按名直连（方案 A）无法在"一喂多"下扇出；与 MediaPipe GraphInputStream 语义不符 |
| 输入侧从"流名唯一共享 manager"改为"per-edge manager" | 1→N 的每个消费者需要独立队列与独立调度回调 | 继续共享单队列会把第二个消费者折叠成无输入端口 → 被当 source 永转（本 bug 根因） |
