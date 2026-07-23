#ifndef GRAPH_RUNTIME_INPUT_STREAM_H_
#define GRAPH_RUNTIME_INPUT_STREAM_H_

#include <string>

#include "graph_runtime/src/stream/packet.h"

namespace graph::runtime {

class InputStream {
 public:
  virtual ~InputStream() = default;

  virtual const std::string& Name() const = 0;

  virtual const Packet& Value() const = 0;
  virtual Packet& Value() = 0;

  template <typename T>
  const T& Get() const {
    return Value().template Get<T>();
  }

  virtual bool IsEmpty() const = 0;
  virtual bool IsDone() const = 0;

  virtual Packet Header() const = 0;
};

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_INPUT_STREAM_H_
