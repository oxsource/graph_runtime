#ifndef GRAPH_RUNTIME_PARSER_REGISTRY_H_
#define GRAPH_RUNTIME_PARSER_REGISTRY_H_

#include <functional>
#include <memory>
#include <string>

#include "src/framework/config/i_graph_config_parser.h"

namespace graph::runtime {

#define GRAPH_RUNTIME_REGISTER_PARSER(extension, parser_class)        \
  static auto _register_parser_##__COUNTER__ = [] {                     \
    ::graph::runtime::ParserRegistry::Register(                       \
        extension,                                                    \
        []() -> std::unique_ptr<::graph::runtime::IGraphConfigParser> {\
          return std::make_unique<parser_class>();                    \
        });                                                           \
    return 0;                                                         \
  }()

class ParserRegistry {
 public:
  using ParserFactory = std::function<std::unique_ptr<IGraphConfigParser>()>;

  static void Register(const std::string& extension, ParserFactory factory);
  static std::unique_ptr<IGraphConfigParser> CreateForFile(
      const std::string& file_path);
  static bool IsFormatSupported(const std::string& file_path);
};

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_PARSER_REGISTRY_H_
