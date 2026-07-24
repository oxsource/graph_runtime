#ifndef GRAPH_RUNTIME_NODE_CONTRACT_H_
#define GRAPH_RUNTIME_NODE_CONTRACT_H_

#include <map>
#include <string>
#include <vector>

#include "src/framework/stream/packet.h"
#include "src/framework/node/node_options.h"
#include "src/framework/public/types.h"

namespace graph::runtime {

class PacketType {
 public:
  template <typename T>
  PacketType& Set() {
    has_type_ = true;
    return *this;
  }
  PacketType& SetAny() { return Set<void>(); }
  PacketType& SetNone() { return *this; }
  PacketType& SetSameAs(const PacketType& other) { return *this; }
  bool IsSet() const { return has_type_; }
 private:
  bool has_type_ = false;
};

class PacketTypeSet {
 public:
  PacketType& Get(const std::string& port_name) {
    return ports_[port_name];
  }
  PacketType& Get(CollectionItemId id) {
    return ports_[std::to_string(id)];
  }
  int NumEntries() const {
    return static_cast<int>(ports_.size());
  }
 private:
  std::map<std::string, PacketType> ports_;
};

class NodeContract {
 public:
  PacketTypeSet& Inputs() { return inputs_; }
  PacketTypeSet& Outputs() { return outputs_; }
  PacketTypeSet& InputSidePackets() { return input_side_packets_; }
  PacketTypeSet& OutputSidePackets() { return output_side_packets_; }

  const NodeOptions& Options() const { return *options_; }
  void SetOptions(const NodeOptions* opts) { options_ = opts; }

  void SetMaxInFlight(int n) { max_in_flight_ = n; }
  int MaxInFlight() const { return max_in_flight_; }

  void SetProcessTimestampBounds(bool enable) {
    process_timestamp_bounds_ = enable;
  }
  bool ProcessTimestampBounds() const { return process_timestamp_bounds_; }

 private:
  PacketTypeSet inputs_;
  PacketTypeSet outputs_;
  PacketTypeSet input_side_packets_;
  PacketTypeSet output_side_packets_;
  const NodeOptions* options_ = nullptr;
  int max_in_flight_ = 1;
  bool process_timestamp_bounds_ = false;
};

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_NODE_CONTRACT_H_
