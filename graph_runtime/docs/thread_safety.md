# Thread Safety Model

## Overview

Phase 1 is single-threaded. The following model documents the intended thread safety
guarantees for Phase 2 multi-threaded execution.

## Thread-Safe Methods (callable from any thread)

| Class | Method | Notes |
|-------|--------|-------|
| `Executor` | `Schedule()` | Must be lock-free or mutex-protected |
| `Executor` | `AddTask()` | Delegates to Schedule() |
| `Scheduler` | `Shutdown()` | Uses atomic state_ flag |
| `Scheduler` | `state()` | Atomic read |
| `ThreadPoolExecutor` | `Schedule()` | Mutex + condition variable |
| `NodeFactoryRegistry` | `Register()` | Mutex-protected |
| `NodeFactoryRegistry` | `CreateByName()` | Mutex-protected |
| `NodeFactoryRegistry` | `IsRegistered()` | Mutex-protected |

## Single-Threaded Methods (caller must synchronize)

| Class | Method | Notes |
|-------|--------|-------|
| `Node` | `Open()` / `Process()` / `Close()` | Called sequentially by Scheduler |
| `InputStreamManager` | `AddPackets()` / `PopPacketAtTimestamp()` | Owned by one SchedulerQueue |
| `OutputStreamManager` | `PropagateUpdatesToMirrors()` | Called from Scheduler task runner |
| `SchedulerQueue` | `AddNode()` / `RunNextTask()` | Single queue, single consumer |
| `GraphContext` | All methods | Per-invocation, ephemeral |

## Phase 2 Additions

- `InputStreamManager`: Add mutex for concurrent AddPackets() and PopPacketAtTimestamp()
- `SchedulerQueue`: Add mutex for priority_queue access
- `Scheduler`: Make non_idle_queue_count_ and state_ atomic
- `GraphContextManager`: Mutex for active_contexts_ map
