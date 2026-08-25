#include "catch2/catch.hpp"

#include <list>
#include <string>
#include <vector>

#include "cmd_args/cmd_args.hpp"

using namespace cmd_args;

/* ==================== OptionParser：token → Option(Expression) ==================== */

TEST_CASE("parser: 长选项双形态 + 短选项三形态入库为 Option Expression", "[parser]")
{
    Model model;
    OptionBuilder ob(&model.opts);
    ob.addFlag('a', "ena", "flag a");
    ob.addFlag('b', "enb", "flag b");
    ob.addOption<int>('t', "timeout", "timeout in seconds", 30);
    OptionParser op(&model.opts);

    // --long=value 内联形态
    Tokens_T t1{ "--timeout=9" };
    REQUIRE_FALSE(op.parse(t1));
    auto *p = model.opts.mgrValues.find("timeout");
    REQUIRE(p != nullptr);
    REQUIRE(p->kind() == ExpressionKind_E::Option);   // 入库即携带 Expression 视角
    REQUIRE(p->strValue == "9");
    REQUIRE(p->bExplicit__);
    REQUIRE(op.isParsed("timeout"));
    REQUIRE(t1.empty());                              // 已消费项移出 token 流

    // --long value 分离形态（消费下一个 token 作值）
    model.opts.mgrValues.forEachValue([](OptionValue &o) { o.reset(); });
    Tokens_T t2{ "--timeout", "42" };
    REQUIRE_FALSE(op.parse(t2));
    REQUIRE(model.opts.mgrValues.find("timeout")->strValue == "42");
    REQUIRE(t2.empty());

    // -x value 短名分离形态
    model.opts.mgrValues.forEachValue([](OptionValue &o) { o.reset(); });
    Tokens_T t3{ "-t", "7" };
    REQUIRE_FALSE(op.parse(t3));
    REQUIRE(model.opts.mgrValues.find("timeout")->strValue == "7");
    REQUIRE(op.isParsed('t'));

    // -x=value 短名内联形态（聚合路径吃 '=' 余串）
    model.opts.mgrValues.forEachValue([](OptionValue &o) { o.reset(); });
    Tokens_T t4{ "-t=5" };
    REQUIRE_FALSE(op.parse(t4));
    REQUIRE(model.opts.mgrValues.find("timeout")->strValue == "5");

    // 聚合 bool：-ab 全部置位
    Tokens_T t5{ "-ab" };
    REQUIRE_FALSE(op.parse(t5));
    REQUIRE(model.opts.mgrValues.find("ena")->strValue == "1");
    REQUIRE(model.opts.mgrValues.find("enb")->strValue == "1");
}

TEST_CASE("parser: 缺值报 NEED_VALUE（ErrorParse 异常通道）", "[parser]")
{
    Model model;
    OptionBuilder ob(&model.opts);
    ob.addOption<int>('t', "timeout", "timeout in seconds", 30);
    OptionParser op(&model.opts);

    Tokens_T t1{ "--timeout" };              // 流尾缺值
    REQUIRE_THROWS_AS(op.parse(t1), ErrorParse);

    Tokens_T t2{ "--timeout", "--other" };   // 下一个是 dash 开头 → 拒绝作值
    REQUIRE_THROWS_AS(op.parse(t2), ErrorParse);

    Tokens_T t3{ "-t" };                     // 短名流尾缺值
    REQUIRE_THROWS_AS(op.parse(t3), ErrorParse);
}

TEST_CASE("parser: 聚合串未知短选项当场报 UNKNOWN_OPTION", "[parser]")
{
    Model model;
    OptionBuilder ob(&model.opts);
    ob.addFlag('a', "ena", "flag a");
    OptionParser op(&model.opts);

    Tokens_T toks{ "-az" };                  // z 未注册
    REQUIRE_THROWS_AS(op.parse(toks), ErrorParse);
}

TEST_CASE("parser: 短回路命中立即回调并中止后续解析", "[parser]")
{
    Model model;
    OptionBuilder ob(&model.opts);
    ob.addOption<int>('t', "timeout", "timeout in seconds", 30);
    bool fired = false;
    ob.addShortCircuit('h', "help", "show help", [&fired] { fired = true; });
    OptionParser op(&model.opts);

    Tokens_T toks{ "--help", "--timeout", "9" };
    REQUIRE(op.parse(toks));                 // 返回值 = 是否命中短回路
    REQUIRE(fired);
    REQUIRE(op.isFired());
    // 命中即停止：后续 token 原样保留、未发生赋值
    REQUIRE(toks == Tokens_T{ "--timeout", "9" });
    REQUIRE(model.opts.mgrValues.find("timeout")->strValue == "30");
}

