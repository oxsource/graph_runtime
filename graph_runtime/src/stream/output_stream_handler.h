#ifndef GRAPH_RUNTIME_OUTPUT_STREAM_HANDLER_H_
#define GRAPH_RUNTIME_OUTPUT_STREAM_HANDLER_H_

#include <memory>
#include <set>
#include <vector>

#include "absl/status/status.h"
#include "graph_runtime/src/stream/output_stream_manager.h"
#include "graph_runtime/src/stream/output_stream_shard.h"
#include "graph_runtime/src/node/graph_context.h"
#include "graph_runtime/src/public/types.h"

namespace graph::runtime {

class OutputStreamHandler {
 public:
  explicit OutputStreamHandler(std::vector<OutputStreamManager*> managers);
  virtual ~OutputStreamHandler() = default;

  void Open(OutputStreamShardSet* shards);
  void PrepareOutputs(Timestamp input_timestamp,
                      OutputStreamShardSet* shards);
  void PostProcess(Timestamp input_timestamp,
                   OutputStreamShardSet* shards);
  void Close(OutputStreamShardSet* shards);

  void UpdateTaskTimestampBound(Timestamp bound);
  void TryPropagateTimestampBound(Timestamp input_bound);

 protected:
  void PropagateOutputPackets(Timestamp input_timestamp,
                               OutputStreamShardSet* shards);

  std::vector<OutputStreamManager*> managers_;
  std::set<Timestamp> completed_input_timestamps_;
  Timestamp task_timestamp_bound_;
};

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_OUTPUT_STREAM_HANDLER_H_
