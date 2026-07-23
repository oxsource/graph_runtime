#include "gtest/gtest.h"
#include "graph_runtime/src/node/node.h"
#include "graph_runtime/src/node/node_factory.h"
#include "graph_runtime/src/node/node_registry.h"
#include "graph_runtime/src/node/node_contract.h"
#include "graph_runtime/src/node/graph_context.h"

namespace graph::runtime {

// A simple test Node for testing the factory and lifecycle
class TestNode : public Node {
 public:
  TestNode(const std::string& name, const NodeOptions& options)
      : Node(name) {}

  static absl::Status GetContract(NodeContract* contract) {
    contract->Inputs().Get("in").SetAny();
    contract->Outputs().Get("out").SetAny();
    return absl::OkStatus();
  }

  absl::Status Open(GraphContext& context) override {
    open_count_++;
    return absl::OkStatus();
  }
  absl::Status Process(GraphContext& context) override {
    process_count_++;
    return absl::OkStatus();
  }
  absl::Status Close(GraphContext& context) override {
    close_count_++;
    return absl::OkStatus();
  }

  int open_count_ = 0;
  int process_count_ = 0;
  int close_count_ = 0;
};

TEST(NodeChainTest, NodeFactoryRegistryRegisterAndCreate) {
  auto factory = std::make_unique<NodeFactoryFor<TestNode>>();
  NodeFactoryRegistry::Register("TestNode", std::move(factory));

  EXPECT_TRUE(NodeFactoryRegistry::IsRegistered("TestNode"));

  NodeOptions opts;
  auto node = NodeFactoryRegistry::CreateByName("TestNode", "node1", opts);
  ASSERT_NE(node, nullptr);
  EXPECT_EQ(node->name(), "node1");
}

TEST(NodeChainTest, NodeFactoryGetContract) {
  auto* factory = NodeFactoryRegistry::GetFactory("TestNode");
  ASSERT_NE(factory, nullptr);

  NodeContract contract;
  auto status = factory->GetContract(&contract);
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(contract.Inputs().NumEntries(), 1);
  EXPECT_EQ(contract.Outputs().NumEntries(), 1);
}

TEST(NodeChainTest, NodeLifecycle) {
  NodeOptions opts;
  auto node = NodeFactoryRegistry::CreateByName("TestNode", "lifecycle_test", opts);
  ASSERT_NE(node, nullptr);

  auto* test_node = dynamic_cast<TestNode*>(node.get());
  ASSERT_NE(test_node, nullptr);

  InputStreamShardSet inputs;
  OutputStreamShardSet outputs;
  GraphContext ctx("lifecycle_test", 0, "TestNode",
                   Timestamp::Unstarted(), inputs, outputs, &opts);

  auto status = node->Open(ctx);
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(test_node->open_count_, 1);

  GraphContext ctx2("lifecycle_test", 0, "TestNode",
                    Timestamp(100), inputs, outputs, &opts);
  status = node->Process(ctx2);
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(test_node->process_count_, 1);

  GraphContext ctx3("lifecycle_test", 0, "TestNode",
                    Timestamp::Done(), inputs, outputs, &opts);
  status = node->Close(ctx3);
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(test_node->close_count_, 1);
}

TEST(NodeChainTest, NodeFactoryRegistryRegisteredTypes) {
  auto types = NodeFactoryRegistry::RegisteredTypes();
  EXPECT_TRUE(std::find(types.begin(), types.end(), "TestNode") != types.end());
}

}  // namespace graph::runtime
