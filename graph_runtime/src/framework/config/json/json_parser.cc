#include "src/framework/config/json/json_parser.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "nlohmann/json.hpp"
#include "src/framework/config/config_validator.h"

namespace graph::runtime {
namespace {

// JSON field key constants — single source of truth for the JSON schema.
constexpr char kKeyMaxQueueSize[]       = "max_queue_size";
constexpr char kKeyReportDeadlock[]     = "report_deadlock";
constexpr char kKeyInputStreams[]       = "input_streams";
constexpr char kKeyOutputStreams[]      = "output_streams";
constexpr char kKeyInputSidePkts[]      = "input_side_packets";
constexpr char kKeyOutputSidePkts[]     = "output_side_packets";
constexpr char kKeyExecutors[]          = "executors";
constexpr char kKeyNodes[]              = "nodes";
constexpr char kKeyName[]               = "name";
constexpr char kKeyType[]               = "type";
constexpr char kKeyNumThreads[]         = "num_threads";
constexpr char kKeyExecutor[]           = "executor";
constexpr char kKeyInputStreamHandler[] = "input_stream_handler";
constexpr char kKeyMaxInFlight[]        = "max_in_flight";
constexpr char kKeySourceLayer[]        = "source_layer";

// Default values
constexpr int    kDefaultMaxQueueSize   = 100;
constexpr int    kDefaultNumThreads     = 0;
constexpr int    kDefaultMaxInFlight    = 1;
constexpr int    kDefaultSourceLayer    = 0;
constexpr char   kDefaultExecutorType[] = "ThreadPoolExecutor";

}  // namespace

absl::StatusOr<GraphConfig> JsonParser::Parse(
    const std::string& file_path) {
  std::ifstream file(file_path);
  if (!file.is_open()) {
    return absl::NotFoundError(
        absl::StrCat("config file not found: ", file_path));
  }
  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string json_text = buffer.str();

  nlohmann::json root;
  try {
    root = nlohmann::json::parse(json_text);
  } catch (const nlohmann::json::parse_error& e) {
    return absl::InvalidArgumentError(
        absl::StrCat(file_path, ": JSON parse error: ", e.what()));
  }

  GraphConfig config;
  config.max_queue_size = root.value(kKeyMaxQueueSize, kDefaultMaxQueueSize);
  config.report_deadlock = root.value(kKeyReportDeadlock, false);

  for (auto& s : root.value(kKeyInputStreams, std::vector<std::string>{}))
    config.input_streams.push_back(std::move(s));
  for (auto& s : root.value(kKeyOutputStreams, std::vector<std::string>{}))
    config.output_streams.push_back(std::move(s));

  // Parse executors
  if (root.contains(kKeyExecutors)) {
    for (const auto& ej : root[kKeyExecutors]) {
      GraphConfig::ExecutorDef def;
      def.name = ej.value(kKeyName, "");
      def.type = ej.value(kKeyType, kDefaultExecutorType);
      def.num_threads = ej.value(kKeyNumThreads, kDefaultNumThreads);
      config.executors.push_back(std::move(def));
    }
  }

  // Parse nodes — connections are implicit via stream name matching.
  if (root.contains(kKeyNodes)) {
    for (const auto& nj : root[kKeyNodes]) {
      GraphConfig::NodeDef def;
      def.name = nj.value(kKeyName, "");
      if (def.name.empty())
        return absl::InvalidArgumentError("node name is required");
      def.type = nj.value(kKeyType, "");
      if (def.type.empty())
        return absl::InvalidArgumentError(
            absl::StrCat("node '", def.name, "': type is required"));
      for (auto& s : nj.value(kKeyInputStreams, std::vector<std::string>{}))
        def.input_streams.push_back(std::move(s));
      for (auto& s : nj.value(kKeyOutputStreams, std::vector<std::string>{}))
        def.output_streams.push_back(std::move(s));
      for (auto& s : nj.value(kKeyInputSidePkts, std::vector<std::string>{}))
        def.input_side_packets.push_back(std::move(s));
      for (auto& s : nj.value(kKeyOutputSidePkts, std::vector<std::string>{}))
        def.output_side_packets.push_back(std::move(s));
      def.executor = nj.value(kKeyExecutor, "");
      def.input_stream_handler = nj.value(kKeyInputStreamHandler, "");
      def.max_in_flight = nj.value(kKeyMaxInFlight, kDefaultMaxInFlight);
      def.source_layer = nj.value(kKeySourceLayer, kDefaultSourceLayer);
      config.nodes.push_back(std::move(def));
    }
  }

  auto status = ConfigValidator::Validate(config);
  if (!status.ok()) return status;

  return config;
}

}  // namespace graph::runtime
