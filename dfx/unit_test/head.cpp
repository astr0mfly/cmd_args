#define CATCH_CONFIG_MAIN
#include "catch2/catch.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <list>
#include <sstream>
#include <string>
#include <vector>

#include "cmd_args/cmd_args.hpp"
#include "cmd_args/context/expression.h"

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
    app.addOption<int>("timeout", "timeout seconds", 1);
    REQUIRE_THROWS_AS(app.addOption<int>("timeout", "again", 2), ErrorArgument);

    Context app2("ut");
    app2.addFlag('v', "verbose", "verbose");
    REQUIRE_THROWS_AS(app2.addOption<int>('v', "volume", "conflict", 1), ErrorArgument);
}

/* ==================== 解析：argv 形式（含 argv[0] 路径不参与词法） ==================== */

TEST_CASE("context: argv 形式端到端解析", "[context]")
{
    Context app("ut_full");
    app.build<ArgumentNamed>("hello", "Say hello")
        .build<ArgumentNamed>("world", "Say world too")
        .build<ArgumentPosition>("name", "your name")
        .build<ArgumentPosition>("count", "count");
    app.addOption<std::string>('o', "output", "output file", "out.txt");
    app.addOption<int>("timeout", "timeout seconds", 30);
    app.addFlag('v', "verbose", "verbose");

    app.argument("world")->env("UT_E_WORLD");

    char const *argv[] = { "C:\\tools\\ut_demo.exe", "--hello", "hi", "--timeout", "99",
                           "-v", "peter", "3" };
    char *envp[] = { const_cast<char *>("UT_E_WORLD=from-env"), nullptr };

    std::error_code ec;
    app.parse(ec, 8, argv, envp);

    REQUIRE(!ec);
    REQUIRE(app.getArgument<std::string>("hello") == "hi");
    REQUIRE(app.getArgument<std::string>("world") == "from-env");  // 环境变量兜底
    REQUIRE(app.getArgument<std::string>("name") == "peter");
    REQUIRE(app.getArgument<std::string>("count") == "3");
    REQUIRE(app.getOption<std::string>("output") == "out.txt");  // 内置默认值
    REQUIRE(app.getOption<int>("timeout") == 99);                // 命令行显式值
    REQUIRE(app.getOption<bool>("verbose") == true);
}

/* ==================== 解析：token 形式 + isParsed() ==================== */

TEST_CASE("context: token 形式 + isParsed()/hasOption", "[context]")
{
    Context app("ut_tok");
    app.build<ArgumentNamed>("u", "named u")
        .build<ArgumentPosition>("pos", "positional");
    app.addOption<std::string>('o', "output", "output file", "f.txt");
    app.addOption<int>("timeout", "timeout seconds", 1);
    app.addFlag('v', "verbose", "verbose");

    std::error_code ec;
    app.parse(ec, Tokens_T{ "--u", "cli", "--timeout=88", "posval" }, Envs_T{});

    REQUIRE(!ec);
    REQUIRE(app.isParsed("u"));
    REQUIRE(app.isParsed("timeout"));
    REQUIRE(app.isParsed("output") == false);
    REQUIRE(app.isParsed('v') == false);
    REQUIRE(app.hasOption("timeout"));
    REQUIRE(app.hasOption("v"));
    REQUIRE(app.getArgument<std::string>("u") == "cli");
    REQUIRE(app.getArgument<std::string>("pos") == "posval");
    REQUIRE(app.getOption<int>("timeout") == 88);
    REQUIRE(app.getOption<std::string>("output") == "f.txt");
}

TEST_CASE("context: -- 之后全部为方位参数（含 - 开头）", "[context]")
{
    Context app("ut_two_dash");
    app.build<ArgumentPosition>("a", "first").build<ArgumentPosition>("b", "second");

    std::error_code ec;
    app.parse(ec, Tokens_T{ "--", "-x", "@" }, Envs_T{});

    REQUIRE(!ec);
    REQUIRE(app.getArgument<std::string>("a") == "-x");
    REQUIRE(app.getArgument<std::string>("b") == "@");
}

