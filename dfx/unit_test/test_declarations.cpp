#include "catch2/catch.hpp"
#include "cmd_args/cmd_args.hpp"
using namespace cmd_args;

TEST_CASE("declare: 批量注册参数和选项", "[declare]")
{
    Context app("ut_declare");
    app.declare(
        ArgumentNamed{"hello", "Say hello"},
        ArgumentPosition{"name", "your name"},
        OptionDef<int>{'t', "timeout", "timeout seconds", 30},
        OptionDef<std::string>{'o', "output", "output file", "out.txt"},
        FlagDef{'v', "verbose", "verbose"}
    );
    REQUIRE(app.argument("hello") != nullptr);
    // ArgumentPosition 通过 mgrPos 存储，用 getArgument 验证
    // app.parse 后才能 getArgument，这里仅验证不抛异常
    REQUIRE(app.option("timeout") != nullptr);
    REQUIRE(app.option("output") != nullptr);
    REQUIRE(app.option("verbose") != nullptr);
    REQUIRE(app.getOption<int>("timeout") == 30);
    REQUIRE(app.getOption<std::string>("output") == "out.txt");
    REQUIRE(app.getOption<bool>("verbose") == false);
}

TEST_CASE("declare: 仅参数", "[declare]")
{
    Context app("ut_declare_args");
    app.declare(
        ArgumentNamed{"src", "source file"},
        ArgumentNamed{"dst", "destination file"}
    );
    REQUIRE(app.argument("src") != nullptr);
    REQUIRE(app.argument("dst") != nullptr);
}

TEST_CASE("declare: 仅选项", "[declare]")
{
    Context app("ut_declare_opts");
    app.declare(
        OptionDef<int>{'j', "jobs", "parallel jobs", 4},
        FlagDef{'q', "quiet", "quiet mode"}
    );
    REQUIRE(app.option("jobs") != nullptr);
    REQUIRE(app.option("quiet") != nullptr);
    REQUIRE(app.getOption<int>("jobs") == 4);
}

TEST_CASE("declare: 支持链式调用", "[declare]")
{
    Context app("ut_declare_chain");
    app.declare(
        ArgumentNamed{"hello", "greeting"},
        FlagDef{'v', "verbose", "verbose"}
    )
    .addOption("timeout", "timeout", 10)
    .addFlag('q', "quiet", "quiet");
    REQUIRE(app.argument("hello") != nullptr);
    REQUIRE(app.option("verbose") != nullptr);
    REQUIRE(app.option("timeout") != nullptr);
    REQUIRE(app.option("quiet") != nullptr);
}

TEST_CASE("declare: 与现有 API 混用", "[declare]")
{
    Context app("ut_declare_mix");
    app.declare(
        ArgumentNamed{"cmd", "command to run"},
        OptionDef<int>{'j', "jobs", "parallel jobs", 4}
    )
    .build<ArgumentNamed>("target", "build target")
    .addFlag('v', "verbose", "verbose");
    REQUIRE(app.argument("cmd") != nullptr);
    REQUIRE(app.argument("target") != nullptr);
    REQUIRE(app.option("jobs") != nullptr);
    REQUIRE(app.option("verbose") != nullptr);
}

TEST_CASE("addOption: int 类型推导（仅长名）", "[declare]")
{
    Context app("ut_deduce_int");
    app.addOption("timeout", "timeout seconds", 30);
    REQUIRE(app.option("timeout") != nullptr);
    REQUIRE(app.getOption<int>("timeout") == 30);
}

TEST_CASE("addOption: double 类型推导（仅长名）", "[declare]")
{
    Context app("ut_deduce_double");
    app.addOption("ratio", "ratio value", 0.75);
    REQUIRE(app.option("ratio") != nullptr);
    REQUIRE(app.getOption<double>("ratio") == Approx(0.75));
}

TEST_CASE("addOption: std::string 类型推导（仅长名）", "[declare]")
{
    Context app("ut_deduce_string");
    app.addOption("output", "output file", std::string("out.txt"));
    REQUIRE(app.option("output") != nullptr);
    REQUIRE(app.getOption<std::string>("output") == "out.txt");
}

TEST_CASE("addOption: int 类型推导（含短名）", "[declare]")
{
    Context app("ut_deduce_int_short");
    app.addOption('t', "timeout", "timeout seconds", 30);
    REQUIRE(app.option("timeout") != nullptr);
    REQUIRE(app.getOption<int>("timeout") == 30);
}

TEST_CASE("addOption: double 类型推导（含短名）", "[declare]")
{
    Context app("ut_deduce_double_short");
    app.addOption('r', "ratio", "ratio value", 0.5);
    REQUIRE(app.option("ratio") != nullptr);
    REQUIRE(app.getOption<double>("ratio") == Approx(0.5));
}

