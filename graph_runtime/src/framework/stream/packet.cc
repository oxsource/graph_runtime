#include "src/framework/stream/packet.h"

#include <string>

#include "absl/strings/str_cat.h"

namespace graph::runtime {

Packet::Packet() : timestamp_(Timestamp::Unset()) {}

Timestamp Packet::timestamp() const {
  return timestamp_;
}

Packet Packet::At(Timestamp ts) const& {
  Packet p(*this);
  p.timestamp_ = ts;
  return p;
}

Packet Packet::At(Timestamp ts) && {
  timestamp_ = ts;
  return std::move(*this);
}

bool Packet::IsEmpty() const {
  return holder_ == nullptr;
}

bool operator==(const Packet& a, const Packet& b) {
  return a.holder_.get() == b.holder_.get();
}

bool operator!=(const Packet& a, const Packet& b) {
  return a.holder_.get() != b.holder_.get();
}

std::string Packet::DebugString() const {
  std::string result =
      absl::StrCat("Packet with timestamp: ", timestamp_.DebugString());
  if (IsEmpty()) {
    absl::StrAppend(&result, " and no data");
  } else {
    absl::StrAppend(&result, " and type: ", holder_->Type().name());
  }
  return result;
}

std::string Packet::DebugTypeName() const {
  if (IsEmpty()) return "{empty}";
  return holder_->Type().name();
}

}  // namespace graph::runtime
