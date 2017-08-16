#include "paddle/operators/net_op.h"

#include <gtest/gtest.h>

namespace paddle {
namespace operators {
using Scope = framework::Scope;
using DeviceContext = platform::DeviceContext;

static int infer_shape_cnt = 0;
static int run_cnt = 0;

class TestOp : public framework::OperatorBase {
 public:
  using framework::OperatorBase::OperatorBase;
  DEFINE_OP_CLONE_METHOD(TestOp);
  void InferShape(const Scope& scope) const override { ++infer_shape_cnt; }
  void Run(const Scope& scope,
           const platform::DeviceContext& dev_ctx) const override {
    ++run_cnt;
  }
};

class EmptyOp : public framework::OperatorBase {
 public:
  using framework::OperatorBase::OperatorBase;
  DEFINE_OP_CLONE_METHOD(EmptyOp);
  void InferShape(const Scope& scope) const override {}
  void Run(const Scope& scope, const DeviceContext& dev_ctx) const override {}
};

template <typename T>
void AssertSameVectorWithoutOrder(const std::vector<T>& expected,
                                  const std::vector<T>& actual) {
  ASSERT_EQ(expected.size(), actual.size());
  std::unordered_set<T> expected_set;
  for (auto& tmp : expected) {
    expected_set.insert(tmp);
  }
  for (auto& act : actual) {
    ASSERT_NE(expected_set.end(), expected_set.find(act));
  }
}

TEST(OpKernel, all) {
  auto net = std::make_shared<NetOp>();
  ASSERT_NE(net, nullptr);

  auto op1 = std::shared_ptr<TestOp>(
      new TestOp("test", {{"X", {"x"}}, {"W", {"w1"}}, {"b", {"b1"}}},
                 {{"Out", {"y"}}}, {}));
  net->AddOp(op1);

  auto op2 = std::shared_ptr<TestOp>(
      new TestOp("test", {{"X", {"y"}}, {"W", {"w2"}}, {"b", {"b2"}}},
                 {{"Out", {"z"}}}, {}));
  net->AddOp(op2);

  net->CompleteAddOp();
  AssertSameVectorWithoutOrder({"x", "w1", "b1", "w2", "b2"},
                               net->inputs_.at(NetOp::kAll));
  AssertSameVectorWithoutOrder({"y", "z"}, net->outputs_.at(NetOp::kAll));

  auto final_outs = net->OutputVars(false);

  ASSERT_EQ(final_outs.size(), 1UL);
  ASSERT_EQ(final_outs[0], "z");
}

TEST(NetOp, insert_op) {
  NetOp net;
  auto op1 = std::shared_ptr<EmptyOp>(
      new EmptyOp("empty", {{"X", {"x"}}, {"W", {"w1"}}, {"b", {"b1"}}},
                  {{"Out", {"y"}}}, {}));
  net.AddOp(op1);
  net.InsertOp(0, op1);
  ASSERT_EQ(2UL, net.ops_.size());
  net.InsertOp(2, op1);
  ASSERT_EQ(3UL, net.ops_.size());
}

TEST(NetOp, Clone) {
  NetOp net;
  net.AddOp(std::shared_ptr<EmptyOp>(new EmptyOp{"empty", {}, {}, {}}));
  net.AddOp(std::shared_ptr<EmptyOp>(new EmptyOp{"empty2", {}, {}, {}}));
  net.CompleteAddOp(true);
  auto new_net_op = net.Clone();
  ASSERT_NE(new_net_op, nullptr);
  ASSERT_TRUE(new_net_op->IsNetOp());
  auto* new_net = static_cast<NetOp*>(new_net_op.get());
  ASSERT_EQ(2, new_net->ops_.size());
  ASSERT_EQ(new_net->ops_[0]->Type(), "empty");
  ASSERT_EQ(new_net->ops_[1]->Type(), "empty2");
}

}  // namespace operators
}  // namespace paddle
