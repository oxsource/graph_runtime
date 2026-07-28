#ifndef GRAPH_RUNTIME_GRAPH_RUNTIME_H_
#define GRAPH_RUNTIME_GRAPH_RUNTIME_H_

#include <functional>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "absl/status/status.h"
#include "src/framework/config/graph_config.h"
#include "src/framework/public/include/graph_runtime/hook.h"
#include "src/framework/public/types.h"
#include "src/framework/public/side_packet.h"
#include "src/framework/scheduler/scheduler.h"
#include "src/framework/node/node.h"
#include "src/framework/profiler/graph_profiler.h"
#include "graph_runtime/profiler.h"
#include "src/framework/stream/input_stream_manager.h"
#include "src/framework/stream/output_stream_manager.h"
#include "src/framework/stream/output_stream_handler.h"

namespace graph::runtime {

class GraphRuntime {
 public:
  GraphRuntime();
  ~GraphRuntime();

  /// @defgroup ExecutionModes Execution Modes
  ///
  /// The runtime offers two mutually exclusive execution paths:
  ///
  /// **Async mode** (`Start` + `WaitUntilDone`):
  ///   - Graph runs on a thread pool (non-blocking).
  ///   - Supports `AddPacketToInputStream`, `CloseInputStream`,
  ///     output callbacks, side packets.
  ///   - Call `Start()` to begin execution, then interact with the
  ///     running graph, then `WaitUntilDone()` to block until termination.
  ///   - Use `Shutdown()` to force termination early.
  ///   - MediaPipe-compatible pattern (see `StartRun` + `WaitUntilDone`).
  ///
  /// **Sync mode** (`Schedule`):
  ///   - Graph runs entirely on the caller's thread (blocking).
  ///   - Does NOT support `AddPacketToInputStream` or callbacks.
  ///   - For batch/static graphs driven by source nodes only.
  ///   - Returns only after all nodes finish.
  ///   - No MediaPipe counterpart; convenience for simple batch use.
  ///
  /// @{

  /// Initialize the graph from a config. Must be called before any
  /// execution method.
  absl::Status Initialize(const GraphConfig& config);

  /// @name Async execution path

  /// Start asynchronous execution on a thread pool. Returns immediately.
  /// After Start(), the graph runs on worker threads. Inject packets via
  /// AddPacketToInputStream and signal completion via CloseInputStream.
  /// WaitUntilDone() blocks until the graph reaches kTerminated.
  absl::Status Start();

  /// Block until the graph reaches kTerminated (async path only).
  absl::Status WaitUntilDone();

  /// Block until all queues are idle (async path only).
  absl::Status WaitForIdle();

  /// Returns true if the graph has terminated (async path).
  bool HasGraphFinished() const;

  /// Returns the current scheduler state (async path).
  SchedulerState GetGraphState() const;

  /// Force-terminate the graph (async path only).
  void Shutdown();

  /// Cancel graph execution. Sets state to kCancelling and records
  /// an error. Queues are drained and WaitUntilDone returns.
  void Cancel();

  /// Pause graph execution (async path). Stops all queues. Resume() to
  /// continue. Has no effect if the graph is not running.
  absl::Status Pause();

  /// Resume a paused graph (async path). Restarts queues and triggers
  /// HandleIdle. Has no effect if the graph is not paused.
  absl::Status Resume();

  /// @name Sync execution path

  /// Execute the graph synchronously on the calling thread.
  /// Only source nodes are processed; AddPacketToInputStream is NOT
  /// supported. Returns when all nodes finish.
  /// For interactive/streaming use cases, use Start() instead.
  absl::Status Schedule();

  /// @}

  /// Add a packet to the named input stream (async path only).
  absl::Status AddPacketToInputStream(const std::string& stream_name,
                                        Packet packet);

  /// Add a packet to an indexed input stream (e.g., tag="VIDEO", index=0
  /// maps to stream name "VIDEO:0").
  absl::Status AddPacketToInputStream(const std::string& tag, int index,
                                        Packet packet);
  absl::Status CloseInputStream(const std::string& stream_name);
  void SetOutputStreamCallback(const std::string& stream_name,
                                 std::function<void(const Packet&)> callback);
  void ClearOutputStreamCallback(const std::string& stream_name);
  absl::Status SetInputSidePacket(const std::string& tag_name, Packet packet);
  void SetOutputSidePacketCallback(
      const std::string& name,
      std::function<void(const Packet&)> callback);

  // ── Profiler ──

  void SetProfilerConfig(const ProfilerConfig& config);
  ProfilingContext* profiler();
  const ProfilingContext* profiler() const;
  std::vector<NodeProfile> GetNodeProfiles() const;

  // Register a hook by type. Replaces any previous hook of the same type.
  // Example: SetHook(hook::kTypeLog, myFn);
  void SetHook(int type, hook::HookFn fn);

  // --- Backpressure ---

  enum GraphInputStreamAddMode { ADD_IF_NOT_FULL, WAIT_TILL_NOT_FULL };

  // Callbacks from InputStreamManager for queue full/not-full events.
  void OnInputStreamFull(InputStreamManager* mgr);
  void OnInputStreamNotFull(InputStreamManager* mgr);

  // Unthrottle blocked sources when the graph is deadlocked.
  void UnthrottleSources();

  // Check if a node is throttled (has any full input stream).
  bool IsNodeThrottled(Node* node) const;

 private:
  Node* FindNode(const std::string& name);

  friend class GraphBuilder;
  GraphConfig config_;
  std::unique_ptr<Scheduler> scheduler_;
  std::vector<std::unique_ptr<Node>> all_nodes_;
  std::list<std::unique_ptr<InputStreamManager>> owned_stream_managers_;
  std::map<std::string, InputStreamManager*> stream_managers_;
  std::set<std::string> graph_input_streams_set_;
  int num_open_input_streams_ = 0;

  // Throttle tracking: full input streams per node.
  std::map<Node*, std::set<InputStreamManager*>> full_input_streams_;
  std::set<std::string> closed_streams_;

  // Owned output stream infrastructure (list for pointer stability).
  std::list<std::unique_ptr<OutputStreamManager>> owned_output_stream_managers_;
  std::list<std::unique_ptr<OutputStreamHandler>> owned_output_stream_handlers_;

  // Owned input stream handlers.
  std::list<std::unique_ptr<InputStreamHandler>> owned_input_stream_handlers_;

  // Output stream callback storage.
  std::map<std::string, std::function<void(const Packet&)>> output_stream_callbacks_;

  // Input side packet storage (tag_name → Packet).
  std::map<std::string, Packet> side_packet_map_;

  // Output side packet callback storage (name → callback).
  std::map<std::string, std::function<void(const Packet&)>> output_side_packet_callbacks_;

  ProfilerConfig profiler_config_override_;
  std::unique_ptr<ProfilingContext> profiler_;
};

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_GRAPH_RUNTIME_H_
