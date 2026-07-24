#include "src/framework/stream/timestamp.h"

#include <algorithm>
#include <climits>
#include <string>

#include "absl/strings/str_cat.h"

namespace graph::runtime {

namespace {

constexpr int64_t kSpecialValueMin = INT64_MIN + 3;
constexpr int64_t kSpecialValueMax = INT64_MAX - 3;

const char* SpecialValueName(int64_t v) {
  if (v == INT64_MIN) return "Timestamp::Unset()";
  if (v == INT64_MIN + 1) return "Timestamp::Unstarted()";
  if (v == INT64_MIN + 2) return "Timestamp::PreStream()";
  if (v == INT64_MIN + 3) return "Timestamp::Min()";
  if (v == INT64_MAX - 3) return "Timestamp::Max()";
  if (v == INT64_MAX - 2) return "Timestamp::PostStream()";
  if (v == INT64_MAX - 1) return "Timestamp::OneOverPostStream()";
  if (v == INT64_MAX) return "Timestamp::Done()";
  return nullptr;
}

}  // namespace

Timestamp Timestamp::Unset() {
  return Timestamp(INT64_MIN, true);
}

Timestamp Timestamp::Unstarted() {
  return Timestamp(INT64_MIN + 1, true);
}

Timestamp Timestamp::PreStream() {
  return Timestamp(INT64_MIN + 2, true);
}

Timestamp Timestamp::Min() {
  return Timestamp(INT64_MIN + 3, true);
}

Timestamp Timestamp::Max() {
  return Timestamp(INT64_MAX - 3, true);
}

Timestamp Timestamp::PostStream() {
  return Timestamp(INT64_MAX - 2, true);
}

Timestamp Timestamp::OneOverPostStream() {
  return Timestamp(INT64_MAX - 1, true);
}

Timestamp Timestamp::Done() {
  return Timestamp(INT64_MAX, true);
}

Timestamp::Timestamp(int64_t timestamp) : timestamp_(timestamp) {
  // ABSL_CHECK(!IsSpecialValue()) would be used in production.
  // For Phase 1, allow but document: special values need named constructors.
}

Timestamp::Timestamp(int64_t value, bool /*construct_special*/)
    : timestamp_(value) {}

int64_t Timestamp::Value() const {
  return timestamp_;
}

bool Timestamp::IsSpecialValue() const {
  return timestamp_ <= kSpecialValueMin || timestamp_ >= kSpecialValueMax;
}

bool Timestamp::IsRangeValue() const {
  return timestamp_ >= kSpecialValueMin && timestamp_ <= kSpecialValueMax;
}

bool Timestamp::IsAllowedInStream() const {
  return timestamp_ >= INT64_MIN + 2 && timestamp_ <= INT64_MAX - 2;
}

bool Timestamp::IsEmpty() const {
  return timestamp_ == INT64_MIN;
}

Timestamp Timestamp::NextAllowedInStream() const {
  if (timestamp_ >= INT64_MAX - 2) return Timestamp(INT64_MAX - 1, true);
  return Timestamp(timestamp_ + 1);
}

Timestamp Timestamp::PreviousAllowedInStream() const {
  if (timestamp_ <= INT64_MIN + 2) return Timestamp(INT64_MIN + 1, true);
  return Timestamp(timestamp_ - 1);
}

std::string Timestamp::DebugString() const {
  const char* name = SpecialValueName(timestamp_);
  if (name) return name;
  return absl::StrCat(timestamp_);
}

TimestampDiff TimestampDiff::Unset() {
  return TimestampDiff(INT64_MIN);
}

TimestampDiff::TimestampDiff(int64_t value) : diff_(value) {}

int64_t TimestampDiff::Value() const {
  return diff_;
}

bool TimestampDiff::IsEmpty() const {
  return diff_ == INT64_MIN;
}

std::string TimestampDiff::DebugString() const {
  if (diff_ == INT64_MIN) return "TimestampDiff::Unset()";
  return absl::StrCat(diff_);
}

TimestampDiff operator-(Timestamp t1, Timestamp t2) {
  return TimestampDiff(t1.Value() - t2.Value());
}

Timestamp operator-(Timestamp t, TimestampDiff d) {
  return Timestamp(std::max<int64_t>(t.Value() - d.Value(), INT64_MIN + 3));
}

Timestamp operator+(Timestamp t, TimestampDiff d) {
  return Timestamp(std::min<int64_t>(t.Value() + d.Value(), INT64_MAX - 3));
}

TimestampDiff operator+(TimestampDiff d1, TimestampDiff d2) {
  return TimestampDiff(d1.Value() + d2.Value());
}

}  // namespace graph::runtime
