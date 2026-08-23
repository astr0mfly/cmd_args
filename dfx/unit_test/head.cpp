#define CATCH_CONFIG_MAIN
#include "catch2/catch.hpp"

#include <list>
#include <string>
#include <vector>

#include "cmd_args/cmd_args.hpp"

using namespace cmd_args;

/* ==================== 词法：Lexer / Scanner ==================== */

TEST_CASE("lexer: argv 原始 token 归一化", "[lexer]")
{
    char const *raw[] = { "-?", "--foo=bar", "-abc", "--", "a=b", "x.y", "-5", "", "   " };
    char       *envp[] = { nullptr };
    Lexer lx(9, raw, envp);

    auto toks = lx.tokens();
    REQUIRE(toks == Tokens_T{ "-?", "--foo=bar", "-abc", "--", "a=b", "x.y", "-5" });
}

TEST_CASE("lexer: 值选项取值保持原样（含 . = 等字符）", "[lexer]")
{
    char const *raw[] = { "-o", "out.txt", "--env-set", "A=1" };
    char       *envp[] = { nullptr };
    Lexer lx(4, raw, envp);

    auto toks = lx.tokens();
    REQUIRE(toks == Tokens_T{ "-o", "out.txt", "--env-set", "A=1" });
}

TEST_CASE("lexer: 空输入得到空 token 流", "[lexer]")
{
    char const *raw[] = { "" };
    char       *envp[] = { nullptr };
    Lexer lx(1, raw, envp);

    REQUIRE(lx.tokens().empty());
}

TEST_CASE("lexer: REPL 整行分词（空白分隔、--name=value 合并）", "[lexer]")
{
    REQUIRE(Lexer::tokensFromLine("hello world") == Tokens_T{ "hello", "world" });
    REQUIRE(Lexer::tokensFromLine("--a=1 b") == Tokens_T{ "--a=1", "b" });
    REQUIRE(Lexer::tokensFromLine("").empty());
}

TEST_CASE("lexer: 未闭合字符串报词法错误", "[lexer]")
{
    REQUIRE_THROWS_AS(Lexer::tokensFromLine("\"abc"), ErrorLexer);
    REQUIRE_THROWS_AS(Lexer::tokensFromLine("--a \"x"), ErrorLexer);
}

/* ==================== 构筑：重名检查 ==================== */

TEST_CASE("builder: 命名参数重名抛 DUPLICATE_NAME", "[builder]")
{
    Context app("ut");
    app.build<ArgumentNamed>("dup", "first");

    REQUIRE_THROWS_AS(app.build<ArgumentNamed>("dup", "second"), ErrorArgument);
}

TEST_CASE("builder: 选项重名抛 DUPLICATE_NAME", "[builder]")
{
    Context app("ut");
    app.add_option<int>("timeout", "timeout seconds", 1);
    REQUIRE_THROWS_AS(app.add_option<int>("timeout", "again", 2), ErrorArgument);

    Context app2("ut");
    app2.add_flag('v', "verbose", "verbose");
    REQUIRE_THROWS_AS(app2.add_option<int>('v', "volume", "conflict", 1), ErrorArgument);
}

/* ==================== 解析：argv 形式（含 argv[0] 路径不参与词法） ==================== */

TEST_CASE("context: argv 形式端到端解析", "[context]")
{
    Context app("ut_full");
    app.build<ArgumentNamed>("hello", "Say hello")
        .build<ArgumentNamed>("world", "Say world too")
        .build<ArgumentPosition>("name", "your name")
        .build<ArgumentPosition>("count", "count");
    app.add_option<std::string>('o', "output", "output file", "out.txt");
    app.add_option<int>("timeout", "timeout seconds", 30);
    app.add_flag('v', "verbose", "verbose");

    app.argument("world")->env("UT_E_WORLD");

    char const *argv[] = { "C:\\tools\\ut_demo.exe", "--hello", "hi", "--timeout", "99",
                           "-v", "peter", "3" };
    char *envp[] = { const_cast<char *>("UT_E_WORLD=from-env"), nullptr };

    std::error_code ec;
    app.parse(ec, 8, argv, envp);

    REQUIRE(!ec);
    REQUIRE(app.get_argument<std::string>("hello") == "hi");
    REQUIRE(app.get_argument<std::string>("world") == "from-env");  // 环境变量兜底
    REQUIRE(app.get_argument<std::string>("name") == "peter");
    REQUIRE(app.get_argument<std::string>("count") == "3");
    REQUIRE(app.get_option<std::string>("output") == "out.txt");  // 内置默认值
    REQUIRE(app.get_option<int>("timeout") == 99);                // 命令行显式值
    REQUIRE(app.get_option<bool>("verbose") == true);
}

/* ==================== 解析：token 形式 + parsed() ==================== */