/* ==================== 解析：错误码 ==================== */

TEST_CASE("parse: 错误码分类", "[error]")
{
    Context app("ut_err");
    app.build<ArgumentPosition>("p", "pos");
    app.addFlag('v', "verbose", "verbose");
    app.addOption<int>("timeout", "timeout", 1);

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
    app3.addOption<int>("timeout", "timeout", 1);
    app3.parse(ec, Tokens_T{ "--timeout" }, Envs_T{});  // 选项缺值
    REQUIRE(ec == make_error_code(ParseError_E::NEED_VALUE));

    Context app4("ut_err4");
    app4.addFlag('v', "verbose", "verbose");
    app4.build<ArgumentPosition>("p", "pos");
    app4.parse(ec, Tokens_T{ "-vq", "x" }, Envs_T{});  // 聚合串里未知短选项
    REQUIRE(ec == make_error_code(ParseError_E::UNKNOWN_OPTION));
}

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

/* ==================== 内置短回路选项（Parser 层 isFired） ==================== */

TEST_CASE("parser: -?/--help、-!/--env 命中即短路", "[parser]")
{
    Model  model;
    Parser p(&model);
    p.set_program_name("ut_parser");

    Tokens_T help{ "--help" };
    auto ec = p.tryParse(help, Envs_T{});
    REQUIRE(!ec);
    REQUIRE(p.isFired());

    Tokens_T env{ "-!" };
    ec = p.tryParse(env, Envs_T{});
    REQUIRE(!ec);
    REQUIRE(p.isFired());

    Tokens_T plain;  // 空 token：无命中、不短路
    ec = p.tryParse(plain, Envs_T{});
    REQUIRE(!ec);
    REQUIRE_FALSE(p.isFired());
}

/* ==================== 配置脚本（YAML）：三类外界环境统一抽象 ==================== */

TEST_CASE("yaml: dump/parse 往返（注释、引号、转义、空值）", "[config]")
{
    yaml_doc doc;
    doc.set("a", "hello world");
    doc.set("b", "say \"hi\"");
    doc.set("c", "line1\nline2");
    doc.set("d", "");

    std::string text = doc.dump();
    auto        back = yaml_doc::parse(text);
    REQUIRE(back.has_value());
    REQUIRE(back->get("a") == "hello world");
    REQUIRE(back->get("b") == "say \"hi\"");
    REQUIRE(back->get("c") == "line1\nline2");
    REQUIRE(back->get("d") == "");

    // 注释、行内注释、plain 值、空值
    auto c = yaml_doc::parse("# top comment\n\nkey: \"v\" # tail\nplain: hi # c\nempty:\n");
    REQUIRE(c.has_value());
    REQUIRE(c->get("key") == "v");
    REQUIRE(c->get("plain") == "hi");
    REQUIRE(c->get("empty") == "");
}

TEST_CASE("yaml: 非法输入返回 nullopt 不崩溃", "[config]")
{
    std::string err;
    REQUIRE_FALSE(yaml_doc::parse("no-separator-here", &err).has_value());
    REQUIRE_FALSE(err.empty());
    REQUIRE_FALSE(yaml_doc::parse("a: \"unterminated", &err).has_value());
    REQUIRE_FALSE(yaml_doc::parse("a: \"x\" trailing", &err).has_value());
}