TEST_CASE("parser: -- 终止选项区且其后 token 原样保留", "[parser]")
{
    Model model;
    OptionBuilder ob(&model.opts);
    ob.addOption<int>('t', "timeout", "timeout in seconds", 30);
    OptionParser op(&model.opts);

    Tokens_T toks{ "--", "-x", "plain" };
    REQUIRE_FALSE(op.parse(toks));
    REQUIRE(op.isOptionsEnded());
    REQUIRE(toks == Tokens_T{ "-x", "plain" });   // '--' 自身被移除，其余不动
}

/* ==================== ArgumentParser：token → Argument(Expression) ==================== */

TEST_CASE("parser: 命名参数 + 方位参数一次解析入库", "[parser]")
{
    Model model;
    ArgumentBuilder ab(&model.args);
    ab.build<ArgumentNamed>("hello", "h");
    ab.build<ArgumentPosition>("dest", "target dir");
    ArgumentParser ap(&model.args);

    Tokens_T toks{ "--hello", "world", "dest" };
    ap.parse(toks);

    REQUIRE(ap.isParsed("hello"));
    REQUIRE(model.args.mgrName.doGet("hello")->getValue() == "world");
    REQUIRE(model.args.mgrPos.mutGetBySeq(0)->getValue() == "dest");
    REQUIRE(toks.empty());
}

TEST_CASE("parser: 方位参数数量越界抛 UNKNWON_ARGS", "[parser]")
{
    Model model;
    ArgumentBuilder ab(&model.args);
    ab.build<ArgumentPosition>("only", "the only positional");
    ArgumentParser ap(&model.args);

    Tokens_T toks{ "a", "b" };               // 1 个槽位收到 2 个 token
    REQUIRE_THROWS_AS(ap.parse(toks), ErrorParse);
}

TEST_CASE("parser: parsePositionalOnly 把 dash token 也当方位参数", "[parser]")
{
    Model model;
    ArgumentBuilder ab(&model.args);
    ab.build<ArgumentPosition>("weird", "dash-like value");
    ArgumentParser ap(&model.args);

    Tokens_T toks{ "-weird" };
    ap.parsePositionalOnly(toks);
    REQUIRE(model.args.mgrPos.mutGetBySeq(0)->getValue() == "-weird");
    REQUIRE(toks.empty());
}

TEST_CASE("parser: KEY=VALUE 行形态直接赋值命名参数", "[parser]")
{
    Model model;
    ArgumentBuilder ab(&model.args);
    ab.build<ArgumentNamed>("hello", "h");
    ArgumentParser ap(&model.args);

    ap.parse("hello=world");
    REQUIRE(model.args.mgrName.doGet("hello")->getValue() == "world");

    // 未注册键报错
    REQUIRE_THROWS_AS(ap.parse("nope=x"), ErrorParse);
}

/* ==================== EnvParser："KEY=VALUE" 串 → Env(Expression) ==================== */

TEST_CASE("parser: 环境串入库为 Env Expression 且 get/expand 标记引用", "[parser]")
{
    Model model;
    EnvParser ep(&model.envs);

    ep.parse(Envs_T{ "UT_ALPHA=1", "broken-line" });   // 无 '=' 的行忽略

    REQUIRE(ep.size() == 1);
    REQUIRE(ep.has("UT_ALPHA"));
    auto const *p = ep.find("UT_ALPHA");
    REQUIRE(p != nullptr);
    REQUIRE(p->kind() == ExpressionKind_E::Env);       // 入库即携带 Expression 视角
    REQUIRE(p->getName() == "UT_ALPHA");

    REQUIRE(ep.get("UT_ALPHA") == "1");
    REQUIRE(ep.referencedCount() == 1);                // get 标记"被引用"
    REQUIRE(ep.get("UT_MISSING").empty());             // 缺失返回空串

    REQUIRE(ep.expand("${UT_ALPHA}/log") == "1/log");  // 展开命中同一存储
}
