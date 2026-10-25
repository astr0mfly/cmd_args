#include "catch2/catch.hpp"

#include <string>

#include "cmd_args/cmd_args.hpp"
#include "cmd_args/base/util.h"

using namespace cmd_args;

/* ==================== Property：值 / 默认值 / 版本号 ==================== */

TEST_CASE("property: 初始值同时成为默认值，未显式赋值时 isSet=false", "[property]")
{
    Property<std::string> prop("d");
    REQUIRE(prop.raw() == "d");
    REQUIRE(prop.defaultValue() == "d");
    REQUIRE(prop.get() == "d");
    REQUIRE_FALSE(prop.isSet());
    REQUIRE(prop.version() == 0);
    REQUIRE(prop.lastError().empty());

    REQUIRE(prop.set("a"));
    REQUIRE(prop.raw() == "a");
    REQUIRE(prop.isSet());
    REQUIRE(prop.version() == 1);

    REQUIRE(prop.set("a"));            // 同值：接受，但值不变、版本不涨
    REQUIRE(prop.version() == 1);

    REQUIRE(prop.set("b"));
    REQUIRE(prop.version() == 2);
}

TEST_CASE("property: 写钩子拒绝时不改值/不涨版本/不触发变更", "[property]")
{
    Property<std::string> prop("keep");
    int nChanges = 0;
    prop.onChange([&](std::string const &, std::string const &) { ++nChanges; });
    prop.onWrite([](std::string &_V) -> std::string {
        if (_V == "bad") { return "rejected: bad"; }
        return {};
    });

    REQUIRE_FALSE(prop.set("bad"));
    REQUIRE(prop.raw() == "keep");
    REQUIRE(prop.version() == 0);
    REQUIRE(nChanges == 0);
    REQUIRE_FALSE(prop.isSet());
    REQUIRE(prop.lastError() == "rejected: bad");

    // 成功写入清空错误并触发一次变更
    REQUIRE(prop.set("ok"));
    REQUIRE(prop.lastError().empty());
    REQUIRE(prop.version() == 1);
    REQUIRE(nChanges == 1);
}

TEST_CASE("property: 写钩子可原地规范化候选值", "[property]")
{
    Property<std::string> prop;
    prop.onWrite([](std::string &_V) -> std::string {
        _V = util::trim(_V);
        return {};
    });

    REQUIRE(prop.set("  v  "));
    REQUIRE(prop.raw() == "v");
}

TEST_CASE("property: 读钩子按注册顺序链式变换且不改存储值", "[property]")
{
    Property<std::string> prop("raw");
    prop.onRead([](std::string const &_V) { return "<" + _V + ">"; });
    prop.onRead([](std::string const &_V) { return _V + "!"; });

    REQUIRE(prop.get() == "<raw>!");
    REQUIRE(prop.raw() == "raw");
    REQUIRE(prop.version() == 0);
}

TEST_CASE("property: 默认值同步受 isSet 守护，reset 还原并清错误", "[property]")
{
    Property<std::string> prop("d");
    int nChanges = 0;
    prop.onChange([&](std::string const &, std::string const &) { ++nChanges; });

    prop.defaultValue("x");            // 未显式赋值 → 当前值同步
    REQUIRE(prop.raw() == "x");
    REQUIRE(prop.version() == 1);
    REQUIRE_FALSE(prop.isSet());

    prop.set("y");
    REQUIRE(prop.version() == 2);
    prop.defaultValue("z");            // 已显式赋值 → 不同步当前值
    REQUIRE(prop.raw() == "y");
    REQUIRE(prop.defaultValue() == "z");

    prop.reset();
    REQUIRE(prop.raw() == "z");
    REQUIRE_FALSE(prop.isSet());
    REQUIRE(prop.version() == 3);
    REQUIRE(nChanges == 3);

    // 惰性默认值：defaultFn 保存后立即求值，reset 时重新求值
    prop.defaultFn([] { return std::string("fn"); });
    REQUIRE(prop.defaultValue() == "fn");
    REQUIRE(prop.raw() == "fn");       // reset 后仍是未赋值状态 → 同步

    prop.set("manual");
    prop.defaultFn([] { return std::string("fn2"); });
    REQUIRE(prop.raw() == "manual");   // 已显式赋值 → 不覆盖
    prop.reset();
    REQUIRE(prop.raw() == "fn2");

    prop.onWrite([](std::string &) -> std::string { return "no"; });
    REQUIRE_FALSE(prop.set("anything"));
    REQUIRE(prop.lastError() == "no");
    prop.reset();
    REQUIRE(prop.lastError().empty());
}

/* ==================== Argument 读写拦截 ==================== */