TEST_CASE("schema: 导出完整性（分区顺序 + env 绑定 + 类型）", "[config]")
{
    Context app("ut_schema");
    app.build<ArgumentNamed>("hello", "h");
    app.build<ArgumentPosition>("name", "n");
    app.addOption<std::string>('o', "output", "out", "out.txt");
    app.addOption<int>("timeout", "t", 30);
    app.addFlag('v', "verbose", "v");
    app.option("output")->env("MY_OUTPUT");

    auto sch = app.schema();
    REQUIRE(sch.entries.size() == 5);
    REQUIRE(sch.entries[0].kind == "named");
    REQUIRE(sch.entries[0].name == "hello");
    REQUIRE(sch.entries[1].kind == "positional");
    REQUIRE(sch.entries[1].name == "name");
    REQUIRE(sch.entries[2].kind == "option");
    REQUIRE(sch.entries[2].name == "output");
    REQUIRE(sch.entries[2].env == "MY_OUTPUT");
    REQUIRE(sch.entries[2].value == "out.txt");
    REQUIRE(sch.entries[3].kind == "option");
    REQUIRE(sch.entries[3].type == "int");
    REQUIRE(sch.entries[4].kind == "flag");
    REQUIRE(sch.entries[4].type == "bool");

    std::string s = app.schema_string();
    REQUIRE(s.find("[named] hello") != std::string::npos);
    REQUIRE(s.find("env: MY_OUTPUT") != std::string::npos);
}

TEST_CASE("config: save_config → load_config 往返，值一致", "[config]")
{
    Context app("ut_round");
    app.build<ArgumentNamed>("hello", "h");
    app.addOption<int>("timeout", "t", 30);

    std::error_code ec;
    char const     *argv[] = { "ut_round", "--hello", "hi", "--timeout", "99" };
    char           *envp[] = { nullptr };
    app.parse(ec, 5, argv, envp);
    REQUIRE(!ec);
    REQUIRE(app.save_config("ut_cfg_round.yaml"));

    Context app2("ut_round");
    app2.build<ArgumentNamed>("hello", "h");
    app2.addOption<int>("timeout", "t", 30);
    REQUIRE(app2.load_config("ut_cfg_round.yaml"));
    REQUIRE(app2.getArgument<std::string>("hello") == "hi");
    REQUIRE(app2.getOption<int>("timeout") == 99);

    std::remove("ut_cfg_round.yaml");
}

TEST_CASE("config: from_yaml 反哺 env（environment 分区 set_env 命中）", "[config]")
{
    Context app("ut_envfeed");
    app.set_env("CMD_UT_X", "old");
    REQUIRE(app.from_yaml("environment.CMD_UT_X: \"hello\"\n"));
    REQUIRE(app.get_env("CMD_UT_X") == "hello");
}

TEST_CASE("config: 优先级 CLI > YAML > 环境变量 > 内置默认", "[config]")
{
    char *envp[] = { nullptr };

    // 全部给出：CLI 胜出
    Context app("ut_prio1");
    app.addOption<int>("timeout", "t", 30);
    app.option("timeout")->env("CMD_UT_T");
    app.set_env("CMD_UT_T", "50");
    REQUIRE(app.from_yaml("options.timeout: \"70\"\n"));
    std::error_code ec;
    char const     *argv[] = { "ut_prio1", "--timeout", "90" };
    app.parse(ec, 3, argv, envp);
    REQUIRE(!ec);
    REQUIRE(app.getOption<int>("timeout") == 90);

    // 无 CLI：配置胜出（配置 > 环境变量）
    Context app2("ut_prio2");
    app2.addOption<int>("timeout", "t", 30);
    app2.option("timeout")->env("CMD_UT_T");
    app2.set_env("CMD_UT_T", "50");
    REQUIRE(app2.from_yaml("options.timeout: \"70\"\n"));
    char const *argv2[] = { "ut_prio2" };
    app2.parse(ec, 1, argv2, envp);
    REQUIRE(!ec);
    REQUIRE(app2.getOption<int>("timeout") == 70);

    // 无配置无 CLI：环境变量胜出（环境变量 > 默认）
    Context app3("ut_prio3");
    app3.addOption<int>("timeout", "t", 30);
    app3.option("timeout")->env("CMD_UT_T");
    app3.set_env("CMD_UT_T", "50");
    char const *argv3[] = { "ut_prio3" };
    app3.parse(ec, 1, argv3, envp);
    REQUIRE(!ec);
    REQUIRE(app3.getOption<int>("timeout") == 50);

    // 全无：内置默认
    Context app4("ut_prio4");
    app4.addOption<int>("timeout", "t", 30);
    char const *argv4[] = { "ut_prio4" };
    app4.parse(ec, 1, argv4, envp);
    REQUIRE(!ec);
    REQUIRE(app4.getOption<int>("timeout") == 30);
}

