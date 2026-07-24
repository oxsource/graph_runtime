#include "src/node/node_registry.h"
#include "src/config/parser_registry.h"

namespace graph::runtime {

static const bool kAnchorNodeFactory = []() {
  NodeFactoryRegistry::RegisteredTypes();
  return true;
}();

static const bool kAnchorParser = []() {
  ParserRegistry::CreateForFile("");
  return true;
}();

}  // namespace graph::runtime
