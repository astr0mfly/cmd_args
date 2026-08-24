#include "catch2/catch.hpp"

#include "cmd_args/cmd_args.hpp"

using namespace cmd_args;

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

/* ==================== 内置短回路选项（Compiler 层 isFired） ==================== */

TEST_CASE("parser: -?/--help、-!/--env 命中即短路", "[compiler]")
{
    Model  model;
    Compiler p(&model);
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

/* ==================== loop：多次 parse 独立性 ==================== */

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

/* ==================== Context 容器化 ==================== */

TEST_CASE("context: model() accessor 返回底层 Model 引用", "[context]")
{
    Context app("ut_model_acc");

    // 程序名仅在 parse 时由 argv[0] 写入 Model；解析后经 model() 读回，
    // 验证访问器返回的确实是底层 Model（与内部数据同源，而非副本）
    std::error_code ec;
    char const *argv[] = { "ut_model_acc", nullptr };
    app.parse(ec, 1, argv, nullptr);
    REQUIRE(!ec);

    auto const &m = app.model();
    REQUIRE(m.strProgramName == "ut_model_acc");
}

TEST_CASE("context: expressions() 经 Context 统一视图覆盖三类条目", "[context]")
{
    Context app("ut_expr_view");
    app.build<ArgumentNamed>("hello", "greeting");
    app.addOption<int>("timeout", "sec", 30);
    app.set_env("MY_K", "MY_V");

    std::error_code ec;
    char const *argv[] = { "test", "--hello", "hi", "--timeout", "9", nullptr };
    app.parse(ec, 5, argv, nullptr);  // argc=非空条目数（尾部 nullptr 不计入）
    REQUIRE(!ec);

    auto exprs = app.model().expressions();
    REQUIRE(exprs.size() >= 3);

    size_t nArg = 0, nOpt = 0, nEnv = 0;
    for (auto const *e : exprs) {
        switch (e->kind()) {
        case ExpressionKind_E::Argument: ++nArg; break;
        case ExpressionKind_E::Option:   ++nOpt; break;
        case ExpressionKind_E::Env:      ++nEnv; break;
        }
    }
    REQUIRE(nArg >= 1);   // 命名参数 hello
    REQUIRE(nOpt >= 1);   // timeout + 内置选项若干
    REQUIRE(nEnv >= 1);   // MY_K（会话环境在 parse 后保留）
}
