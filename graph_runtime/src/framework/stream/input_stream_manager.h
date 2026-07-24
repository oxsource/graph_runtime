#ifndef GRAPH_RUNTIME_INPUT_STREAM_MANAGER_H_
#define GRAPH_RUNTIME_INPUT_STREAM_MANAGER_H_

#include <deque>
#include <functional>
#include <list>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "src/framework/stream/packet.h"

namespace graph::runtime {

using QueueSizeCallback = std::function<void(class InputStreamManager*, bool*)>;
using PacketArrivalCallback = std::function<void()>;

class InputStreamManager {
 public:
  explicit InputStreamManager(std::string name, int max_queue_size = -1);

  const std::string& name() const;

  absl::Status AddPackets(const std::list<Packet>& packets, bool* notify);
  absl::Status MovePackets(std::list<Packet>* packets, bool* notify);

  void SetNextTimestampBound(Timestamp bound);
  void Close();

  Packet PopPacketAtTimestamp(Timestamp timestamp,
                              int* num_packets_dropped,
                              bool* stream_is_done);
  Packet PopQueueHead(bool* stream_is_done);
  Packet QueueHead() const;

  bool IsEmpty() const;
  bool IsFull() const;
  bool IsDone() const;
  int QueueSize() const;
  int MaxQueueSize() const;
  int64_t NumPacketsAdded() const;

  Timestamp MinTimestampOrBound(bool* is_empty) const;

  void SetMaxQueueSize(int max_queue_size);
  void SetQueueSizeCallbacks(QueueSizeCallback full_cb,
                             QueueSizeCallback not_full_cb);
  void SetArrivalCallback(PacketArrivalCallback cb);

  void PrepareForRun();
  void CleanupAfterRun();

 private:
  std::string name_;
  std::deque<Packet> queue_;
  int64_t num_packets_added_ = 0;
  Timestamp next_timestamp_bound_{Timestamp::PreStream()};
  Timestamp last_select_timestamp_{Timestamp::Unstarted()};
  bool closed_ = false;
  int max_queue_size_ = -1;
  bool enable_timestamps_ = true;

  PacketArrivalCallback arrival_callback_;
  QueueSizeCallback becomes_full_callback_;
  QueueSizeCallback becomes_not_full_callback_;
  bool last_reported_stream_full_ = false;
};

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_INPUT_STREAM_MANAGER_H_
