#ifndef GRAPH_RUNTIME_SIDE_PACKET_H_
#define GRAPH_RUNTIME_SIDE_PACKET_H_

#include <map>
#include <string>

#include "absl/status/status.h"
#include "src/stream/packet.h"
#include "src/public/types.h"

namespace graph::runtime {

class PacketSet {
 public:
  Packet Get(const std::string& name) const {
    auto it = packets_.find(name);
    return it != packets_.end() ? it->second : Packet();
  }
  void Set(const std::string& name, const Packet& packet) {
    packets_[name] = packet;
  }
  int NumEntries() const { return static_cast<int>(packets_.size()); }

 private:
  std::map<std::string, Packet> packets_;
};

class OutputSidePacketSet {
 public:
  absl::Status Set(const std::string& name, const Packet& packet) {
    packets_[name] = packet;
    return absl::OkStatus();
  }
  absl::Status Set(const std::string& name, Packet&& packet) {
    packets_[name] = std::move(packet);
    return absl::OkStatus();
  }
  Packet Get(const std::string& name) const {
    auto it = packets_.find(name);
    return it != packets_.end() ? it->second : Packet();
  }
  int NumEntries() const { return static_cast<int>(packets_.size()); }

 private:
  std::map<std::string, Packet> packets_;
};

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_SIDE_PACKET_H_
