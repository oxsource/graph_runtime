#include "src/framework/tool/validate_name.h"

#include <cstdlib>
#include <vector>

#include "absl/strings/str_split.h"

namespace graph::runtime {

absl::StatusOr<TagIndexName> ParseTagIndexName(const std::string& input) {
  TagIndexName result;
  std::vector<std::string> parts = absl::StrSplit(input, ':');

  if (parts.size() == 1) {
    result.name = parts[0];
    result.tag = "";
    result.index = -1;
    return result;
  }

  if (parts.size() == 2) {
    result.tag = parts[0];
    result.name = "";
    char* end = nullptr;
    long idx = std::strtol(parts[1].c_str(), &end, 10);
    if (*end != '\0' || parts[1].empty()) {
      return absl::InvalidArgumentError(
          "invalid index in '" + input + "': '" + parts[1] + "' is not an integer");
    }
    result.index = static_cast<int>(idx);
    return result;
  }

  if (parts.size() == 3) {
    result.tag = parts[0];
    result.name = parts[2];
    char* end = nullptr;
    long idx = std::strtol(parts[1].c_str(), &end, 10);
    if (*end != '\0' || parts[1].empty()) {
      return absl::InvalidArgumentError(
          "invalid index in '" + input + "': '" + parts[1] + "' is not an integer");
    }
    result.index = static_cast<int>(idx);
    return result;
  }

  return absl::InvalidArgumentError(
      "invalid format '" + input + "': expected TAG:index, TAG:index:name, or name");
}

}  // namespace graph::runtime
