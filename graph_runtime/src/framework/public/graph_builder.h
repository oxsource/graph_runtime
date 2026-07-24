#ifndef GRAPH_RUNTIME_GRAPH_BUILDER_H_
#define GRAPH_RUNTIME_GRAPH_BUILDER_H_

#include <memory>

#include "absl/status/statusor.h"
#include "src/framework/config/graph_config.h"

namespace graph::runtime {

class GraphRuntime;

class GraphBuilder {
 public:
  static absl::StatusOr<std::unique_ptr<GraphRuntime>> Build(
      const GraphConfig& config);

 private:
  GraphBuilder() = default;
};

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_GRAPH_BUILDER_H_
