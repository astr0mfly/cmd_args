#include "catch2/catch.hpp"

#include "cmd_args/cmd_args.hpp"
#include "cmd_args/context/expression.h"

using namespace cmd_args;

/* ==================== Expression 类层次 ==================== */

TEST_CASE("expression: 构造 + kind/getName/getHelp 基本访问", "[expression]")
{
    Expression e(ExpressionKind_E::Argument, "hello", "say hello");
    REQUIRE(e.kind() == ExpressionKind_E::Argument);
    REQUIRE(e.getName() == "hello");
    REQUIRE(e.getHelp() == "say hello");

    Expression e2(ExpressionKind_E::Option, "timeout", "seconds");
    REQUIRE(e2.kind() == ExpressionKind_E::Option);
    REQUIRE(e2.getName() == "timeout");

    Expression e3(ExpressionKind_E::Env, "MY_VAR");
    REQUIRE(e3.kind() == ExpressionKind_E::Env);
    REQUIRE(e3.getName() == "MY_VAR");
    REQUIRE(e3.getHelp().empty());  // help 可选
}

TEST_CASE("expression: Argument kind() 返回 Argument", "[expression]")
{
    Context app("ut_expr_arg");
    app.build<ArgumentNamed>("hello", "say hello");

    auto *pNamed = app.argument("hello");
    REQUIRE(pNamed != nullptr);
    REQUIRE(pNamed->kind() == ExpressionKind_E::Argument);
    REQUIRE(pNamed->getName() == "hello");
    REQUIRE(pNamed->getHelp() == "say hello");

    // 方位参数：Context::argument() 仅覆盖命名参数，经公开的 Model/ArgumentBuilder 访问
    Model        model;
    ArgumentBuilder ab(&model.args);
    ab.build<ArgumentPosition>("name", "your name");

    auto *pPos = model.args.mgrPos.doGet("name");
    REQUIRE(pPos != nullptr);
    REQUIRE(pPos->kind() == ExpressionKind_E::Argument);
}

TEST_CASE("expression: Option/OptionValue/OptionAction kind() 返回 Option", "[expression]")
{
    // Option 基类：构造即携带 kind=Option，getName/getHelp 覆盖为自有存储
    Option opt('t', "timeout", "seconds");
    REQUIRE(opt.kind() == ExpressionKind_E::Option);
    REQUIRE(opt.getName() == "timeout");
    REQUIRE(opt.getHelp() == "seconds");

    // OptionValue：Context::option() 返回的句柄（OptionValue → Option → Expression）
    Context app("ut_expr_opt");
    app.addOption<int>("timeout", "timeout seconds", 30);
    auto *pVal = app.option("timeout");
    REQUIRE(pVal != nullptr);
    REQUIRE(pVal->kind() == ExpressionKind_E::Option);
    REQUIRE(pVal->getName() == "timeout");
    REQUIRE(pVal->getHelp() == "timeout seconds");

    // OptionAction：短回路选项（经 Model + OptionBuilder 构筑，mgrActions 只读访问）
    Model         model;
    OptionBuilder ob(&model.opts);
    ob.addShortCircuit('h', "help", "show help", [] {});
    auto const *pAct = model.opts.mgrActions.doGet("help");
    REQUIRE(pAct != nullptr);
    REQUIRE(pAct->kind() == ExpressionKind_E::Option);
    REQUIRE(pAct->getName() == "help");
    REQUIRE(pAct->getHelp() == "show help");
}

TEST_CASE("expression: environment kind() 返回 Env", "[expression]")
{
    Model model;
    EnvBuilder eb(&model.envs);
    eb.setEnv("MY_KEY", "my_value");

    auto const *p = model.envs.doGet("MY_KEY");
    REQUIRE(p != nullptr);
    REQUIRE(p->kind() == ExpressionKind_E::Env);
    REQUIRE(p->getName() == "MY_KEY");
}