TEST_CASE("property: Argument 写钩子拒绝经 parse 错误码通道返回 VALIDATION_FAILED", "[property][argument]")
{
    Context app("ut_prop_arg_write");
    app.build<ArgumentNamed>("name", "n");
    auto *arg = app.argument("name");
    REQUIRE(arg != nullptr);
    arg->property().onWrite([](std::string &_V) -> std::string {
        if (_V == "bad") { return "name rejected"; }
        return {};
    });

    std::error_code ec;
    app.parse(ec, Tokens_T{ "--name", "bad" }, Envs_T{});
    REQUIRE(ec.value() == static_cast<int>(ValidationError_E::VALIDATION_FAILED));
    REQUIRE(app.getLastVal() == static_cast<int>(ValidationError_E::VALIDATION_FAILED));
    REQUIRE(arg->getValue().empty());
    REQUIRE(arg->property().lastError() == "name rejected");

    app.reset();
    app.parse(ec, Tokens_T{ "--name", "ok" }, Envs_T{});
    REQUIRE_FALSE(ec);
    REQUIRE(app.getArgument<std::string>("name") == "ok");
    REQUIRE(arg->property().lastError().empty());

    // 直接 set 同样被拦截
    REQUIRE_THROWS_AS(arg->set("bad"), ErrorValidation);
    REQUIRE(arg->property().raw() == "ok");

    // setExternal 走 setRaw 旁路写钩子（配置导入语义）
    arg->setExternal("bad");
    REQUIRE(arg->property().raw() == "bad");
}

TEST_CASE("property: Argument 读钩子拦截取值且不影响存储值", "[property][argument]")
{
    Context app("ut_prop_arg_read");
    app.build<ArgumentNamed>("who", "w");
    auto *arg = app.argument("who");
    REQUIRE(arg != nullptr);
    arg->property().onRead([](std::string const &_V) {
        return _V.empty() ? std::string("(anonymous)") : _V;
    });

    REQUIRE(arg->getValue() == "(anonymous)");
    REQUIRE(arg->property().raw().empty());
    REQUIRE(app.getArgument<std::string>("who") == "(anonymous)");

    std::error_code ec;
    app.parse(ec, Tokens_T{ "--who", "bob" }, Envs_T{});
    REQUIRE_FALSE(ec);
    REQUIRE(app.getArgument<std::string>("who") == "bob");
    REQUIRE(arg->property().raw() == "bob");
}

/* ==================== OptionValue 读写拦截 ==================== */

TEST_CASE("property: OptionValue 写/读钩子与 setExternal 旁路", "[property][option]")
{
    Context app("ut_prop_opt");
    app.addOption<std::string>('n', "name", "name", "");
    app.option("name")->property().onWrite([](std::string &_V) -> std::string {
        if (_V == "bad") { return "option rejected"; }
        return {};
    });
    app.option("name")->property().onRead([](std::string const &_V) {
        return _V.empty() ? std::string("(none)") : _V;
    });

    std::error_code ec;
    app.parse(ec, Tokens_T{ "--name", "bad" }, Envs_T{});
    REQUIRE(ec.value() == static_cast<int>(ValidationError_E::VALIDATION_FAILED));
    // 注：解析期会懒注册内置选项，ValueOptMgr 的 vector 可能搬家，指针须重新取
    REQUIRE(app.option("name")->property().lastError() == "option rejected");
    REQUIRE(app.getOption<std::string>("name") == "(none)");

    app.reset();
    app.parse(ec, Tokens_T{ "--name", "ok" }, Envs_T{});
    REQUIRE_FALSE(ec);
    REQUIRE(app.getOption<std::string>("name") == "ok");

    // setExternal 旁路写钩子；setValue 仍被拦截
    app.option("name")->setExternal("bad");
    REQUIRE(app.option("name")->property().raw() == "bad");
    REQUIRE_THROWS_AS(app.option("name")->setValue("bad"), ErrorValidation);
}

/* ==================== 数据源：env seed 过钩子 / ${} 展开旁路 ==================== */

TEST_CASE("property: env 兜底 seed 过写钩子，${} 展开走旁路", "[property][env]")
{
    Context app("ut_prop_env");
    app.build<ArgumentNamed>("name", "n");
    app.argument("name")->env("UT_PROP_NAME");
    app.argument("name")->property().onWrite([](std::string &_V) -> std::string {
        if (_V == "bad") { return "env rejected"; }
        if (_V == "expanded") { return "expanded rejected"; }
        return {};
    });

    std::error_code ec;
    app.parse(ec, Tokens_T{}, Envs_T{ "UT_PROP_NAME=bad" });
    REQUIRE(ec.value() == static_cast<int>(ValidationError_E::VALIDATION_FAILED));
    REQUIRE(app.getLastVal() == static_cast<int>(ValidationError_E::VALIDATION_FAILED));

    app.reset();
    app.parse(ec, Tokens_T{}, Envs_T{ "UT_PROP_NAME=good" });
    REQUIRE_FALSE(ec);
    REQUIRE(app.getArgument<std::string>("name") == "good");
    REQUIRE(app.argument("name")->isEnvHit());

    // CLI 写入模板串通过准入，展开结果经 setRaw 旁路写钩子
    app.reset();
    app.parse(ec, Tokens_T{ "--name", "${UT_PROP_NAME}" }, Envs_T{ "UT_PROP_NAME=expanded" });
    REQUIRE_FALSE(ec);
    REQUIRE(app.getArgument<std::string>("name") == "expanded");
    REQUIRE(app.argument("name")->property().lastError().empty());
}