TEST_CASE("config: 未知键记 warning 不中断（dump 可查）", "[config]")
{
    Context app("ut_warn");
    app.build<ArgumentNamed>("hello", "h");
    REQUIRE(app.from_yaml("arguments.hello: \"hi\"\nunknown.key: \"x\"\n"));
    REQUIRE(app.getArgument<std::string>("hello") == "hi");
    REQUIRE(app.dump().find("unknown.key") != std::string::npos);
}

TEST_CASE("loop: 两次独立输入互不合并、值各自生效、未给项回默认", "[loop]")
{
    Context app("ut_loop");
    app.build<ArgumentNamed>("hello", "h");
    app.addOption<int>("timeout", "t", 30);

    std::error_code ec;
    // 第一次调用（等价 loop 的一行输入）
    app.parse(ec, Lexer::tokensFromLine("--hello a --timeout 9"), Envs_T{});
    REQUIRE(!ec);
    REQUIRE(app.getArgument<std::string>("hello") == "a");
    REQUIRE(app.getOption<int>("timeout") == 9);

    // 第二次调用：reset 后未给项回默认，新输入独立生效（不合并）
    app.reset();
    app.parse(ec, Lexer::tokensFromLine("--hello b"), Envs_T{});
    REQUIRE(!ec);
    REQUIRE(app.getArgument<std::string>("hello") == "b");
    REQUIRE(app.getOption<int>("timeout") == 30);
    REQUIRE_FALSE(app.getOption<int>("timeout") == 9);
}

TEST_CASE("loop: 行错误不影响下一行（每行独立解析）", "[loop]")
{
    Context app("ut_loop_err");
    app.build<ArgumentNamed>("hello", "h");

    std::error_code ec;
    app.parse(ec, Lexer::tokensFromLine("--nope x"), Envs_T{});
    REQUIRE(ec);  // 行1 报错

    app.reset();
    app.parse(ec, Lexer::tokensFromLine("--hello ok"), Envs_T{});
    REQUIRE(!ec);  // 行2 正常
    REQUIRE(app.getArgument<std::string>("hello") == "ok");
    REQUIRE(app.isOk());  // reset 已清空错误栈
}

TEST_CASE("parser: --load-config / --save-config 内置 e2e", "[parser]")
{
    {
        std::ofstream ofs("ut_cfg_e2e.yaml", std::ios::binary | std::ios::trunc);
        ofs << "arguments.hello: \"from-config\"\n";
    }
    {
        std::ofstream ofs("ut_cfg_e2e_pos.yaml", std::ios::binary | std::ios::trunc);
        ofs << "positionals.name: \"peter\"\npositionals.count: \"3\"\n";
    }

    // load-config + CLI 覆盖：CLI 胜出
    Context app("ut_e2e1");
    app.build<ArgumentNamed>("hello", "h");
    std::error_code ec;
    app.parse(ec, Tokens_T{ "--load-config", "ut_cfg_e2e.yaml", "--hello", "cli" }, Envs_T{});
    REQUIRE(!ec);
    REQUIRE(app.getArgument<std::string>("hello") == "cli");

    // 仅 load-config：配置生效
    Context app2("ut_e2e2");
    app2.build<ArgumentNamed>("hello", "h");
    app2.parse(ec, Tokens_T{ "--load-config", "ut_cfg_e2e.yaml" }, Envs_T{});
    REQUIRE(!ec);
    REQUIRE(app2.getArgument<std::string>("hello") == "from-config");

    // 配置提供位置参数后，CLI 无需重复提供
    Context app4("ut_e2e4");
    app4.build<ArgumentPosition>("name", "n");
    app4.build<ArgumentPosition>("count", "c");
    app4.parse(ec, Tokens_T{ "--load-config", "ut_cfg_e2e_pos.yaml" }, Envs_T{});
    REQUIRE(!ec);
    REQUIRE(app4.getArgument<std::string>("name") == "peter");
    REQUIRE(app4.getArgument<std::string>("count") == "3");

    // CLI 仍可覆盖配置提供的位置参数（CLI > 配置）
    Context app5("ut_e2e5");
    app5.build<ArgumentPosition>("name", "n");
    app5.build<ArgumentPosition>("count", "c");
    app5.parse(ec, Tokens_T{ "--load-config", "ut_cfg_e2e_pos.yaml", "peter2", "4" }, Envs_T{});
    REQUIRE(!ec);
    REQUIRE(app5.getArgument<std::string>("name") == "peter2");
    REQUIRE(app5.getArgument<std::string>("count") == "4");

    // save-config：解析完成后写出 YAML，内容含解析结果
    Context app3("ut_e2e3");
    app3.build<ArgumentNamed>("hello", "h");
    app3.addOption<int>("timeout", "t", 30);
    app3.parse(ec,
               Tokens_T{ "--hello", "saved", "--timeout", "77", "--save-config",
                         "ut_cfg_save.yaml" },
               Envs_T{});
    REQUIRE(!ec);
    {
        std::ifstream ifs("ut_cfg_save.yaml", std::ios::binary);
        std::ostringstream oss;
        oss << ifs.rdbuf();
        std::string text = oss.str();
        REQUIRE(text.find("arguments.hello: \"saved\"") != std::string::npos);
        REQUIRE(text.find("options.timeout: \"77\"") != std::string::npos);
    }

    std::remove("ut_cfg_e2e.yaml");
    std::remove("ut_cfg_e2e_pos.yaml");
    std::remove("ut_cfg_save.yaml");
}

