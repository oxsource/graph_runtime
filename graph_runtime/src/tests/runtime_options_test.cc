// Tests for the GraphRuntime::Options parameter-object pattern:
//   GraphRuntime::Initialize(config, options)
// with options defaulting to empty when omitted.
//
// Node options reach nodes via the constructor (GraphBuilder passes
// NodeDef::options to NodeFactoryRegistry::CreateByName), so the probe node
// below captures its options at construction time.

#include "src/framework/public/graph_runtime.h"

#include <string>

#include "gtest/gtest.h"
#include "src/framework/config/graph_config.h"
#include "src/framework/node/node.h"
#include "src/framework/node/node_contract.h"
#include "src/framework/node/node_factory.h"
#include "src/framework/node/node_registry.h"

namespace graph::runtime {

namespace {

// Captures the "probe" option handed to the node constructor.
std::string g_captured_probe;

class OptionsProbeNode : public Node {
 public:
  OptionsProbeNode(const std::string& name, const NodeOptions& options)
      : Node(name) {
    if (const auto* v = options.Get<std::string>("probe")) {
      g_captured_probe = *v;
    }
  }

  static absl::Status GetContract(NodeContract* c) { return absl::OkStatus(); }

  absl::Status Open(GraphContext&) override { return absl::OkStatus(); }
  absl::Status Process(GraphContext&) override { return absl::OkStatus(); }
  absl::Status Close(GraphContext&) override { return absl::OkStatus(); }
};

// Register once; unique type name avoids clashes with other test files.
bool RegisterProbeNode() {
  static const bool registered = []() {
    NodeFactoryRegistry::Register(
        "OptionsProbeNode", std::make_unique<NodeFactoryFor<OptionsProbeNode>>());
    return true;
  }();
  return registered;
}

GraphConfig SingleProbeNodeConfig() {
  GraphConfig config;
  config.nodes.push_back(
      {"n1", "OptionsProbeNode", {}, {}, {}, {}, {}, "", "", 1, 0});
  return config;
}

}  // namespace

class RuntimeOptionsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    RegisterProbeNode();
    g_captured_probe.clear();
  }
};

TEST_F(RuntimeOptionsTest, InitializeWithoutOptionsUsesDefaults) {
  GraphConfig config = SingleProbeNodeConfig();
  GraphRuntime runtime;

  // Single-argument form: default GraphRuntime::Options{} → no overrides.
  ASSERT_TRUE(runtime.Initialize(config).ok());
  EXPECT_TRUE(g_captured_probe.empty());
}

TEST_F(RuntimeOptionsTest, ExplicitOptionsApplyToMatchingNodeType) {
  GraphConfig config = SingleProbeNodeConfig();
  GraphRuntime runtime;

  GraphRuntime::Options opts;
  opts.nodes["OptionsProbeNode"].Set("probe", std::string("via_options"));
  ASSERT_TRUE(runtime.Initialize(config, opts).ok());
  EXPECT_EQ(g_captured_probe, "via_options");
}

TEST_F(RuntimeOptionsTest, ExplicitOptionsOverrideConfigOptions) {
  GraphConfig config = SingleProbeNodeConfig();
  config.nodes[0].options.Set("probe", std::string("from_config"));
  GraphRuntime runtime;

  GraphRuntime::Options opts;
  opts.nodes["OptionsProbeNode"].Set("probe", std::string("from_options"));
  ASSERT_TRUE(runtime.Initialize(config, opts).ok());
  EXPECT_EQ(g_captured_probe, "from_options");
}

TEST_F(RuntimeOptionsTest, UnrelatedOverridesDoNotLeak) {
  GraphConfig config = SingleProbeNodeConfig();
  GraphRuntime runtime;

  GraphRuntime::Options opts;
  opts.nodes["SomeOtherNode"].Set("probe", std::string("nope"));
  ASSERT_TRUE(runtime.Initialize(config, opts).ok());
  EXPECT_TRUE(g_captured_probe.empty());
}

TEST_F(RuntimeOptionsTest, ApplyPatchesMatchingNodes) {
  GraphConfig config = SingleProbeNodeConfig();
  GraphRuntime::Options opts;
  opts.nodes["OptionsProbeNode"].Set("probe", std::string("applied"));

  opts.Apply(&config);

  const auto* v = config.nodes[0].options.Get<std::string>("probe");
  ASSERT_NE(v, nullptr);
  EXPECT_EQ(*v, "applied");
}

TEST_F(RuntimeOptionsTest, ApplySkipsUnrelatedAndEmpty) {
  GraphConfig config = SingleProbeNodeConfig();

  // Unrelated node type: config untouched.
  GraphRuntime::Options unrelated;
  unrelated.nodes["OtherType"].Set("probe", std::string("x"));
  unrelated.Apply(&config);
  EXPECT_FALSE(config.nodes[0].options.Has("probe"));

  // Empty bundle: config untouched.
  GraphRuntime::Options empty;
  empty.Apply(&config);
  EXPECT_FALSE(config.nodes[0].options.Has("probe"));

  // Null config: no crash.
  empty.Apply(nullptr);
  SUCCEED();
}

TEST_F(RuntimeOptionsTest, InitializeWithOptionsIsIdempotent) {
  GraphConfig config = SingleProbeNodeConfig();
  GraphRuntime runtime;

  GraphRuntime::Options opts;
  opts.nodes["OptionsProbeNode"].Set("probe", std::string("first"));
  ASSERT_TRUE(runtime.Initialize(config, opts).ok());
  EXPECT_EQ(g_captured_probe, "first");

  // Second Initialize (e.g. graph re-use) reapplies the same options.
  g_captured_probe.clear();
  ASSERT_TRUE(runtime.Initialize(config, opts).ok());
  EXPECT_EQ(g_captured_probe, "first");
}

}  // namespace graph::runtime
