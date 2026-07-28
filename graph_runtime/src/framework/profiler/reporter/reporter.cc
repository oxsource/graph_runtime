#include "src/framework/profiler/reporter/reporter.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"

namespace graph::runtime {
namespace {

class SimpleJsonReader {
 public:
  explicit SimpleJsonReader(const std::string& content)
      : content_(content), pos_(0) {}

  bool ok() const { return pos_ < content_.size(); }

  void SkipWhitespace() {
    while (pos_ < content_.size() &&
           (content_[pos_] == ' ' || content_[pos_] == '\n' ||
            content_[pos_] == '\r' || content_[pos_] == '\t')) {
      ++pos_;
    }
  }

  bool Expect(char c) {
    SkipWhitespace();
    if (pos_ < content_.size() && content_[pos_] == c) {
      ++pos_;
      return true;
    }
    return false;
  }

  std::string ReadString() {
    SkipWhitespace();
    if (pos_ >= content_.size() || content_[pos_] != '"') return "";
    ++pos_;
    std::string result;
    while (pos_ < content_.size() && content_[pos_] != '"') {
      if (content_[pos_] == '\\') {
        ++pos_;
        if (pos_ >= content_.size()) break;
        switch (content_[pos_]) {
          case '"': result += '"'; break;
          case '\\': result += '\\'; break;
          case 'n': result += '\n'; break;
          case 'r': result += '\r'; break;
          case 't': result += '\t'; break;
          default: result += content_[pos_]; break;
        }
      } else {
        result += content_[pos_];
      }
      ++pos_;
    }
    if (pos_ < content_.size()) ++pos_;
    return result;
  }

  int64_t ReadInt64() {
    SkipWhitespace();
    bool neg = false;
    if (pos_ < content_.size() && content_[pos_] == '-') {
      neg = true;
      ++pos_;
    }
    int64_t val = 0;
    while (pos_ < content_.size() && std::isdigit(content_[pos_])) {
      val = val * 10 + (content_[pos_] - '0');
      ++pos_;
    }
    return neg ? -val : val;
  }

  double ReadDouble() {
    SkipWhitespace();
    bool neg = false;
    if (pos_ < content_.size() && content_[pos_] == '-') {
      neg = true;
      ++pos_;
    }
    double val = 0.0;
    while (pos_ < content_.size() && std::isdigit(content_[pos_])) {
      val = val * 10.0 + (content_[pos_] - '0');
      ++pos_;
    }
    if (pos_ < content_.size() && content_[pos_] == '.') {
      ++pos_;
      double frac = 0.0;
      double div = 1.0;
      while (pos_ < content_.size() && std::isdigit(content_[pos_])) {
        frac = frac * 10.0 + (content_[pos_] - '0');
        div *= 10.0;
        ++pos_;
      }
      val += frac / div;
    }
    return neg ? -val : val;
  }

  bool ReadBool() {
    SkipWhitespace();
    if (content_.substr(pos_, 4) == "true") {
      pos_ += 4;
      return true;
    }
    if (content_.substr(pos_, 5) == "false") {
      pos_ += 5;
      return false;
    }
    return false;
  }

  void SkipValue() {
    SkipWhitespace();
    if (pos_ >= content_.size()) return;
    if (content_[pos_] == '"') {
      ReadString();
    } else if (content_[pos_] == '{') {
      SkipObject();
    } else if (content_[pos_] == '[') {
      SkipArray();
    } else if (content_[pos_] == 't' || content_[pos_] == 'f') {
      ReadBool();
    } else {
      ReadDouble();
    }
  }

  void SkipObject() {
    Expect('{');
    int depth = 1;
    while (pos_ < content_.size() && depth > 0) {
      SkipWhitespace();
      if (content_[pos_] == '{') { ++depth; ++pos_; }
      else if (content_[pos_] == '}') { --depth; ++pos_; }
      else if (content_[pos_] == '"') { ReadString(); }
      else if (content_[pos_] == '[') { SkipArray(); }
      else if (content_[pos_] == 't' || content_[pos_] == 'f') { ReadBool(); }
      else if (content_[pos_] == '-' || std::isdigit(content_[pos_])) { ReadDouble(); }
      else { ++pos_; }
    }
  }

  void SkipArray() {
    Expect('[');
    int depth = 1;
    while (pos_ < content_.size() && depth > 0) {
      SkipWhitespace();
      if (content_[pos_] == '[') { ++depth; ++pos_; }
      else if (content_[pos_] == ']') { --depth; ++pos_; }
      else if (content_[pos_] == '{') { SkipObject(); }
      else if (content_[pos_] == '"') { ReadString(); }
      else if (content_[pos_] == 't' || content_[pos_] == 'f') { ReadBool(); }
      else if (content_[pos_] == '-' || std::isdigit(content_[pos_])) { ReadDouble(); }
      else { ++pos_; }
    }
  }

  std::string ReadKey() {
    SkipWhitespace();
    if (pos_ >= content_.size() || content_[pos_] != '"') return "";
    return ReadString();
  }

