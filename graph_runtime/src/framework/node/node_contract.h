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
    type_name_ = typeid(T).name();
    return *this;
  }
  PacketType& SetAny() { return Set<void>(); }
  PacketType& SetNone() { return *this; }
  PacketType& SetSameAs(const PacketType& other) {
    has_type_ = other.has_type_;
    type_name_ = other.type_name_;
    return *this;
  }
  bool IsSet() const { return has_type_; }
  const std::string& TypeName() const { return type_name_; }

 private:
  bool has_type_ = false;
  std::string type_name_;
};

class PacketTypeSet {
 public:
  PacketType& Get(const std::string& port_name) {
    return ports_[port_name];
  }
  const PacketType& Get(const std::string& port_name) const {
    static const PacketType kEmpty;
    auto it = ports_.find(port_name);
    if (it != ports_.end()) return it->second;
    return kEmpty;
  }
  PacketType& Get(CollectionItemId id) {
    return ports_[std::to_string(id)];
  }
  // Indexed access: Get("VIDEO", 0) constructs "VIDEO:0" internally.
  PacketType& Get(const std::string& tag, int index) {
    return ports_[tag + ":" + std::to_string(index)];
  }
  const PacketType& Get(const std::string& tag, int index) const {
    auto it = ports_.find(tag + ":" + std::to_string(index));
    if (it != ports_.end()) return it->second;
    static const PacketType kEmpty;
    return kEmpty;
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
  const PacketTypeSet& Inputs() const { return inputs_; }
  PacketTypeSet& Outputs() { return outputs_; }
  const PacketTypeSet& Outputs() const { return outputs_; }
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
