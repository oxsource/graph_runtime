#include "src/framework/stream/output_stream_handler.h"

#include "src/framework/config/stream_name.h"

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
  // Address shards by port name (the key nodes use via ctx.Outputs().Get()),
  // creating entries on demand. Indexing an empty shard set would dereference
  // the map's end() iterator, so always resolve by port name.
  for (auto* mgr : managers_) {
    auto& shard = shards->Get(PortName(mgr->Name()));
    mgr->ResetShard(&shard);
  }
}

void OutputStreamHandler::PostProcess(
    Timestamp input_timestamp, OutputStreamShardSet* shards) {
  PropagateOutputPackets(input_timestamp, shards);
}

void OutputStreamHandler::Close(OutputStreamShardSet* shards) {
  for (auto* mgr : managers_) {
    if (shards) {
      auto& shard = shards->Get(PortName(mgr->Name()));
      mgr->PropagateUpdatesToMirrors(Timestamp::Done(), &shard);
    }
    mgr->Close();
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

void OutputStreamHandler::SetOutputStreamCallback(
    const std::string& stream_name,
    std::function<void(const Packet&)> callback) {
  output_stream_callbacks_[stream_name] = std::move(callback);
}

void OutputStreamHandler::ClearOutputStreamCallback(
    const std::string& stream_name) {
  output_stream_callbacks_.erase(stream_name);
}

void OutputStreamHandler::PropagateOutputPackets(
    Timestamp input_timestamp, OutputStreamShardSet* shards) {
  for (auto* mgr : managers_) {
    if (mgr->IsClosed()) continue;

    // Resolve by port name so packets written via ctx.Outputs().Get(port)
    // (or Get(tag, index) whose key equals the port name) are propagated.
    auto& shard = shards->Get(PortName(mgr->Name()));
    Timestamp output_bound =
        mgr->ComputeOutputTimestampBound(shard, input_timestamp);

    // Fire registered callbacks before PropagateUpdatesToMirrors clears
    // the packet queue, so callbacks can access the output packets.
    auto cb_it = output_stream_callbacks_.find(mgr->Name());
    if (cb_it != output_stream_callbacks_.end()) {
      for (const auto& packet : shard.OutputQueue()) {
        cb_it->second(packet);
      }
    }

    mgr->PropagateUpdatesToMirrors(output_bound, &shard);

    if (shard.IsClosed()) {
      mgr->Close();
    }
  }
}

}  // namespace graph::runtime
