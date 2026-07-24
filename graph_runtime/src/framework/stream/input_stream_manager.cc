#include "src/framework/stream/input_stream_manager.h"

#include <algorithm>

namespace graph::runtime {

InputStreamManager::InputStreamManager(std::string name, int max_queue_size)
    : name_(std::move(name)), max_queue_size_(max_queue_size) {}

const std::string& InputStreamManager::name() const { return name_; }

static bool CheckTimestamp(const Packet& p) {
  return p.timestamp().IsAllowedInStream() || p.timestamp().IsSpecialValue();
}

absl::Status InputStreamManager::AddPackets(const std::list<Packet>& packets,
                                             bool* notify) {
  *notify = false;
  bool was_queue_full = IsFull();

  if (closed_) {
    return absl::FailedPreconditionError("Stream is closed");
  }

  bool queue_became_non_empty = queue_.empty() && !packets.empty();

  for (const auto& packet : packets) {
    if (enable_timestamps_ && !CheckTimestamp(packet)) {
      return absl::InvalidArgumentError("Invalid timestamp");
    }
    next_timestamp_bound_ = packet.timestamp().NextAllowedInStream();
    ++num_packets_added_;
    queue_.push_back(packet);
  }

  if (!was_queue_full && IsFull() && becomes_full_callback_) {
    becomes_full_callback_(this, &last_reported_stream_full_);
  }

  *notify = queue_became_non_empty;
  if (*notify && arrival_callback_) {
    arrival_callback_();
  }
  return absl::OkStatus();
}

absl::Status InputStreamManager::MovePackets(std::list<Packet>* packets,
                                              bool* notify) {
  *notify = false;
  bool was_queue_full = IsFull();

  if (closed_) {
    return absl::FailedPreconditionError("Stream is closed");
  }

  bool queue_became_non_empty = queue_.empty() && !packets->empty();

  for (auto& packet : *packets) {
    if (enable_timestamps_ && !CheckTimestamp(packet)) {
      return absl::InvalidArgumentError("Invalid timestamp");
    }
    next_timestamp_bound_ = packet.timestamp().NextAllowedInStream();
    ++num_packets_added_;
    queue_.push_back(std::move(packet));
  }
  packets->clear();

  if (!was_queue_full && IsFull() && becomes_full_callback_) {
    becomes_full_callback_(this, &last_reported_stream_full_);
  }

  *notify = queue_became_non_empty;
  if (*notify && arrival_callback_) {
    arrival_callback_();
  }
  return absl::OkStatus();
}

void InputStreamManager::SetNextTimestampBound(Timestamp bound) {
  if (bound > next_timestamp_bound_) {
    next_timestamp_bound_ = bound;
    if (arrival_callback_ && queue_.empty()) {
      arrival_callback_();
    }
  }
}

void InputStreamManager::Close() {
  closed_ = true;
  next_timestamp_bound_ = Timestamp::Done();
}

Packet InputStreamManager::PopPacketAtTimestamp(
    Timestamp timestamp, int* num_packets_dropped, bool* stream_is_done) {
  *num_packets_dropped = -1;
  bool was_queue_full = IsFull();
  Packet packet;
  Timestamp current_timestamp = Timestamp::Unset();

  if (!queue_.empty() && queue_.front().timestamp() <= timestamp) {
    packet = std::move(queue_.front());
    queue_.pop_front();
    current_timestamp = packet.timestamp();
    *num_packets_dropped = 0;
  }

  if (current_timestamp != timestamp) {
    Timestamp bound = MinTimestampOrBound(nullptr);
    packet = Packet().At(bound.PreviousAllowedInStream());
    ++(*num_packets_dropped);
  }

  last_select_timestamp_ = timestamp;

  if (next_timestamp_bound_ <= timestamp) {
    next_timestamp_bound_ = timestamp.NextAllowedInStream();
  }

  bool queue_became_non_full = (was_queue_full && !IsFull());
  if (queue_became_non_full && becomes_not_full_callback_) {
    becomes_not_full_callback_(this, &last_reported_stream_full_);
  }

  *stream_is_done = IsDone();
  return packet;
}

Packet InputStreamManager::PopQueueHead(bool* stream_is_done) {
  if (queue_.empty()) {
    *stream_is_done = IsDone();
    return Packet();
  }
  Packet p = std::move(queue_.front());
  queue_.pop_front();
  *stream_is_done = IsDone();
  return p;
}

Packet InputStreamManager::QueueHead() const {
  if (queue_.empty()) return Packet();
  return queue_.front();
}

bool InputStreamManager::IsEmpty() const { return queue_.empty(); }

bool InputStreamManager::IsFull() const {
  return max_queue_size_ != -1 &&
         static_cast<int>(queue_.size()) >= max_queue_size_;
}

bool InputStreamManager::IsDone() const {
  return queue_.empty() && next_timestamp_bound_ == Timestamp::Done();
}

int InputStreamManager::QueueSize() const {
  return static_cast<int>(queue_.size());
}

int InputStreamManager::MaxQueueSize() const { return max_queue_size_; }

int64_t InputStreamManager::NumPacketsAdded() const {
  return num_packets_added_;
}

Timestamp InputStreamManager::MinTimestampOrBound(bool* is_empty) const {
  if (!queue_.empty()) {
    if (is_empty) *is_empty = false;
    return queue_.front().timestamp();
  }
  if (is_empty) *is_empty = true;
  return next_timestamp_bound_;
}

void InputStreamManager::SetMaxQueueSize(int max_queue_size) {
  max_queue_size_ = max_queue_size;
}

void InputStreamManager::SetQueueSizeCallbacks(
    QueueSizeCallback full_cb, QueueSizeCallback not_full_cb) {
  becomes_full_callback_ = std::move(full_cb);
  becomes_not_full_callback_ = std::move(not_full_cb);
}

void InputStreamManager::SetArrivalCallback(PacketArrivalCallback cb) {
  arrival_callback_ = std::move(cb);
}

void InputStreamManager::PrepareForRun() {
  queue_.clear();
  num_packets_added_ = 0;
  next_timestamp_bound_ = Timestamp::PreStream();
  last_select_timestamp_ = Timestamp::Unstarted();
  closed_ = false;
  last_reported_stream_full_ = false;
}

void InputStreamManager::CleanupAfterRun() {
  queue_.clear();
}

}  // namespace graph::runtime
