#include "src/framework/public/graph_builder.h"
#include "src/framework/public/graph_runtime.h"

#include "src/framework/config/config_validator.h"
#include "src/framework/scheduler/scheduler.h"
#include "src/framework/scheduler/thread_pool_executor.h"
#include "src/framework/node/node_registry.h"

namespace graph::runtime {

absl::StatusOr<std::unique_ptr<GraphRuntime>> GraphBuilder::Build(
    const GraphConfig& config) {
  // Validate config before any construction
  auto validation_status = ConfigValidator::Validate(config);
  if (!validation_status.ok()) return validation_status;

  auto runtime = absl::WrapUnique(new GraphRuntime());
  runtime->config_ = config;

  // Create executors
  for (const auto& edef : config.executors) {
    ExecutorOptions opts;
    opts.num_threads = edef.num_threads;
    auto executor = ThreadPoolExecutor::Create(opts);
    if (!executor.ok()) return executor.status();
    if (edef.name.empty()) {
      (void)runtime->scheduler_->SetDefaultExecutor(*executor);
    } else {
      (void)runtime->scheduler_->SetNonDefaultExecutor(edef.name, *executor);
    }
  }

  // If no executors defined, create a default one
  if (config.executors.empty()) {
    ExecutorOptions opts;
    opts.num_threads = std::min(
        static_cast<int>(std::thread::hardware_concurrency()),
        std::max(1, static_cast<int>(config.nodes.size())));
    auto executor = ThreadPoolExecutor::Create(opts);
    if (!executor.ok()) return executor.status();
    (void)runtime->scheduler_->SetDefaultExecutor(*executor);
  }

  // Create nodes via NodeFactoryRegistry
  for (const auto& ndef : config.nodes) {
    auto node = NodeFactoryRegistry::CreateByName(
        ndef.type, ndef.name, ndef.options);
    if (!node) {
      return absl::NotFoundError(
          absl::StrCat("Node type not registered: ", ndef.type));
    }
    node->SetExecutorName(ndef.executor);
    node->SetSourceLayer(ndef.source_layer);
    runtime->all_nodes_.push_back(std::move(node));
  }

  return runtime;
}

}  // namespace graph::runtime
