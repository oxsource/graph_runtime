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
  bool was_queue_full = false;
  bool queue_became_non_empty = false;
  PacketArrivalCallback arrival;
  QueueSizeCallback full_cb;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    *notify = false;
    was_queue_full =
        max_queue_size_ != -1 &&
        static_cast<int>(queue_.size()) >= max_queue_size_;

    if (closed_) {
      return absl::FailedPreconditionError("Stream is closed");
    }

    queue_became_non_empty = queue_.empty() && !packets.empty();

    for (const auto& packet : packets) {
      if (enable_timestamps_ && !CheckTimestamp(packet)) {
        return absl::InvalidArgumentError("Invalid timestamp");
      }
      next_timestamp_bound_ = packet.timestamp().NextAllowedInStream();
      ++num_packets_added_;
      queue_.push_back(packet);
    }

    if (!was_queue_full &&
        max_queue_size_ != -1 &&
        static_cast<int>(queue_.size()) >= max_queue_size_) {
      full_cb = becomes_full_callback_;
    }
    arrival = arrival_callback_;
    *notify = queue_became_non_empty;
  }

  if (full_cb) full_cb(this, &last_reported_stream_full_);
  if (*notify && arrival) arrival();
  return absl::OkStatus();
}

absl::Status InputStreamManager::MovePackets(std::list<Packet>* packets,
                                              bool* notify) {
  bool was_queue_full = false;
  bool queue_became_non_empty = false;
  PacketArrivalCallback arrival;
  QueueSizeCallback full_cb;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    *notify = false;
    was_queue_full =
        max_queue_size_ != -1 &&
        static_cast<int>(queue_.size()) >= max_queue_size_;

    if (closed_) {
      return absl::FailedPreconditionError("Stream is closed");
    }

    queue_became_non_empty = queue_.empty() && !packets->empty();

    for (auto& packet : *packets) {
      if (enable_timestamps_ && !CheckTimestamp(packet)) {
        return absl::InvalidArgumentError("Invalid timestamp");
      }
      next_timestamp_bound_ = packet.timestamp().NextAllowedInStream();
      ++num_packets_added_;
      queue_.push_back(std::move(packet));
    }
    packets->clear();

    if (!was_queue_full &&
        max_queue_size_ != -1 &&
        static_cast<int>(queue_.size()) >= max_queue_size_) {
      full_cb = becomes_full_callback_;
    }
    arrival = arrival_callback_;
    *notify = queue_became_non_empty;
  }

  if (full_cb) full_cb(this, &last_reported_stream_full_);
  if (*notify && arrival) arrival();
  return absl::OkStatus();
}

void InputStreamManager::SetNextTimestampBound(Timestamp bound) {
  PacketArrivalCallback arrival;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (bound > next_timestamp_bound_) {
      next_timestamp_bound_ = bound;
      // Always wake the owning node on a bound advance (including Timestamp::
      // Done from a stopping upstream source). The consumer must run one more
      // Process to observe the done state and finalize (e.g. an encoder Flush
      // draining codec-buffered frames); without this, a source that stops
      // while the consumer is momentarily busy leaves the done signal lost.
      arrival = arrival_callback_;
    }
  }
  if (arrival) arrival();
}

void InputStreamManager::Close() {
  PacketArrivalCallback arrival;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (closed_) return;
    closed_ = true;
    next_timestamp_bound_ = Timestamp::Done();
    // Notify the owning node so it can run its final flush (the MediaPipe
    // kReadyForClose equivalent) even when the queue is already empty.
    arrival = arrival_callback_;
  }
  if (arrival) arrival();
}

