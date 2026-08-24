#include "catch2/catch.hpp"

#include <string>
#include <vector>

#include "cmd_args/cmd_args.hpp"

using namespace cmd_args;

/* ==================== 环境变量：EnvData / EnvBuilder / EnvParser 直接操作 ==================== */

TEST_CASE("env: 装载与展示格式（含标记）", "[env]")
{
    EnvData   d;
    EnvBuilder b(&d);
    EnvParser p(&d);

    p.parse(Envs_T{ "A=1", "B=2" });
    REQUIRE(p.envString("p") == "environments: p\nNo. : \"key\" = \"value\"\n"
                                "0 : \"A\" = \"1\"\n1 : \"B\" = \"2\"\n");

    // 修改已有 -> *
    b.setEnv("A", "9");
    REQUIRE(p.envString("p").find("0* : \"A\" = \"9\"") != std::string::npos);

    // 新建 -> +
    b.setEnv("C", "3");
    REQUIRE(p.envString("p").find("2+ : \"C\" = \"3\"") != std::string::npos);

    // 读取 -> >
    REQUIRE(p.get("A") == "9");
    REQUIRE(p.envString("p").find("0*> : \"A\" = \"9\"") != std::string::npos);

    REQUIRE(p.has("A"));
    REQUIRE_FALSE(p.has("NOPE"));
    REQUIRE(p.getOr("NOPE", "dflt") == "dflt");
    REQUIRE(p.size() == 3);
}

TEST_CASE("env: EnvData 存储/查询/遍历", "[env]")
{
    EnvData d;
    REQUIRE(d.doEmpty());
    REQUIRE(d.doSize() == 0);

    d.doPush("A", "1", false);
    d.doPush("B", "2", false);
    d.doPush(environment{ "A", "9", true });  // 键已存在：保持原条目
    REQUIRE_FALSE(d.doEmpty());
    REQUIRE(d.doSize() == 2);
    REQUIRE(d.doFind("A"));
    REQUIRE_FALSE(d.doFind("NOPE"));

    REQUIRE(d.doGet("A")->strValue == "1");   // 重复 push 保持首次生效
    REQUIRE(d.doErase("B"));
    REQUIRE_FALSE(d.doErase("B"));
    REQUIRE(d.doSize() == 1);

    std::size_t n = 0;
    d.doVisit([&](environment const &e) { ++n; REQUIRE(e.strKey == "A"); });
    REQUIRE(n == 1);
}

TEST_CASE("env: set/unset", "[env]")
{
    EnvData   d;
    EnvBuilder b(&d);
    EnvParser p(&d);

    p.parse(Envs_T{ "A=1" });

    REQUIRE(b.unsetEnv("A"));
    REQUIRE_FALSE(p.has("A"));
    REQUIRE_FALSE(b.unsetEnv("A"));  // 已不存在

    b.setEnv("A", "again");
    REQUIRE(p.has("A"));
    REQUIRE(p.get("A") == "again");
}

TEST_CASE("env: ${VAR} / %VAR% / $VAR 三种展开", "[env]")
{
    EnvData   d;
    EnvBuilder b(&d);
    EnvParser p(&d);

    p.parse(Envs_T{ "NAME=world" });

    REQUIRE(p.expand("hello ${NAME}!") == "hello world!");
    REQUIRE(p.expand("%NAME%") == "world");
    REQUIRE(p.expand("$NAME") == "world");
    REQUIRE(p.expand("hi ${NAME}") == "hi world");

    // 未命中保持原样
    REQUIRE(p.expand("${NOPE}") == "${NOPE}");
    REQUIRE(p.expand("100%") == "100%");
    REQUIRE(p.expand(" $ ") == " $ ");

    // 命中后标记被引用
    p.expand("${NAME}");
    REQUIRE(p.envString("p").find("0>") != std::string::npos);
}

TEST_CASE("env: apply() 回写 修改+新建，并清除标记", "[env]")
{
    EnvData   d;
    EnvBuilder b(&d);
    EnvParser p(&d);

    p.parse(Envs_T{ "A=1" });

    b.setEnv("A", "9");           // 修改已有
    b.setEnv("C", "3");           // 新建
    auto applied = p.apply();

    std::vector<std::string> expect{ "A=9", "C=3" };
    REQUIRE(applied == expect);

    // 标记已清除，再次 apply 为空
    REQUIRE(p.apply().empty());
    REQUIRE(p.envString("p").find('*') == std::string::npos);
    REQUIRE(p.envString("p").find('+') == std::string::npos);
    REQUIRE(p.has("C"));
}

/* ==================== 环境变量：Context 层联动 ==================== */

