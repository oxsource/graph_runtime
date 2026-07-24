#ifndef GRAPH_RUNTIME_GRAPH_RUNTIME_H_
#define GRAPH_RUNTIME_GRAPH_RUNTIME_H_

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "src/config/graph_config.h"
#include "src/public/types.h"
#include "src/public/side_packet.h"
#include "src/scheduler/scheduler.h"
#include "src/node/node.h"

namespace graph::runtime {

enum HookType : int {
  kHookTypeSentinel = 0,
  kHookTypeLogIntercept = 1,
};

struct GraphHook {
  int type;
  bool (*hook_fn)(const void* data, int flag);
};

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

  void SetGlobalHook(const GraphHook* table);
  const GraphHook* GetGlobalHook(int type) const;

 private:
  friend class GraphBuilder;
  GraphConfig config_;
  std::unique_ptr<Scheduler> scheduler_;
  std::vector<std::unique_ptr<Node>> all_nodes_;
};

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_GRAPH_RUNTIME_H_
