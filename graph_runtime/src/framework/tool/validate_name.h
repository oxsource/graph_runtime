#ifndef GRAPH_RUNTIME_VALIDATE_NAME_H_
#define GRAPH_RUNTIME_VALIDATE_NAME_H_

#include <string>
#include <tuple>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace graph::runtime {

// Parsed result from a "TAG:index:name" string.
struct TagIndexName {
  std::string tag;      // e.g. "VIDEO"
  int index;            // e.g. 2
  std::string name;     // e.g. "left_cam" (may be empty)
};

// Parse a "TAG:index" or "TAG:index:name" string.
// "VIDEO:0"         → {tag="VIDEO", index=0, name=""}
// "VIDEO:2:left"    → {tag="VIDEO", index=2, name="left"}
// "plain_name"      → {tag="", index=-1, name="plain_name"}
absl::StatusOr<TagIndexName> ParseTagIndexName(const std::string& input);

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_VALIDATE_NAME_H_
