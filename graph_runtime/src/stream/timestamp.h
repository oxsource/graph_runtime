#ifndef GRAPH_RUNTIME_TIMESTAMP_H_
#define GRAPH_RUNTIME_TIMESTAMP_H_

#include <cstdint>
#include <string>

namespace graph::runtime {

class Timestamp {
 public:
  static Timestamp Unset();
  static Timestamp Unstarted();
  static Timestamp PreStream();
  static Timestamp Min();
  static Timestamp Max();
  static Timestamp PostStream();
  static Timestamp OneOverPostStream();
  static Timestamp Done();

  explicit Timestamp(int64_t timestamp);
  int64_t Value() const;

  bool IsSpecialValue() const;
  bool IsRangeValue() const;
  bool IsAllowedInStream() const;
  bool IsEmpty() const;

  Timestamp NextAllowedInStream() const;
  Timestamp PreviousAllowedInStream() const;

  std::string DebugString() const;

  bool operator==(const Timestamp& other) const {
    return timestamp_ == other.timestamp_;
  }
  bool operator!=(const Timestamp& other) const {
    return timestamp_ != other.timestamp_;
  }
  bool operator<(const Timestamp& other) const {
    return timestamp_ < other.timestamp_;
  }

 private:
  friend class TimestampDiff;
  explicit Timestamp(int64_t value, bool /*construct_special*/);
  int64_t timestamp_;
};

class TimestampDiff {
 public:
  static TimestampDiff Unset();
  explicit TimestampDiff(int64_t value);
  int64_t Value() const;
  bool IsEmpty() const;
  std::string DebugString() const;

 private:
  int64_t diff_;
};

TimestampDiff operator-(Timestamp t1, Timestamp t2);
Timestamp operator-(Timestamp t, TimestampDiff d);
Timestamp operator+(Timestamp t, TimestampDiff d);
TimestampDiff operator+(TimestampDiff d1, TimestampDiff d2);

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_TIMESTAMP_H_