 private:
  std::string content_;
  size_t pos_;
};

}  // namespace

absl::Status Reporter::Accumulate(const std::string& json_path) {
  std::ifstream file(json_path);
  if (!file.is_open()) {
    return absl::NotFoundError(
        absl::StrCat("Profile file not found: ", json_path));
  }
  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string content = buffer.str();

  SimpleJsonReader reader(content);

  if (!reader.Expect('{')) {
    return absl::InvalidArgumentError("Expected JSON object");
  }

  while (reader.ok()) {
    if (reader.Expect('}')) break;

    std::string key = reader.ReadKey();
    if (key.empty()) break;

    reader.Expect(':');

    if (key == "nodes") {
      if (!reader.Expect('[')) {
        reader.SkipValue();
        reader.Expect(',');
        continue;
      }
      while (reader.ok()) {
        if (reader.Expect(']')) break;
        if (!reader.Expect('{')) {
          reader.SkipValue();
          reader.Expect(',');
          continue;
        }

        std::string name;
        int64_t open_runtime = 0;
        int64_t close_runtime = 0;
        int64_t process_count = 0;
        int64_t process_time_total = 0;

        while (reader.ok()) {
          if (reader.Expect('}')) {
            reader.Expect(',');
            break;
          }
          std::string field = reader.ReadKey();
          if (field.empty()) break;
          reader.Expect(':');

          if (field == "node_name") {
            name = reader.ReadString();
            reader.Expect(',');
          } else if (field == "open_runtime_usec") {
            open_runtime = reader.ReadInt64();
            reader.Expect(',');
          } else if (field == "close_runtime_usec") {
            close_runtime = reader.ReadInt64();
            reader.Expect(',');
          } else if (field == "process_count") {
            process_count = reader.ReadInt64();
            reader.Expect(',');
          } else if (field == "process_time_total_usec") {
            process_time_total = reader.ReadInt64();
            reader.Expect(',');
          } else if (field == "process_time_mean_usec") {
            reader.ReadDouble();
            reader.Expect(',');
          } else {
            reader.SkipValue();
            reader.Expect(',');
          }
        }

        if (!name.empty()) {
          PerNodeAccum* entry = nullptr;
          for (auto& e : node_data_) {
            if (e.node_name == name) { entry = &e; break; }
          }
          if (!entry) {
            PerNodeAccum e;
            e.node_name = name;
            e.process_time_min_usec = INT64_MAX;
            node_data_.push_back(e);
            entry = &node_data_.back();
          }

          entry->open_runtime_usec = open_runtime;
          entry->close_runtime_usec = close_runtime;
          entry->process_count += process_count;
          entry->process_time_total_usec += process_time_total;
          if (process_count > 0) {
            int64_t avg = process_time_total / process_count;
            if (avg < entry->process_time_min_usec)
              entry->process_time_min_usec = avg;
            if (avg > entry->process_time_max_usec)
              entry->process_time_max_usec = avg;
          }
          entry->samples++;
        }
      }
    } else {
      reader.SkipValue();
      reader.Expect(',');
    }
  }

  return absl::OkStatus();
}

ProfileReport Reporter::Report() const {
  ProfileReport report;
  for (const auto& entry : node_data_) {
    ProfileReport::NodeStats stats;
    stats.node_name = entry.node_name;
    stats.open_runtime_usec = entry.open_runtime_usec;
    stats.close_runtime_usec = entry.close_runtime_usec;
    stats.process_count = entry.process_count;
    stats.process_time_total_usec = entry.process_time_total_usec;
    stats.process_time_mean_usec =
        (entry.process_count > 0)
            ? static_cast<double>(entry.process_time_total_usec) /
                  entry.process_count
            : 0.0;
    stats.process_time_min_usec =
        (entry.process_time_min_usec == INT64_MAX)
            ? 0 : entry.process_time_min_usec;
    stats.process_time_max_usec = entry.process_time_max_usec;
    report.nodes.push_back(std::move(stats));
    report.total_process_count += stats.process_count;
    report.total_process_time_usec += stats.process_time_total_usec;
  }
  return report;
}

std::vector<Reporter::Delta> Reporter::Compare(
    const ProfileReport& baseline) const {
  std::vector<Delta> deltas;
  auto current = Report();

  std::map<std::string, const ProfileReport::NodeStats*> baseline_map;
  for (const auto& node : baseline.nodes) {
    baseline_map[node.node_name] = &node;
  }

  for (const auto& node : current.nodes) {
    Delta delta;
    delta.node_name = node.node_name;
    auto it = baseline_map.find(node.node_name);
    if (it != baseline_map.end()) {
      double baseline_mean = it->second->process_time_mean_usec;
      delta.process_mean_delta_usec =
          node.process_time_mean_usec - baseline_mean;
      if (baseline_mean > 0.0) {
        delta.process_mean_delta_pct =
            (delta.process_mean_delta_usec / baseline_mean) * 100.0;
      } else {
        delta.process_mean_delta_pct = 0.0;
      }
    } else {
      delta.process_mean_delta_usec = node.process_time_mean_usec;
      delta.process_mean_delta_pct = 100.0;
    }
    deltas.push_back(std::move(delta));
  }

  return deltas;
}

void Reporter::Clear() {
  node_data_.clear();
}

}  // namespace graph::runtime
