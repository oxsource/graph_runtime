#include <chrono>
#include <thread>

#include "gtest/gtest.h"
#include "src/framework/scheduler/scheduler_queue.h"
#include "src/framework/scheduler/thread_pool_executor.h"
#include "src/framework/node/node.h"

namespace graph::runtime {

class TestNode2 : public Node {
 public:
  TestNode2(const std::string& name, const NodeOptions& options)
      : Node(name) {}
  static absl::Status GetContract(NodeContract* contract) {
    return absl::OkStatus();
  }
  absl::Status Open(GraphContext& context) override { return absl::OkStatus(); }
  absl::Status Process(GraphContext& context) override { return absl::OkStatus(); }
  absl::Status Close(GraphContext& context) override { return absl::OkStatus(); }
};

TEST(ExecutionInfraTest, SchedulerQueueAddNode) {
  SchedulerQueue queue("test");
  TestNode2 node("test_node", NodeOptions());
  queue.AddNode(&node);
  EXPECT_FALSE(queue.IsIdle());
}

TEST(ExecutionInfraTest, SchedulerQueueIdleAfterConsume) {
  SchedulerQueue queue("test");
  TestNode2 node("test_node", NodeOptions());

  bool became_idle = false;
  queue.SetIdleCallback([&](bool idle) {
    if (idle) became_idle = true;
  });

  queue.AddNode(&node);
  EXPECT_FALSE(queue.IsIdle());
  queue.RunNextTask();
}

TEST(ExecutionInfraTest, ThreadPoolExecutorCreatesAndSchedules) {
  ExecutorOptions opts;
  opts.num_threads = 2;
  auto executor = ThreadPoolExecutor::Create(opts);
  ASSERT_TRUE(executor.ok());

  bool task_ran = false;
  (*executor)->Schedule([&] { task_ran = true; });
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_TRUE(task_ran);
}

}  // namespace graph::runtime
