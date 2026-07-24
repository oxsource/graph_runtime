#ifndef GRAPH_RUNTIME_GRAPH_RUNTIME_H_
#define GRAPH_RUNTIME_GRAPH_RUNTIME_H_

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "src/config/graph_config.h"
#include "src/public/include/graph_runtime/hook.h"
#include "src/public/types.h"
#include "src/public/side_packet.h"
#include "src/scheduler/scheduler.h"
#include "src/node/node.h"
#include "src/stream/input_stream_manager.h"

namespace graph::runtime {

class GraphRuntime {
 public:
  GraphRuntime();
  ~GraphRuntime();

  absl::Status Initialize(const GraphConfig& config);
  absl::Status Start();
  absl::Status WaitUntilDone();
  void Shutdown();

  absl::Status AddPacketToInputStream(const std::string& stream_name,
                                       Packet packet);
  absl::Status CloseInputStream(const std::string& stream_name);
  void SetOutputStreamCallback(const std::string& stream_name,
                                std::function<void(const Packet&)> callback);
  void ClearOutputStreamCallback(const std::string& stream_name);
  absl::Status SetInputSidePacket(const std::string& tag_name, Packet packet);
  void SetOutputSidePacketCallback(
      const std::string& name,
      std::function<void(const Packet&)> callback);

  // Register a hook by type. Replaces any previous hook of the same type.
  // Example: SetHook(hook::kTypeLog, myFn);
  void SetHook(int type, hook::HookFn fn);

 private:
  Node* FindNode(const std::string& name);

  friend class GraphBuilder;
  GraphConfig config_;
  std::unique_ptr<Scheduler> scheduler_;
  std::vector<std::unique_ptr<Node>> all_nodes_;
  std::map<std::string, InputStreamManager*> stream_managers_;
  std::set<std::string> graph_input_streams_set_;
  int num_open_input_streams_ = 0;
};

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_GRAPH_RUNTIME_H_