TEST_CASE("addOption: std::string 类型推导（含短名）", "[declare]")
{
    Context app("ut_deduce_string_short");
    app.addOption('o', "output", "output file", std::string("out.txt"));
    REQUIRE(app.option("output") != nullptr);
    REQUIRE(app.getOption<std::string>("output") == "out.txt");
}

TEST_CASE("addOption: 类型推导链式调用", "[declare]")
{
    Context app("ut_deduce_chain");
    app.addOption("timeout", "timeout", 30)
       .addOption("ratio", "ratio", 0.5)
       .addOption("output", "output", std::string("out.txt"))
       .addFlag('v', "verbose", "verbose");
    REQUIRE(app.getOption<int>("timeout") == 30);
    REQUIRE(app.getOption<double>("ratio") == Approx(0.5));
    REQUIRE(app.getOption<std::string>("output") == "out.txt");
    REQUIRE(app.getOption<bool>("verbose") == false);
}

TEST_CASE("group: 基本分组", "[declare]")
{
    Context app("ut_group");
    app.group("网络",
        OptionDef<int>{'p', "port", "port number", 8080},
        OptionDef<std::string>{'H', "host", "host name", "localhost"}
    );
    REQUIRE(app.option("port") != nullptr);
    REQUIRE(app.option("host") != nullptr);
    REQUIRE(app.getOption<int>("port") == 8080);
    REQUIRE(app.getOption<std::string>("host") == "localhost");
    auto const &groups = app.model().groups_;
    REQUIRE(groups.size() == 1);
    REQUIRE(groups[0].name == "网络");
    REQUIRE(groups[0].members.size() == 2);
}

TEST_CASE("group: 多个分组", "[declare]")
{
    Context app("ut_group_multi");
    app.group("网络",
        OptionDef<int>{'p', "port", "port", 80},
        OptionDef<std::string>{'H', "host", "host", "localhost"}
    )
    .group("输出",
        OptionDef<std::string>{'o', "output", "output file", "out.txt"},
        FlagDef{'v', "verbose", "verbose"}
    );
    auto const &groups = app.model().groups_;
    REQUIRE(groups.size() == 2);
    REQUIRE(groups[0].name == "网络");
    REQUIRE(groups[1].name == "输出");
}

TEST_CASE("group: 与 declare 混用", "[declare]")
{
    Context app("ut_group_mix");
    app.declare(
        ArgumentNamed{"cmd", "command"},
        OptionDef<int>{'j', "jobs", "jobs", 4}
    )
    .group("网络",
        OptionDef<int>{'p', "port", "port", 80}
    )
    .addFlag('v', "verbose", "verbose");
    REQUIRE(app.argument("cmd") != nullptr);
    REQUIRE(app.option("jobs") != nullptr);
    REQUIRE(app.option("port") != nullptr);
    REQUIRE(app.option("verbose") != nullptr);
}

TEST_CASE("group: 解析与访问", "[declare]")
{
    Context app("ut_group_parse");
    app.group("网络",
        OptionDef<int>{'p', "port", "port", 80},
        OptionDef<std::string>{'H', "host", "host", "localhost"}
    )
    .group("输出",
        FlagDef{'v', "verbose", "verbose"}
    );
    Tokens_T toks{"--port", "9090", "--verbose"};
    Envs_T envs;
    app.parse(toks, envs);
    REQUIRE(app.getOption<int>("port") == 9090);
    REQUIRE(app.getOption<bool>("verbose") == true);
    REQUIRE(app.getOption<std::string>("host") == "localhost");
}

TEST_CASE("declare: 完整 CLI 场景", "[declare]")
{
    Context app("ut_declare_full");
    app.declare(
        ArgumentNamed{"src", "source directory"},
        ArgumentPosition{"dest", "destination directory"},
        OptionDef<int>{'j', "jobs", "parallel jobs", 4},
        OptionDef<std::string>{'o', "output", "output format", "json"},
        FlagDef{'v', "verbose", "verbose output"},
        FlagDef{'f', "force", "force overwrite"}
    );

    Tokens_T toks{"--src", "src_dir", "dst_dir", "-j", "8", "--output", "xml", "-v"};
    Envs_T envs;
    app.parse(toks, envs);

    REQUIRE(app.getArgument<std::string>("src") == "src_dir");
    REQUIRE(app.getArgument<std::string>("dest") == "dst_dir");
    REQUIRE(app.getOption<int>("jobs") == 8);
    REQUIRE(app.getOption<std::string>("output") == "xml");
    REQUIRE(app.getOption<bool>("verbose") == true);
    REQUIRE(app.getOption<bool>("force") == false);
}
