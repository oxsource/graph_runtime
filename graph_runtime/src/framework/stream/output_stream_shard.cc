#include "src/stream/output_stream_shard.h"

#include <utility>

namespace graph::runtime {

OutputStreamShard::OutputStreamShard() {}

OutputStreamShard::OutputStreamShard(OutputStreamSpec* spec)
    : spec_(spec) {}

void OutputStreamShard::SetSpec(OutputStreamSpec* spec) { spec_ = spec; }

void OutputStreamShard::Reset(Timestamp next_timestamp_bound, bool close) {
  output_queue_.clear();
  next_timestamp_bound_ = next_timestamp_bound;
  updated_next_timestamp_bound_ = Timestamp::Unset();
  closed_ = close;
}

const std::string& OutputStreamShard::Name() const {
  static const std::string empty;
  return spec_ ? spec_->name : empty;
}

template <typename T>
void OutputStreamShard::AddPacketInternal(T&& packet) {
  if (closed_) return;
  if (packet.IsEmpty()) {
    SetNextTimestampBound(packet.timestamp().NextAllowedInStream());
    return;
  }
  next_timestamp_bound_ = packet.timestamp().NextAllowedInStream();
  updated_next_timestamp_bound_ = next_timestamp_bound_;
  output_queue_.push_back(std::forward<T>(packet));
}

void OutputStreamShard::AddPacket(const Packet& packet) {
  AddPacketInternal(packet);
}

void OutputStreamShard::AddPacket(Packet&& packet) {
  AddPacketInternal(std::move(packet));
}

void OutputStreamShard::SetNextTimestampBound(Timestamp bound) {
  next_timestamp_bound_ = bound;
  updated_next_timestamp_bound_ = bound;
}

void OutputStreamShard::Close() {
  closed_ = true;
  next_timestamp_bound_ = Timestamp::Done();
  updated_next_timestamp_bound_ = Timestamp::Done();
}

bool OutputStreamShard::IsClosed() const { return closed_; }

void OutputStreamShard::SetOffset(TimestampDiff offset) {
  if (spec_ && !spec_->locked_intro_data) {
    spec_->offset = offset;
    spec_->offset_enabled = true;
  }
}

bool OutputStreamShard::OffsetEnabled() const {
  return spec_ && spec_->offset_enabled;
}

TimestampDiff OutputStreamShard::Offset() const {
  return spec_ ? spec_->offset : TimestampDiff(0);
}

void OutputStreamShard::SetHeader(const Packet& packet) {
  if (spec_ && !spec_->locked_intro_data) {
    spec_->header = packet;
  }
}

const Packet& OutputStreamShard::Header() const {
  static const Packet empty;
  return spec_ ? spec_->header : empty;
}

Timestamp OutputStreamShard::LastAddedPacketTimestamp() const {
  if (output_queue_.empty()) return Timestamp::Unset();
  return output_queue_.back().timestamp();
}

}  // namespace graph::runtime
