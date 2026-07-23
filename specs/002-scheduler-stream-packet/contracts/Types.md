# Contract: Types

**File**: `graph_runtime/src/public/types.h`

```cpp
namespace graph::runtime {

// --- CollectionItemId ---
// Lightweight handle for indexing into a Collection<T>.
// Used by InputStreamHandler, OutputStreamHandler, NodeContract, etc.
using CollectionItemId = int;

// --- ErrorCallback ---
// Invoked when a Node returns a non-OK, non-Stop status.
// The callback sets HasError() and initiates the kCancelling transition.
using ErrorCallback = std::function<void(absl::Status error)>;

// --- Stop Status ---
// MediaPipe uses tool::StatusStop() to signal graceful pipeline termination.
// In our runtime, a Node returning IsStopStatus() = true triggers the
// stopping_ flag and closes all active sources.
inline bool IsStopStatus(const absl::Status& status) {
  // Phase 1: use a specific status code.
  // Phase 2: may migrate to a custom Status type.
  return status.code() == absl::StatusCode::kUnavailable;
}

// --- StatusStop helper ---
inline absl::Status StatusStop() {
  return absl::UnavailableError("Stop");
}

}  // namespace graph::runtime
```

**Semantics**:
- `CollectionItemId` is a lightweight index handle for TAG-indexed port collections. Simplified to `int` for Phase 1.
- `ErrorCallback` is registered by GraphRuntime during Scheduler creation. Invoked when any Node::Process() returns a non-OK, non-Stop status.
- `IsStopStatus(status)` returns true if the status signals graceful pipeline termination. When detected by the Scheduler, it sets `stopping_ = true` and closes all active sources.
- `StatusStop()` creates a stop-status packet that Nodes can return from Process() to signal "no more data" (primarily used by sources).
