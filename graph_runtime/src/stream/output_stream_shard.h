#ifndef GRAPH_RUNTIME_OUTPUT_STREAM_SHARD_H_
#define GRAPH_RUNTIME_OUTPUT_STREAM_SHARD_H_

#include <list>
#include <string>

#include "graph_runtime/src/stream/output_stream.h"

namespace graph::runtime {

class OutputStreamManager;

struct OutputStreamSpec {
  std::string name;
  bool locked_intro_data = false;
  bool offset_enabled = false;
  TimestampDiff offset = TimestampDiff(0);
  Packet header;
};

class OutputStreamShard : public OutputStream {
 public:
  OutputStreamShard();
  explicit OutputStreamShard(OutputStreamSpec* spec);
  ~OutputStreamShard() override = default;

  void SetSpec(OutputStreamSpec* spec);
  void Reset(Timestamp next_timestamp_bound, bool close);

  const std::string& Name() const final;
  void AddPacket(const Packet& packet) final;
  void AddPacket(Packet&& packet) final;
  void SetNextTimestampBound(Timestamp bound) final;
  Timestamp NextTimestampBound() const final { return next_timestamp_bound_; }
  void Close() final;
  bool IsClosed() const final;
  void SetOffset(TimestampDiff offset) final;
  bool OffsetEnabled() const final;
  TimestampDiff Offset() const final;
  void SetHeader(const Packet& packet) final;
  const Packet& Header() const final;

  bool IsEmpty() const { return output_queue_.empty(); }
  Timestamp LastAddedPacketTimestamp() const;
  std::list<Packet>& OutputQueue() { return output_queue_; }

 private:
  friend class OutputStreamManager;
  template <typename T>
  void AddPacketInternal(T&& packet);

  OutputStreamSpec* spec_ = nullptr;
  std::list<Packet> output_queue_;
  bool closed_ = false;
  Timestamp next_timestamp_bound_{Timestamp::Unset()};
  Timestamp updated_next_timestamp_bound_{Timestamp::Unset()};
};

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_OUTPUT_STREAM_SHARD_H_
