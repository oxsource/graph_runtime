#ifndef GRAPH_RUNTIME_OUTPUT_STREAM_MANAGER_H_
#define GRAPH_RUNTIME_OUTPUT_STREAM_MANAGER_H_

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "src/stream/packet.h"
#include "src/stream/output_stream_shard.h"
#include "src/public/types.h"

namespace graph::runtime {

class InputStreamHandler;

struct Mirror {
  InputStreamHandler* handler;
  CollectionItemId id;
};

class OutputStreamManager {
 public:
  explicit OutputStreamManager(std::string name);

  const std::string& Name() const;

  void AddMirror(InputStreamHandler* handler, CollectionItemId id);
  void PrepareForRun(ErrorCallback error_callback);

  Timestamp ComputeOutputTimestampBound(
      const OutputStreamShard& shard, Timestamp input_timestamp) const;

  void PropagateUpdatesToMirrors(Timestamp next_bound,
                                  OutputStreamShard* shard);
  void PropagateHeader();
  void LockIntroData();

  void Close();
  bool IsClosed() const;
  void CleanupAfterRun();
  Timestamp NextTimestampBound() const;

  void ResetShard(OutputStreamShard* shard);
  bool OffsetEnabled() const { return spec_.offset_enabled; }
  TimestampDiff Offset() const { return spec_.offset; }

  OutputStreamSpec* Spec();

 private:
  OutputStreamSpec spec_;
  std::vector<Mirror> mirrors_;
  Timestamp next_timestamp_bound_{Timestamp::Unset()};
  bool closed_ = false;
  int64_t num_packets_added_ = 0;
};

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_OUTPUT_STREAM_MANAGER_H_
