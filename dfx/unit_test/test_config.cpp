#include "catch2/catch.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include "cmd_args/cmd_args.hpp"

using namespace cmd_args;

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
