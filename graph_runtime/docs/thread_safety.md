# Thread Safety Model

## Overview

The scheduler supports two execution paths:

| Path | Method | Thread Model | External Input |
|------|--------|-------------|----------------|
| **Sync** | `Schedule()` | Single-threaded (caller's thread) | ❌ Not supported |
| **Async** | `Start() + WaitUntilDone()` | Multi-threaded (ThreadPool) | ✅ `AddPacketToInputStream` |

The async path uses a thread pool executor that processes `SchedulerQueue` items
on worker threads while the application thread injects packets.

## Async Path Thread Architecture

```
Application Thread                   Executor Thread(s)
─────────────────────               ─────────────────────
Initialize(graph)                  
  │                                 
Start()                            
  ├─ HandleIdle()                   
  │                                 │  ThreadPool worker wakes
  │                                 ├─ RunNextTask()
  │                                 │   ├─ RunNode()
  │                                 │   │   ├─ PopQueueHead() [InputStreamManager]
  │                                 │   │   ├─ node->Process()
  │                                 │   │   ├─ PostProcess() [OutputStreamHandler]
  │                                 │   │   └─ re-schedule while input remains (drain)
  │                                 │   ├─ --num_pending_tasks_
  │                                 │   └─ UpdateIdleState() → HandleIdle()
  │                                 │       └─ cv_.notify_all()
  │                                 
AddPacketToInputStream("in", pkt)   
  ├─ AddPackets() [InputStreamManager]
  │   └─ arrival callback → q->AddNode(node) → SubmitToExecutor()
  └─ AddedPacketToInputStream()
       └─ HandleIdle()   // termination re-check only
  │                                 │  ThreadPool worker wakes
  │                                 ├─ RunNextTask() ...
  │                                 │   └─ RunNode(): pop → Process → PostProcess
  │                                 │       └─ re-schedule while input remains (drain)
  │                                 
CloseInputStream("in")              
  ├─ Close() [InputStreamManager]    
  │   └─ arrival callback → q->AddNode(node)   // final flush (kReadyForClose)
  ├─ IncClosedGraphInputStreams()   
  └─ AddedPacketToInputStream()
       └─ HandleIdle() → Quit() → STATE_TERMINATED
  
WaitUntilDone()                     
  └─ cv_.wait() wakes, returns     
```

## Thread-Safe Methods (callable from any thread)

| Class | Method | Mechanism |
|-------|--------|-----------|
| `Scheduler` | `Shutdown()` | Atomic `stopping_` flag |
| `Scheduler` | `state()` | Read-only, non-atomic but stable |
| `Scheduler` | `WaitUntilDone()` / `WaitForIdle()` | Condition variable guarded by `mutex_` |
| `SchedulerQueue` | `AddNode()` / `AddNodeForOpen()` | Lock-free (single consumer) |
| `ThreadPoolExecutor` | `Schedule()` | Mutex + condition variable |
| `InputStreamManager` | `AddPackets()` | Called from application thread only |
| `InputStreamManager` | Arrival callback → `SchedulerQueue::AddNode()` | Fires on producer threads (app or executor); `AddNode` is internally locked |
| `NodeFactoryRegistry` | All public methods | Static mutex |
| `OutputStreamHandler` | `SetOutputStreamCallback()` | Called from application thread |

## Single-Threaded Assumptions (caller must synchronize)

| Class | Method | Rationale |
|-------|--------|-----------|
| `Scheduler::HandleIdle()` | All | Re-entrance guarded by `std::atomic<int> handling_idle_`. Called from both app and executor threads. |
| `SchedulerQueue::RunNode()` | `PopQueueHead()` | Single consumer — only one executor thread processes a given queue at a time. |
| `GraphRuntime::AddPacketToInputStream()` | `AddPackets()` | Called from application thread while scheduler runs on executor threads. |
| `GraphRuntime::SetOutputStreamCallback()` | Register callback | Set before `Start()`; storage is application-thread-only. |

## Event-Driven Draining (MediaPipe parity)

Buffered packets are drained by input-state events, not by polling in
`HandleIdle`. Three triggers keep a node scheduled while its input holds work:

1. **Stream arrival** — `GraphRuntime::Initialize` wires a per-stream arrival
   callback on every `InputStreamManager`. Whenever a stream transitions from
   empty to non-empty (`AddPackets`/`MovePackets`), the owning node is added to
   its `SchedulerQueue`. No `IsRunning()` guard is used: `AddNode` queues the
   item while paused and the executor submission happens on `Resume`.
2. **Post-process re-scheduling** — after each `Process` invocation,
   `SchedulerQueue::RunNode` re-adds the node while any input packet remains
   buffered (`HasPendingInput`). This mirrors MediaPipe's
   `EndScheduling → SchedulingLoop` pattern and drains a batch (e.g. packets
   emitted by an encoder during Flush) one packet per invocation.
3. **Stream close** — `InputStreamManager::Close()` notifies the owning node
   so a final flush runs even when the queue is already empty (MediaPipe
   `kReadyForClose` equivalent; the done signal is delivered with the last
   packet otherwise).

Consequence: when every scheduler queue is idle, every input stream is drained,
so `Scheduler::HandleIdle` can terminate on `has_error_` or
`no_more_sources && !inputs_remaining` alone (matching MediaPipe's
`active_sources_.empty() && sources_queue_.empty() && graph_input_streams_closed_`)
without scanning input queues.

## HandleIdle Re-entrance

`HandleIdle()` can be called from two threads concurrently (main thread via
`AddedPacketToInputStream`, executor thread via idle callback). It uses a
`std::atomic<int> handling_idle_` guard to allow only the first caller to
execute; concurrent callers return immediately. This is safe because the
winning caller runs the full idle-detection loop.

## Key Data Races Prevented

| Race | Prevention |
|------|-----------|
| `num_pending_tasks_` increment (SubmitToExecutor) vs decrement (RunNextTask) | Atomic-like: executor thread is the sole consumer; app thread never modifies. |
| `queue_` push (AddNode, app thread) vs pop (RunNextTask, executor thread) | `std::priority_queue` single-producer single-consumer — no mutex needed. See _Intel 64 and IA-32 Architectures_ §8.2 for memory ordering. |
| `handling_idle_` concurrent increment | `std::atomic<int>` with sequential consistency. |
| `state_` read (WaitUntilDone) vs write (HandleIdle) | `cv_.wait()` with mutex — state is read under lock in the predicate lambda, written under lock in `HandleIdle`. |

## `std::atomic<int>` in Scheduler

One atomic variable is used to avoid a mutex that would be destroyed before
pending executor tasks complete:

| Variable | Purpose |
|----------|---------|
| `handling_idle_` | Guards re-entrance into `HandleIdle`. `std::atomic` chosen over a bare `int` because the function is called from both the application and executor threads without a shared mutex. |
