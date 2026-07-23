#include "src/config/json/json_parser.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "nlohmann/json.hpp"
#include "src/config/config_validator.h"

namespace graph::runtime {

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
  config.max_queue_size = root.value("max_queue_size", 100);
  config.report_deadlock = root.value("report_deadlock", false);

  for (auto& s : root.value("input_streams", std::vector<std::string>{}))
    config.input_streams.push_back(std::move(s));
  for (auto& s : root.value("output_streams", std::vector<std::string>{}))
    config.output_streams.push_back(std::move(s));

  // Parse executors
  if (root.contains("executors")) {
    for (const auto& ej : root["executors"]) {
      GraphConfig::ExecutorDef def;
      def.name = ej.value("name", "");
      def.type = ej.value("type", "ThreadPoolExecutor");
      def.num_threads = ej.value("num_threads", 0);
      config.executors.push_back(std::move(def));
    }
  }

  // Parse nodes
  if (root.contains("nodes")) {
    for (const auto& nj : root["nodes"]) {
      GraphConfig::NodeDef def;
      def.name = nj.value("name", "");
      if (def.name.empty())
        return absl::InvalidArgumentError("node name is required");
      def.type = nj.value("type", "");
      if (def.type.empty())
        return absl::InvalidArgumentError(
            absl::StrCat("node '", def.name, "': type is required"));
      for (auto& s : nj.value("input_streams", std::vector<std::string>{}))
        def.input_streams.push_back(std::move(s));
      for (auto& s : nj.value("output_streams", std::vector<std::string>{}))
        def.output_streams.push_back(std::move(s));
      for (auto& s : nj.value("input_side_packets", std::vector<std::string>{}))
        def.input_side_packets.push_back(std::move(s));
      for (auto& s : nj.value("output_side_packets", std::vector<std::string>{}))
        def.output_side_packets.push_back(std::move(s));
      def.executor = nj.value("executor", "");
      def.input_stream_handler = nj.value("input_stream_handler", "");
      def.max_in_flight = nj.value("max_in_flight", 1);
      def.source_layer = nj.value("source_layer", 0);
      config.nodes.push_back(std::move(def));
    }
  }

  // Parse streams
  if (root.contains("streams")) {
    for (const auto& sj : root["streams"]) {
      GraphConfig::StreamDef def;
      def.name = sj.value("name", "");
      if (def.name.empty())
        return absl::InvalidArgumentError("stream name is required");
      def.source_node = sj.value("source_node", "");
      def.source_port = sj.value("source_port", "");
      def.dest_node = sj.value("dest_node", "");
      def.dest_port = sj.value("dest_port", "");
      config.streams.push_back(std::move(def));
    }
  }

  auto status = ConfigValidator::Validate(config);
  if (!status.ok()) return status;

  return config;
}

}  // namespace graph::runtime