/* ==================== 选项 action（system_call 子进程执行） ==================== */

namespace {
/* action 测试辅助：把 {value} 写入临时目录文件，作为"命令真实执行过"的可观测副作用 */
std::string action_test_cmd(std::string const &_File, bool _Append)
{
#ifdef _WIN32
    return "cmd /c echo {value}" + std::string(_Append ? ">>" : ">") + "\"" + _File + "\"";
#else
    return "echo {value} " + std::string(_Append ? ">>" : ">") + "\"" + _File + "\"";
#endif
}

std::string action_test_file(std::string const &_Tag)
{
    return (std::filesystem::temp_directory_path()
            / ("cmd_args_action_" + _Tag + ".txt"))
        .string();
}

/* Windows 的 echo 输出带 \r\n，读回行时剥掉 \r */
void strip_cr(std::string &_S)
{
    if (!_S.empty() && _S.back() == '\r') { _S.pop_back(); }
}
}  // namespace

TEST_CASE("context: 选项 action 命中即执行 system_call，{value} 原样替换", "[action]")
{
    auto file = action_test_file("echo");
    std::remove(file.c_str());

    Context app("ut_action_echo");
    app.addOption<std::string>("task", "run a system command", "");
    app.option("task")->action(action_test_cmd(file, false));

    std::error_code ec;
    app.parse(ec, Tokens_T{ "--task", "Hello World" }, Envs_T{});
    REQUIRE(!ec);

    // 子进程命令确实被执行：临时文件被写出，且 {value} 被替换为选项值（含空格）
    std::ifstream ifs(file);
    REQUIRE(ifs.good());
    std::string line;
    std::getline(ifs, line);
    strip_cr(line);
    REQUIRE(line == "Hello World");

    // action 属于执行配置而非取值：不进入 schema / YAML 导出
    REQUIRE(app.schema_string().find("cmd_args_action_") == std::string::npos);
    REQUIRE(app.to_yaml().find("{value}") == std::string::npos);
}

TEST_CASE("context: action 三次命中同一选项时由 Executor 按 key 执行一次", "[action]")
{
    auto file = action_test_file("order");
    std::remove(file.c_str());

    Context app("ut_action_order");
    app.addOption<std::string>("step", "run a step", "");
    app.option("step")->action(action_test_cmd(file, true));

    std::error_code ec;
    app.parse(ec, Tokens_T{ "--step", "one", "--step", "two", "--step", "three" }, Envs_T{});
    REQUIRE(!ec);

    // Executor 按 key 执行：actionOrder 去重后只有 "step"，用最终值 "three"
    std::ifstream ifs(file);
    REQUIRE(ifs.good());
    std::string line;
    std::getline(ifs, line);
    strip_cr(line);
    REQUIRE(line == "three");  // 最终值
}