TEST_CASE("context: token 形式 + parsed()/has_option", "[context]")
{
    Context app("ut_tok");
    app.build<ArgumentNamed>("u", "named u")
        .build<ArgumentPosition>("pos", "positional");
    app.add_option<std::string>('o', "output", "output file", "f.txt");
    app.add_option<int>("timeout", "timeout seconds", 1);
    app.add_flag('v', "verbose", "verbose");

    std::error_code ec;
    app.parse(ec, Tokens_T{ "--u", "cli", "--timeout=88", "posval" }, Envs_T{});

    REQUIRE(!ec);
    REQUIRE(app.parsed("u"));
    REQUIRE(app.parsed("timeout"));
    REQUIRE(app.parsed("output") == false);
    REQUIRE(app.parsed('v') == false);
    REQUIRE(app.has_option("timeout"));
    REQUIRE(app.has_option("v"));
    REQUIRE(app.get_argument<std::string>("u") == "cli");
    REQUIRE(app.get_argument<std::string>("pos") == "posval");
    REQUIRE(app.get_option<int>("timeout") == 88);
    REQUIRE(app.get_option<std::string>("output") == "f.txt");
}

TEST_CASE("context: -- 之后全部为方位参数（含 - 开头）", "[context]")
{
    Context app("ut_two_dash");
    app.build<ArgumentPosition>("a", "first").build<ArgumentPosition>("b", "second");

    std::error_code ec;
    app.parse(ec, Tokens_T{ "--", "-x", "@" }, Envs_T{});

    REQUIRE(!ec);
    REQUIRE(app.get_argument<std::string>("a") == "-x");
    REQUIRE(app.get_argument<std::string>("b") == "@");
}

/* ==================== 解析：错误码 ==================== */

TEST_CASE("parse: 错误码分类", "[error]")
{
    Context app("ut_err");
    app.build<ArgumentPosition>("p", "pos");
    app.add_flag('v', "verbose", "verbose");
    app.add_option<int>("timeout", "timeout", 1);

    std::error_code ec;

    app.parse(ec, Tokens_T{ "--nope", "x" }, Envs_T{});
    REQUIRE(ec == make_error_code(ParseError_E::UNKNOWN_OPTION));

    // 内置 -?/--help 等：默认开启，--help 命中即短路
    app.parse(ec, Tokens_T{ "--help" }, Envs_T{});
    REQUIRE(!ec);

    // 关闭内置选项必须发生在首次 parse 之前（注册后无法撤销）
    {
        Context closed("ut_closed");
        closed.build<ArgumentPosition>("p", "pos");
        closed.set_builtin_options(false);
        closed.parse(ec, Tokens_T{ "--help", "x" }, Envs_T{});
        REQUIRE(ec == make_error_code(ParseError_E::UNKNOWN_OPTION));
    }

    Context app1("ut_err1");
    app1.build<ArgumentPosition>("p", "p");
    app1.parse(ec, Tokens_T{ "only" }, Envs_T{});  // 需要 1 个方位参数 -> 恰好够
    REQUIRE(!ec);

    Context app2("ut_err2");
    app2.build<ArgumentPosition>("a", "a").build<ArgumentPosition>("b", "b");
    app2.parse(ec, Tokens_T{ "one" }, Envs_T{});  // 给 1 个，需要 2 个
    REQUIRE(ec == make_error_code(ParseError_E::UNKNWON_ARGS));

    Context app3("ut_err3");
    app3.add_option<int>("timeout", "timeout", 1);
    app3.parse(ec, Tokens_T{ "--timeout" }, Envs_T{});  // 选项缺值
    REQUIRE(ec == make_error_code(ParseError_E::NEED_VALUE));

    Context app4("ut_err4");
    app4.add_flag('v', "verbose", "verbose");
    app4.build<ArgumentPosition>("p", "pos");
    app4.parse(ec, Tokens_T{ "-vq", "x" }, Envs_T{});  // 聚合串里未知短选项
    REQUIRE(ec == make_error_code(ParseError_E::UNKNOWN_OPTION));
}

/* ==================== 环境变量：直接操作 environment_store ==================== */

TEST_CASE("env: 装载与展示格式（含标记）", "[env]")
{
    environment_store store;

    store.parse(Envs_T{ "A=1", "B=2" });
    REQUIRE(store.env_string("p") == "environments: p\nNo. : \"key\" = \"value\"\n"
                                     "0 : \"A\" = \"1\"\n1 : \"B\" = \"2\"\n");

    // 修改已有 -> *
    store.set("A", "9");
    REQUIRE(store.env_string("p").find("0* : \"A\" = \"9\"") != std::string::npos);

    // 新建 -> +
    store.set("C", "3");
    REQUIRE(store.env_string("p").find("2+ : \"C\" = \"3\"") != std::string::npos);

    // 读取 -> >
    REQUIRE(store.get("A") == "9");
    REQUIRE(store.env_string("p").find("0*> : \"A\" = \"9\"") != std::string::npos);

    REQUIRE(store.has("A"));
    REQUIRE_FALSE(store.has("NOPE"));
    REQUIRE(store.get_or("NOPE", "dflt") == "dflt");
    REQUIRE(store.size() == 3);
}

