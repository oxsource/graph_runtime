#ifndef GRAPH_RUNTIME_CONFIG_STREAM_NAME_H_
#define GRAPH_RUNTIME_CONFIG_STREAM_NAME_H_

#include <string>

namespace graph::runtime {

// Port name = the part before the first ':' in "port:stream" format.
// Returns the whole string when no colon is present (plain stream names).
inline std::string PortName(const std::string& port_stream) {
  auto pos = port_stream.find(':');
  if (pos == std::string::npos) return port_stream;
  return port_stream.substr(0, pos);
}

// Stream name = the part after the first ':' in "port:stream" format.
// Returns the whole string when no colon is present (plain stream names).
inline std::string StreamName(const std::string& port_stream) {
  auto pos = port_stream.find(':');
  if (pos == std::string::npos) return port_stream;
  return port_stream.substr(pos + 1);
}

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_CONFIG_STREAM_NAME_H_