TEST_CASE("context: 环境变量兜底与 YAML 外部赋值不触发 action", "[action]")
{
    auto file = action_test_file("fallback");
    std::remove(file.c_str());

    Context app("ut_action_fb");
    app.addOption<std::string>("task", "run a command", "default");
    app.option("task")->action(action_test_cmd(file, false));
    app.option("task")->env("UT_ACTION_TASK");

    std::error_code ec;
    app.parse(ec, Tokens_T{}, Envs_T{ "UT_ACTION_TASK=from-env" });
    REQUIRE(!ec);
    REQUIRE(app.getOption<std::string>("task") == "from-env");  // 环境变量兜底值生效
    REQUIRE_FALSE(std::ifstream(file).good());  // 但 action 未被触发（未写文件）

    // YAML 外部赋值（setExternal 路径）同样不触发 action
    app.from_yaml("options.task: \"from-yaml\"");
    REQUIRE_FALSE(std::ifstream(file).good());
}

TEST_CASE("context: action 命令失败向 stderr 告警且不中断解析", "[action]")
{
    Context app("ut_action_fail");
    app.addOption<int>("a", "a", 0);
    app.option("a")->action(
#ifdef _WIN32
        "cmd /c exit 1"
#else
        "false"
#endif
    );
    app.addOption<int>("b", "b", 0);

    std::ostringstream oss;
    std::streambuf *old = std::cerr.rdbuf(oss.rdbuf());  // 捕获 stderr
    std::error_code ec;
    app.parse(ec, Tokens_T{ "--a", "1", "--b", "2" }, Envs_T{});
    std::cerr.rdbuf(old);

    REQUIRE(!ec);                              // 失败不中断解析
    REQUIRE(app.getOption<int>("b") == 2);    // 后续选项照常赋值
    REQUIRE(oss.str().find("(action)") != std::string::npos);  // 有告警输出
}

/* ==================== Interpreter 解释执行（基类引擎） ==================== */

TEST_CASE("interpreter: once 未解释时抛 ErrorInterp(STOPPED)", "[interpreter]")
{
    Context app("ut_once_stopped");
    app.addOption<int>("timeout", "t", 30);

    // 未 parse 直接 once()：解释器未就绪，抛 STOPPED 而非退出进程
    REQUIRE_THROWS_AS(app.once(), ErrorInterp);
}

TEST_CASE("interpreter: loop 逐行解释执行，exit 退出、行间不合并", "[interpreter]")
{
    Context app("ut_loop_interp");
    app.build<ArgumentNamed>("hello", "h");
    app.addOption<int>("timeout", "t", 30);

    // 输入流重定向模拟 REPL：两行解释 + exit 退出
    std::istringstream in("--hello a --timeout 9\n--hello b\nexit\n");
    std::ostringstream out;
    std::streambuf *oldIn  = std::cin.rdbuf(in.rdbuf());
    std::streambuf *oldOut = std::cout.rdbuf(out.rdbuf());

    app.loop();  // 逐行解释执行直到 exit

    std::cin.rdbuf(oldIn);
    std::cout.rdbuf(oldOut);

    // 两行都解释成功：无错误输出
    REQUIRE(out.str().find("(parse error)") == std::string::npos);
    // 循环结束状态 = 最后一次输入（行间 reset：hello=b，timeout 回默认）
    REQUIRE(app.getArgument<std::string>("hello") == "b");
    REQUIRE(app.getOption<int>("timeout") == 30);
}

