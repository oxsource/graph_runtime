#ifndef GRAPH_RUNTIME_PACKET_H_
#define GRAPH_RUNTIME_PACKET_H_

#include <memory>
#include <string>
#include <type_traits>
#include <typeinfo>

#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "src/stream/timestamp.h"

namespace graph::runtime {

class Packet {
 public:
  Packet();
  Packet(const Packet&) = default;
  Packet& operator=(const Packet&) = default;
  Packet(Packet&&) = default;
  Packet& operator=(Packet&&) = default;

  template <typename T, typename... Args>
  static Packet MakePacket(Args&&... args);

  template <typename T>
  static Packet Adopt(const T* ptr);

  Timestamp timestamp() const;
  Packet At(Timestamp ts) const&;
  Packet At(Timestamp ts) &&;

  bool IsEmpty() const;

  template <typename T>
  absl::StatusOr<T> Get() const;

  template <typename T>
  absl::Status ValidateAsType() const;

  template <typename T>
  absl::StatusOr<std::shared_ptr<const T>> Share() const;

  friend bool operator==(const Packet& a, const Packet& b);
  friend bool operator!=(const Packet& a, const Packet& b);

  std::string DebugString() const;
  std::string DebugTypeName() const;

 private:
  struct HolderBase {
    virtual ~HolderBase() = default;
    virtual const void* Ptr() const = 0;
    virtual const std::type_info& Type() const = 0;
  };

  template <typename T>
  struct Holder : HolderBase {
    explicit Holder(const T* ptr) : ptr_(ptr) {}
    ~Holder() override { delete ptr_; }
    const void* Ptr() const override { return ptr_; }
    const std::type_info& Type() const override { return typeid(T); }
    const T* ptr_;
  };

  std::shared_ptr<const HolderBase> holder_;
  Timestamp timestamp_;
};

template <typename T, typename... Args>
Packet Packet::MakePacket(Args&&... args) {
  Packet p;
  p.holder_ = std::shared_ptr<const HolderBase>(
      new Holder<T>(new T(std::forward<Args>(args)...)));
  return p;
}

template <typename T>
Packet Packet::Adopt(const T* ptr) {
  Packet p;
  p.holder_ = std::shared_ptr<const HolderBase>(new Holder<T>(ptr));
  return p;
}

template <typename T>
absl::StatusOr<T> Packet::Get() const {
  if (IsEmpty()) {
    return absl::InternalError("Packet is empty");
  }
  if (holder_->Type() != typeid(T)) {
    return absl::InvalidArgumentError(
        absl::StrCat("Packet type mismatch: expected ", typeid(T).name(),
                     ", got ", holder_->Type().name()));
  }
  const auto* holder = static_cast<const Holder<T>*>(holder_.get());
  return *holder->ptr_;
}

template <typename T>
absl::StatusOr<std::shared_ptr<const T>> Packet::Share() const {
  if (IsEmpty()) {
    return absl::InternalError("Packet is empty");
  }
  if (holder_->Type() != typeid(T)) {
    return absl::InvalidArgumentError(
        absl::StrCat("Packet type mismatch: expected ", typeid(T).name(),
                     ", got ", holder_->Type().name()));
  }
  const auto* holder = static_cast<const Holder<T>*>(holder_.get());
  return std::shared_ptr<const T>(holder_, holder->ptr_);
}

template <typename T>
absl::Status Packet::ValidateAsType() const {
  if (IsEmpty()) {
    return absl::InternalError("Packet is empty");
  }
  if (holder_->Type() != typeid(T)) {
    return absl::InvalidArgumentError(
        absl::StrCat("Packet type mismatch: expected ", typeid(T).name(),
                     ", got ", holder_->Type().name()));
  }
  return absl::OkStatus();
}

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_PACKET_H_