TEST_CASE("context: 值优先级 命令行 > 环境变量 > 内置默认", "[env]")
{
    // 命令行显式值胜出
    Context app("ut_prec1");
    app.build<ArgumentNamed>("u", "u").build<ArgumentPosition>("p", "p");
    app.addOption<int>("timeout", "timeout", 30);
    app.argument("u")->env("UT_E_U");
    app.option("timeout")->env("UT_E_TIMEOUT");

    std::error_code ec;
    app.parse(ec, Tokens_T{ "--u", "cli", "--timeout", "77", "pos" },
              Envs_T{ "UT_E_U=env", "UT_E_TIMEOUT=9" });
    REQUIRE(!ec);
    REQUIRE(app.getArgument<std::string>("u") == "cli");
    REQUIRE(app.getOption<int>("timeout") == 77);

    // 环境变量胜初期内置默认
    Context app2("ut_prec2");
    app2.build<ArgumentNamed>("u", "u").build<ArgumentPosition>("p", "p");
    app2.addOption<int>("timeout", "timeout", 30);
    app2.argument("u")->env("UT_E_U");
    app2.option("timeout")->env("UT_E_TIMEOUT");

    app2.parse(ec, Tokens_T{ "pos" }, Envs_T{ "UT_E_U=env", "UT_E_TIMEOUT=9" });
    REQUIRE(!ec);
    REQUIRE(app2.getArgument<std::string>("u") == "env");
    REQUIRE(app2.getOption<int>("timeout") == 9);

    // 都没有 -> 内置默认
    Context app3("ut_prec3");
    app3.build<ArgumentPosition>("p", "p");
    app3.addOption<int>("timeout", "timeout", 30);
    app3.option("timeout")->env("UT_E_TIMEOUT");

    app3.parse(ec, Tokens_T{ "pos" }, Envs_T{});
    REQUIRE(!ec);
    REQUIRE(app3.getOption<int>("timeout") == 30);
}

TEST_CASE("context: --env-set / --env-apply 内置交互", "[env]")
{
    Context app("ut_env_opt");
    app.build<ArgumentPosition>("p", "p");

    std::error_code ec;
    // --env-set 新建会话变量（= 形式），再 --env-apply 回写
    app.parse(ec, Tokens_T{ "--env-set=UT_VAR_S=1", "x" }, Envs_T{});
    REQUIRE(!ec);
    REQUIRE(app.get_env("UT_VAR_S") == "1");
    REQUIRE(app.env_string().find("+ : \"UT_VAR_S\" = \"1\"") != std::string::npos);

    size_t n = app.apply_env();
    REQUIRE(n >= 1);
    REQUIRE(app.apply_env() == 0);  // 标记已清除

    // 空格形式 --env-set KEY=VALUE
    Context app2("ut_env_opt2");
    app2.build<ArgumentPosition>("p", "p");
    app2.parse(ec, Tokens_T{ "--env-set", "UT_VAR_T=two", "y" }, Envs_T{});
    REQUIRE(!ec);
    REQUIRE(app2.get_env("UT_VAR_T") == "two");
}

TEST_CASE("context: 内置 --env-set 在错误行上副作用已生效（解析期即时语义）", "[context]")
{
    Context app("ut_builtin_errside");
    std::error_code ec;
    char const *argv[] = { "test", "--env-set", "UT_BUILTIN_ERR=1", "--bogus", nullptr };
    app.parse(ec, 4, argv, nullptr);
    REQUIRE(ec);                                       // 行解析失败
    REQUIRE(app.get_env("UT_BUILTIN_ERR") == "1");     // 但内置选项副作用已生效
}

TEST_CASE("context: expand_env / has_env / get_env_or", "[env]")
{
    Context app("ut_expand");
    app.build<ArgumentPosition>("p", "p");
    app.set_env("UT_E_X", "z");

    REQUIRE(app.has_env("UT_E_X"));
    REQUIRE(app.get_env("UT_E_X") == "z");
    REQUIRE(app.get_env_or("UT_E_MISS", "dflt") == "dflt");
    REQUIRE(app.expand_env("${UT_E_X}") == "z");
    REQUIRE(app.expand_env("%UT_E_X%") == "z");
    REQUIRE(app.expand_env("$UT_E_X") == "z");
    REQUIRE(app.expand_env("${UT_E_MISS}") == "${UT_E_MISS}");
}

TEST_CASE("context: env_string 头部与标记", "[env]")
{
    Context app("ut_disp");
    app.build<ArgumentPosition>("p", "p");
    app.set_env("A", "1");
    app.set_env("B", "2");
    (void)app.get_env("A");  // 标记被引用

    std::string s = app.env_string();
    REQUIRE(s.find("environments: ut_disp") == 0);
    REQUIRE(s.find("No. : \"key\" = \"value\"") != std::string::npos);
    REQUIRE(s.find("0>+ : \"A\" = \"1\"") != std::string::npos);
    REQUIRE(s.find("1+ : \"B\" = \"2\"") != std::string::npos);
}
