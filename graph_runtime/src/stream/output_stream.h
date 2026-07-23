#ifndef GRAPH_RUNTIME_OUTPUT_STREAM_H_
#define GRAPH_RUNTIME_OUTPUT_STREAM_H_

#include <string>

#include "graph_runtime/src/stream/packet.h"
#include "graph_runtime/src/stream/timestamp.h"

namespace graph::runtime {

class OutputStream {
 public:
  virtual ~OutputStream() = default;

  virtual const std::string& Name() const = 0;
  virtual void AddPacket(const Packet& packet) = 0;
  virtual void AddPacket(Packet&& packet) = 0;
  virtual void SetNextTimestampBound(Timestamp timestamp) = 0;
  virtual Timestamp NextTimestampBound() const = 0;
  virtual void Close() = 0;
  virtual bool IsClosed() const = 0;

  virtual void SetOffset(TimestampDiff offset) = 0;
  virtual bool OffsetEnabled() const = 0;
  virtual TimestampDiff Offset() const = 0;
  virtual void SetHeader(const Packet& packet) = 0;
  virtual const Packet& Header() const = 0;
};

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_OUTPUT_STREAM_H_
