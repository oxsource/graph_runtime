#ifndef GRAPH_RUNTIME_TYPES_H_
#define GRAPH_RUNTIME_TYPES_H_

#include <functional>

#include "absl/status/status.h"

namespace graph::runtime {

using CollectionItemId = int;

using ErrorCallback = std::function<void(absl::Status error)>;

inline bool IsStopStatus(const absl::Status& status) {
  return status.code() == absl::StatusCode::kUnavailable;
}

inline absl::Status StatusStop() {
  return absl::UnavailableError("Stop");
}

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_TYPES_H_