TEST_CASE("env: set/unset", "[env]")
{
    environment_store store;
    store.parse(Envs_T{ "A=1" });

    store.unset("A");
    REQUIRE_FALSE(store.has("A"));
    REQUIRE_FALSE(store.unset("A"));  // 已不存在

    store.set("A", "again");
    REQUIRE(store.has("A"));
    REQUIRE(store.get("A") == "again");
}

TEST_CASE("env: ${VAR} / %VAR% / $VAR 三种展开", "[env]")
{
    environment_store store;
    store.parse(Envs_T{ "NAME=world" });

    REQUIRE(store.expand("hello ${NAME}!") == "hello world!");
    REQUIRE(store.expand("%NAME%") == "world");
    REQUIRE(store.expand("$NAME") == "world");
    REQUIRE(store.expand("hi ${NAME}") == "hi world");

    // 未命中保持原样
    REQUIRE(store.expand("${NOPE}") == "${NOPE}");
    REQUIRE(store.expand("100%") == "100%");
    REQUIRE(store.expand(" $ ") == " $ ");

    // 命中后标记被引用
    store.expand("${NAME}");
    REQUIRE(store.env_string("p").find("0>") != std::string::npos);
}

TEST_CASE("env: apply() 回写 修改+新建，并清除标记", "[env]")
{
    environment_store store;
    store.parse(Envs_T{ "A=1" });

    store.set("A", "9");           // 修改已有
    store.set("C", "3");           // 新建
    auto applied = store.apply();

    std::vector<std::string> expect{ "A=9", "C=3" };
    REQUIRE(applied == expect);

    // 标记已清除，再次 apply 为空
    REQUIRE(store.apply().empty());
    REQUIRE(store.env_string("p").find('*') == std::string::npos);
    REQUIRE(store.env_string("p").find('+') == std::string::npos);
    REQUIRE(store.has("C"));
}

/* ==================== 环境变量：Context 层联动 ==================== */

TEST_CASE("context: 值优先级 命令行 > 环境变量 > 内置默认", "[env]")
{
    // 命令行显式值胜出
    Context app("ut_prec1");
    app.build<ArgumentNamed>("u", "u").build<ArgumentPosition>("p", "p");
    app.add_option<int>("timeout", "timeout", 30);
    app.argument("u")->env("UT_E_U");
    app.option("timeout")->env("UT_E_TIMEOUT");

    std::error_code ec;
    app.parse(ec, Tokens_T{ "--u", "cli", "--timeout", "77", "pos" },
              Envs_T{ "UT_E_U=env", "UT_E_TIMEOUT=9" });
    REQUIRE(!ec);
    REQUIRE(app.get_argument<std::string>("u") == "cli");
    REQUIRE(app.get_option<int>("timeout") == 77);

    // 环境变量胜初期内置默认
    Context app2("ut_prec2");
    app2.build<ArgumentNamed>("u", "u").build<ArgumentPosition>("p", "p");
    app2.add_option<int>("timeout", "timeout", 30);
    app2.argument("u")->env("UT_E_U");
    app2.option("timeout")->env("UT_E_TIMEOUT");

    app2.parse(ec, Tokens_T{ "pos" }, Envs_T{ "UT_E_U=env", "UT_E_TIMEOUT=9" });
    REQUIRE(!ec);
    REQUIRE(app2.get_argument<std::string>("u") == "env");
    REQUIRE(app2.get_option<int>("timeout") == 9);

    // 都没有 -> 内置默认
    Context app3("ut_prec3");
    app3.build<ArgumentPosition>("p", "p");
    app3.add_option<int>("timeout", "timeout", 30);
    app3.option("timeout")->env("UT_E_TIMEOUT");

    app3.parse(ec, Tokens_T{ "pos" }, Envs_T{});
    REQUIRE(!ec);
    REQUIRE(app3.get_option<int>("timeout") == 30);
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

/* ==================== 内置短回路选项（Parser 层 fired） ==================== */

TEST_CASE("parser: -?/--help、-!/--env 命中即短路", "[parser]")
{
    Model  model;
    Parser p(&model);
    p.set_program_name("ut_parser");

    Tokens_T help{ "--help" };
    auto ec = p.tryParse(help, Envs_T{});
    REQUIRE(!ec);
    REQUIRE(p.fired());

    Tokens_T env{ "-!" };
    ec = p.tryParse(env, Envs_T{});
    REQUIRE(!ec);
    REQUIRE(p.fired());

    Tokens_T plain;  // 空 token：无命中、不短路
    ec = p.tryParse(plain, Envs_T{});
    REQUIRE(!ec);
    REQUIRE_FALSE(p.fired());
}