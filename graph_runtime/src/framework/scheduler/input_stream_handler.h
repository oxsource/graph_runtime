#ifndef GRAPH_RUNTIME_INPUT_STREAM_HANDLER_H_
#define GRAPH_RUNTIME_INPUT_STREAM_HANDLER_H_

#include <functional>
#include <list>
#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "src/framework/stream/packet.h"
#include "src/framework/stream/input_stream_manager.h"
#include "src/framework/node/node.h"
#include "src/framework/node/graph_context.h"
#include "src/framework/public/types.h"

namespace graph::runtime {

class SyncSet {
 public:
  explicit SyncSet(std::vector<InputStreamManager*> managers);

  enum Readiness { kNotReady, kReadyForProcess, kReadyForClose };

  Readiness GetReadiness(Timestamp* min_stream_timestamp);
  void FillInputSet(Timestamp timestamp, GraphContext& context);
  void FillInputBounds(GraphContext& context);

 private:
  std::vector<InputStreamManager*> managers_;
};

class InputStreamHandler {
 public:
  using ScheduleCallback = std::function<void(Node& node)>;

  enum Readiness { kNotReady, kReadyForProcess, kReadyForClose };

  virtual ~InputStreamHandler() = default;

  virtual void SetInputStreamManagers(
      const std::vector<InputStreamManager*>& managers) = 0;
  virtual void SetScheduleCallback(ScheduleCallback cb) = 0;

  virtual bool ScheduleInvocations(int max_allowance,
                                   Timestamp* input_bound,
                                   Node& node,
                                   GraphContext& context) = 0;
  virtual Readiness GetNodeReadiness(Timestamp* min_stream_timestamp) = 0;
  virtual void FillInputSet(Timestamp timestamp, GraphContext& context) = 0;

  virtual void NotifyPacketArrival() = 0;
  virtual void SetNextTimestampBound(CollectionItemId id,
                                      Timestamp bound) = 0;

  // Add/Move packets to the InputStreamManager identified by id.
  virtual absl::Status AddPacketsToStream(CollectionItemId id,
                                           const std::list<Packet>& packets,
                                           bool* notify) = 0;
  virtual absl::Status MovePacketsToStream(CollectionItemId id,
                                            std::list<Packet>* packets,
                                            bool* notify) = 0;

  virtual void Close() = 0;
};

class DefaultInputStreamHandler : public InputStreamHandler {
 public:
  DefaultInputStreamHandler();

  void SetInputStreamManagers(
      const std::vector<InputStreamManager*>& managers) override;
  void SetScheduleCallback(ScheduleCallback cb) override;

  bool ScheduleInvocations(int max_allowance,
                           Timestamp* input_bound,
                           Node& node,
                           GraphContext& context) override;
  Readiness GetNodeReadiness(Timestamp* min_stream_timestamp) override;
  void FillInputSet(Timestamp timestamp, GraphContext& context) override;

  void NotifyPacketArrival() override;
  void SetNextTimestampBound(CollectionItemId id, Timestamp bound) override;

  absl::Status AddPacketsToStream(CollectionItemId id,
                                   const std::list<Packet>& packets,
                                   bool* notify) override;
  absl::Status MovePacketsToStream(CollectionItemId id,
                                    std::list<Packet>* packets,
                                    bool* notify) override;

  void Close() override;

 private:
  std::unique_ptr<SyncSet> sync_set_;
  ScheduleCallback schedule_callback_;
  std::vector<InputStreamManager*> managers_;
  bool notified_ = false;
};

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_INPUT_STREAM_HANDLER_H_
