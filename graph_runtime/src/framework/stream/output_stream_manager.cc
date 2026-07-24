#include "src/framework/stream/output_stream_manager.h"

#include <algorithm>
#include <utility>

#include "src/framework/scheduler/input_stream_handler.h"

namespace graph::runtime {

OutputStreamManager::OutputStreamManager(std::string name)
    : spec_{std::move(name)} {}

const std::string& OutputStreamManager::Name() const { return spec_.name; }

void OutputStreamManager::AddMirror(InputStreamHandler* handler,
                                     CollectionItemId id) {
  mirrors_.push_back({handler, id});
}

void OutputStreamManager::PrepareForRun(ErrorCallback error_callback) {
  // Reset state for a new run
  next_timestamp_bound_ = Timestamp::Unset();
  closed_ = false;
}

Timestamp OutputStreamManager::ComputeOutputTimestampBound(
    const OutputStreamShard& shard, Timestamp input_timestamp) const {
  Timestamp new_bound = next_timestamp_bound_;

  if (spec_.offset_enabled) {
    Timestamp input_bound = input_timestamp.NextAllowedInStream();
    if (spec_.offset.Value() > 0) {
      auto diff = spec_.offset;
      input_bound = input_timestamp + diff;
    }
    if (input_bound > new_bound) {
      new_bound = input_bound;
    }
  }

  if (!shard.IsEmpty()) {
    Timestamp packet_bound =
        shard.LastAddedPacketTimestamp().NextAllowedInStream();
    if (packet_bound > new_bound) {
      new_bound = packet_bound;
    }
  }

  if (shard.updated_next_timestamp_bound_ > new_bound) {
    new_bound = shard.updated_next_timestamp_bound_;
  }

  return new_bound;
}

void OutputStreamManager::PropagateUpdatesToMirrors(
    Timestamp next_bound, OutputStreamShard* shard) {
  auto* packets = &shard->output_queue_;

  if (next_bound != Timestamp::Unset()) {
    next_timestamp_bound_ = next_bound;
    num_packets_added_ += packets->size();
  }

  bool set_bound = (next_bound != Timestamp::Unset()) &&
      (!packets->empty()
           ? shard->LastAddedPacketTimestamp().NextAllowedInStream() != next_bound
           : true);

  for (size_t idx = 0; idx < mirrors_.size(); ++idx) {
    auto& mirror = mirrors_[idx];
    if (!mirror.handler) continue;

    if (!packets->empty()) {
      if (idx == mirrors_.size() - 1) {
        // Last mirror: move packets (transfer ownership)
        bool notify = false;
        mirror.handler->MovePacketsToStream(mirror.id, packets, &notify);
      } else {
        // Other mirrors: copy packets
        bool notify = false;
        mirror.handler->AddPacketsToStream(mirror.id, *packets, &notify);
      }
    }
    if (set_bound) {
      mirror.handler->SetNextTimestampBound(mirror.id, next_bound);
    }
  }

  // packets should be empty after MovePackets, clear just in case
  packets->clear();
}

void OutputStreamManager::PropagateHeader() {
  if (spec_.header.IsEmpty()) return;
  for (auto& mirror : mirrors_) {
    if (mirror.handler) {
      mirror.handler->SetNextTimestampBound(mirror.id, Timestamp::PreStream());
    }
  }
}

void OutputStreamManager::LockIntroData() {
  spec_.locked_intro_data = true;
}

void OutputStreamManager::Close() {
  if (closed_) return;
  closed_ = true;
  next_timestamp_bound_ = Timestamp::Done();
  for (auto& mirror : mirrors_) {
    if (mirror.handler) {
      mirror.handler->SetNextTimestampBound(mirror.id, Timestamp::Done());
    }
  }
}

bool OutputStreamManager::IsClosed() const { return closed_; }

Timestamp OutputStreamManager::NextTimestampBound() const {
  return next_timestamp_bound_;
}

void OutputStreamManager::ResetShard(OutputStreamShard* shard) {
  shard->Reset(next_timestamp_bound_, closed_);
}

OutputStreamSpec* OutputStreamManager::Spec() { return &spec_; }

void OutputStreamManager::CleanupAfterRun() {
  next_timestamp_bound_ = Timestamp::Unset();
  closed_ = false;
  num_packets_added_ = 0;
  spec_.locked_intro_data = false;
  spec_.header = Packet();
}

}  // namespace graph::runtime