TEST_CASE("interpreter: loop 行错误展示后继续下一行", "[interpreter]")
{
    Context app("ut_loop_err2");
    app.build<ArgumentNamed>("hello", "h");

    std::istringstream in("--nope x\n--hello ok\nexit\n");
    std::ostringstream out;
    std::streambuf *oldIn  = std::cin.rdbuf(in.rdbuf());
    std::streambuf *oldOut = std::cout.rdbuf(out.rdbuf());

    app.loop();

    std::cin.rdbuf(oldIn);
    std::cout.rdbuf(oldOut);

    // 第一行错误被展示，第二行正常解释；状态为最后一次成功输入
    REQUIRE(out.str().find("(parse error)") != std::string::npos);
    REQUIRE(app.getArgument<std::string>("hello") == "ok");
}

/* ==================== Interpreter 注入模式：直接构造 Parser + Errors ==================== */

TEST_CASE("interpreter: 继承三解析器模式逐行解释（写入 *Data）", "[interpreter]")
{
    Model model;
    Errors errors;
    Interpreter interp(&model);

    // 通过 OptionBuilder 注册选项到 Model 的 OptionData
    OptionBuilder ob(&model.m_opts__);
    ob.addOption<int>("timeout", "t", 30);

    // 正常行：--timeout 9 → 继承 parse + virtual hook 内联触发 action → 写入 *Data
    auto ec = interp.interpret("--timeout 9", errors);
    REQUIRE(!ec);
    REQUIRE(interp.prepared());

    auto *p = model.m_opts__.mgrValues.find("timeout");
    REQUIRE(p != nullptr);
    REQUIRE(p->strValue == "9");

    // setExitCode 生效
    interp.setExitCode(7);
    REQUIRE(interp.exitCode() == 7);

    // 错误行：--nope → ec 非零，错误栈有内容
    ec = interp.interpret("--nope x", errors);
    REQUIRE(ec);
    REQUIRE(!errors.isOk());
    REQUIRE(interp.describeError(ec, errors).find("(parse error)") != std::string::npos);

    // resetData 有效：下一次 interpret 前选项还原为默认值
    REQUIRE(p->strValue == "30");
}

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
    ArgumentBuilder ab(&model.m_args__);
    ab.build<ArgumentPosition>("name", "your name");

    auto *pPos = model.m_args__.mgrPos.doGet("name");
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
    OptionBuilder ob(&model.m_opts__);
    ob.addShortCircuit('h', "help", "show help", [] {});
    auto const *pAct = model.m_opts__.mgrActions.doGet("help");
    REQUIRE(pAct != nullptr);
    REQUIRE(pAct->kind() == ExpressionKind_E::Option);
    REQUIRE(pAct->getName() == "help");
    REQUIRE(pAct->getHelp() == "show help");
}

TEST_CASE("expression: environment kind() 返回 Env", "[expression]")
{
    Model model;
    EnvBuilder eb(&model.m_envs__);
    eb.setEnv("MY_KEY", "my_value");

    auto const *p = model.m_envs__.doGet("MY_KEY");
    REQUIRE(p != nullptr);
    REQUIRE(p->kind() == ExpressionKind_E::Env);
    REQUIRE(p->getName() == "MY_KEY");
}

/* ==================== Model 数据仓库查询 ==================== */

TEST_CASE("model: hasAction 随 actionOrder 翻转", "[model]")
{
    Model model;
    OptionBuilder ob(&model.m_opts__);
    ob.addOption<std::string>("task", "t", "")->action("echo {value}");

    // 构筑后未解析：actionOrder 为空 → 无待执行 action
    REQUIRE_FALSE(model.hasAction());

    // 模拟解析期记录（Executor 顺序执行的数据来源）
    model.m_opts__.recordParseOrder("task");
    REQUIRE(model.hasAction());

    model.m_opts__.clearParseOrder();
    REQUIRE_FALSE(model.hasAction());
}

TEST_CASE("model: expressions() 返回统一 Expression 视图", "[model]")
{
    Model model;
    ArgumentBuilder ab(&model.m_args__);
    ab.build<ArgumentNamed>("hello", "greeting");
    OptionBuilder ob(&model.m_opts__);
    ob.addOption<int>("timeout", "sec", 30);
    EnvBuilder eb(&model.m_envs__);
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