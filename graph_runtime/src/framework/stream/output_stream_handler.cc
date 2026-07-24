#include "src/stream/output_stream_handler.h"

namespace graph::runtime {

OutputStreamHandler::OutputStreamHandler(
    std::vector<OutputStreamManager*> managers)
    : managers_(std::move(managers)) {}

void OutputStreamHandler::Open(OutputStreamShardSet* shards) {
  PropagateOutputPackets(Timestamp::Unstarted(), shards);
  for (auto* mgr : managers_) {
    mgr->PropagateHeader();
    mgr->LockIntroData();
  }
}

void OutputStreamHandler::PrepareOutputs(
    Timestamp input_timestamp, OutputStreamShardSet* shards) {
  for (size_t i = 0; i < managers_.size(); ++i) {
    auto& shard = shards->Index(static_cast<int>(i));
    managers_[i]->ResetShard(&shard);
  }
}

void OutputStreamHandler::PostProcess(
    Timestamp input_timestamp, OutputStreamShardSet* shards) {
  PropagateOutputPackets(input_timestamp, shards);
}

void OutputStreamHandler::Close(OutputStreamShardSet* shards) {
  for (size_t i = 0; i < managers_.size(); ++i) {
    if (shards) {
      auto& shard = shards->Index(static_cast<int>(i));
      managers_[i]->PropagateUpdatesToMirrors(Timestamp::Done(), &shard);
    }
    managers_[i]->Close();
  }
}

void OutputStreamHandler::UpdateTaskTimestampBound(Timestamp bound) {
  task_timestamp_bound_ = bound;
  TryPropagateTimestampBound(bound);
}

void OutputStreamHandler::TryPropagateTimestampBound(
    Timestamp input_bound) {
  if (!input_bound.IsRangeValue()) return;

  OutputStreamShard empty_shard;
  for (auto* mgr : managers_) {
    if (mgr->OffsetEnabled() && !mgr->IsClosed() &&
        input_bound + mgr->Offset() > mgr->NextTimestampBound()) {
      mgr->PropagateUpdatesToMirrors(
          input_bound + mgr->Offset(), &empty_shard);
    }
  }
}

void OutputStreamHandler::PropagateOutputPackets(
    Timestamp input_timestamp, OutputStreamShardSet* shards) {
  for (size_t i = 0; i < managers_.size(); ++i) {
    auto* mgr = managers_[i];
    if (mgr->IsClosed()) continue;

    auto& shard = shards->Index(static_cast<int>(i));
    Timestamp output_bound =
        mgr->ComputeOutputTimestampBound(shard, input_timestamp);
    mgr->PropagateUpdatesToMirrors(output_bound, &shard);

    if (shard.IsClosed()) {
      mgr->Close();
    }
  }
}

}  // namespace graph::runtime
