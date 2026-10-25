#include "catch2/catch.hpp"

#include <string>
#include <vector>

#include "cmd_args/cmd_args.hpp"
#include "cmd_args/expression/expression.h"

using namespace cmd_args;

/* ==================== Model 数据仓库查询 ==================== */

TEST_CASE("model: hasAction 随 actionOrder 翻转", "[model]")
{
    Model model;
    OptionBuilder ob(&model.opts);
    ob.addOption<std::string>("task", "t", "")->action("echo {value}");

    // 构筑后未解析：actionOrder 为空 → 无待执行 action
    REQUIRE_FALSE(model.hasAction());

    // 模拟解析期记录（Executor 顺序执行的数据来源）
    model.opts.recordParseOrder("task");
    REQUIRE(model.hasAction());

    model.opts.clearParseOrder();
    REQUIRE_FALSE(model.hasAction());
}

TEST_CASE("model: 失败解析后 actionOrder 清空，下次解析不触发陈旧 action", "[model]")
{
    Context app("ut_stale_action");
    std::vector<std::string> fired;
    app.addOption<std::string>("task", "k", "");
    app.option("task")->onSet([&](std::string const &_V) { fired.emplace_back(_V); });

    // 第一次解析：先命中 --task 再因未知选项失败
    std::error_code ec;
    char const *argv1[] = { "test", "--task", "one", "--bogus", nullptr };
    app.parse(ec, 4, argv1, nullptr);
    REQUIRE(ec);                                  // 解析失败
    REQUIRE(app.model().hasAction() == false);    // 失败即清空（不再残留待执行）

    // 第二次成功解析：只含无关内容，不得触发第一次残留的 task
    char const *argv2[] = { "test", nullptr };
    app.parse(ec, 1, argv2, nullptr);
    REQUIRE(!ec);
    REQUIRE(fired.empty());                       // 陈旧 action 未被执行
}

TEST_CASE("model: expressions() 返回统一 Expression 视图", "[model]")
{
    Model model;
    ArgumentBuilder ab(&model.args);
    ab.build<ArgumentNamed>("hello", "greeting");
    OptionBuilder ob(&model.opts);
    ob.addOption<int>("timeout", "sec", 30);
    EnvBuilder eb(&model.envs);
    eb.setEnv("MY_K", "MY_V");

    auto exprs = model.expressions();
    REQUIRE(exprs.size() == 3);

    size_t nArg = 0, nOpt = 0, nEnv = 0;
    for (auto const *e : exprs) {
        switch (e->kind()) {
        case ExpressionKind_E::Argument: ++nArg; break;
        case ExpressionKind_E::Option:   ++nOpt; break;
        case ExpressionKind_E::Env:      ++nEnv; break;
        }
    }
    REQUIRE(nArg == 1);
    REQUIRE(nOpt == 1);
    REQUIRE(nEnv == 1);
}

/* ==================== Executor 批量执行器 ==================== */

TEST_CASE("executor: Executor(Model*) 构造 + execute 执行 action", "[executor]")
{
    Model model;
    OptionBuilder ob(&model.opts);
    ob.addOption<std::string>("task", "t", "")
        ->onSet([&](std::string const &_V) {
            model.strProgramName = "executed:" + _V;
        });

    Executor exec(&model);  // 新构造签名

    // 模拟解析后状态：手动记录 actionOrder + 赋值
    model.opts.recordParseOrder("task");
    model.opts.mgrValues.mutFind("task")->setValue("build");
    model.opts.mgrValues.mutFind("task")->bExplicit__ = true;

    exec.execute();
    REQUIRE(model.strProgramName == "executed:build");
}