Packet InputStreamManager::PopPacketAtTimestamp(
    Timestamp timestamp, int* num_packets_dropped, bool* stream_is_done) {
  Packet packet;
  QueueSizeCallback not_full_cb;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    *num_packets_dropped = -1;
    const bool was_queue_full =
        max_queue_size_ != -1 &&
        static_cast<int>(queue_.size()) >= max_queue_size_;
    Timestamp current_timestamp = Timestamp::Unset();

    if (!queue_.empty() && queue_.front().timestamp() <= timestamp) {
      packet = std::move(queue_.front());
      queue_.pop_front();
      current_timestamp = packet.timestamp();
      *num_packets_dropped = 0;
    }

    if (current_timestamp != timestamp) {
      Timestamp bound =
          !queue_.empty() ? queue_.front().timestamp() : next_timestamp_bound_;
      packet = Packet().At(bound.PreviousAllowedInStream());
      ++(*num_packets_dropped);
    }

    last_select_timestamp_ = timestamp;

    if (next_timestamp_bound_ <= timestamp) {
      next_timestamp_bound_ = timestamp.NextAllowedInStream();
    }

    const bool queue_became_non_full =
        (was_queue_full && !(max_queue_size_ != -1 &&
                             static_cast<int>(queue_.size()) >= max_queue_size_));
    if (queue_became_non_full) not_full_cb = becomes_not_full_callback_;

    *stream_is_done =
        queue_.empty() && next_timestamp_bound_ == Timestamp::Done();
  }
  if (not_full_cb) not_full_cb(this, &last_reported_stream_full_);
  return packet;
}

Packet InputStreamManager::PopQueueHead(bool* stream_is_done) {
  Packet p;
  std::lock_guard<std::mutex> lock(mutex_);
  if (queue_.empty()) {
    *stream_is_done =
        queue_.empty() && next_timestamp_bound_ == Timestamp::Done();
    return Packet();
  }
  p = std::move(queue_.front());
  queue_.pop_front();
  // Frame and done are SEPARATE signals (MediaPipe): popping a frame must NOT
  // also report done, even if the stream is now empty and its bound is Done.
  // The done state is only observed on a later empty invocation, so a node that
  // flushes on IsDone() (e.g. an encoder) does not flush twice (once on the
  // last-frame invocation and again on the finalize invocation).
  *stream_is_done = false;
  return p;
}

Packet InputStreamManager::QueueHead() const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (queue_.empty()) return Packet();
  return queue_.front();
}

bool InputStreamManager::IsEmpty() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return queue_.empty();
}

bool InputStreamManager::IsFull() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return max_queue_size_ != -1 &&
         static_cast<int>(queue_.size()) >= max_queue_size_;
}

bool InputStreamManager::IsDone() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return queue_.empty() && next_timestamp_bound_ == Timestamp::Done();
}

int InputStreamManager::QueueSize() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return static_cast<int>(queue_.size());
}

int InputStreamManager::MaxQueueSize() const { return max_queue_size_; }

int64_t InputStreamManager::NumPacketsAdded() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return num_packets_added_;
}

Timestamp InputStreamManager::MinTimestampOrBound(bool* is_empty) const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!queue_.empty()) {
    if (is_empty) *is_empty = false;
    return queue_.front().timestamp();
  }
  if (is_empty) *is_empty = true;
  return next_timestamp_bound_;
}

void InputStreamManager::SetMaxQueueSize(int max_queue_size) {
  std::lock_guard<std::mutex> lock(mutex_);
  max_queue_size_ = max_queue_size;
}

void InputStreamManager::SetQueueSizeCallbacks(
    QueueSizeCallback full_cb, QueueSizeCallback not_full_cb) {
  std::lock_guard<std::mutex> lock(mutex_);
  becomes_full_callback_ = std::move(full_cb);
  becomes_not_full_callback_ = std::move(not_full_cb);
}

void InputStreamManager::SetArrivalCallback(PacketArrivalCallback cb) {
  std::lock_guard<std::mutex> lock(mutex_);
  arrival_callback_ = std::move(cb);
}

void InputStreamManager::PrepareForRun() {
  std::lock_guard<std::mutex> lock(mutex_);
  queue_.clear();
  num_packets_added_ = 0;
  next_timestamp_bound_ = Timestamp::PreStream();
  last_select_timestamp_ = Timestamp::Unstarted();
  closed_ = false;
  last_reported_stream_full_ = false;
}

void InputStreamManager::CleanupAfterRun() {
  std::lock_guard<std::mutex> lock(mutex_);
  queue_.clear();
}

}  // namespace graph::runtime
