#include "catch2/catch.hpp"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "cmd_args/cmd_args.hpp"

using namespace cmd_args;

/* ==================== ContextRuntime：流式运行时引擎 ==================== */

TEST_CASE("runtime: 未 parse 直接 once() 抛 ErrorInterp(STOPPED)", "[ctx_runtime]")
{
    ContextRuntime app("ut_rt_stopped");
    REQUIRE_THROWS_AS(app.once(), ErrorInterp);
}

TEST_CASE("runtime: parse 走流式解释，结果与批量语义等价", "[ctx_runtime]")
{
    ContextRuntime app("ut_rt_parse");
    app.build<ArgumentNamed>("hello", "h");
    app.addOption<int>('t', "timeout", "timeout in seconds", 30);

    std::error_code ec;
    char const *argv[] = { "test", "--hello", "a", "-t", "9", nullptr };
    app.parse(ec, 5, argv, nullptr);
    REQUIRE(!ec);
    REQUIRE(app.getArgument<std::string>("hello") == "a");
    REQUIRE(app.getOption<int>("timeout") == 9);
}

TEST_CASE("runtime: parse 错误码透传（NEED_VALUE / UNKNOWN_OPTION）", "[ctx_runtime]")
{
    ContextRuntime app1("ut_rt_need");
    app1.addOption<int>("timeout", "t", 30);
    std::error_code ec1;
    char const *argv1[] = { "test", "--timeout", nullptr };
    app1.parse(ec1, 2, argv1, nullptr);
    REQUIRE(ec1 == make_error_code(ParseError_E::NEED_VALUE));

    ContextRuntime app2("ut_rt_unknown");
    std::error_code ec2;
    char const *argv2[] = { "test", "--nope", "x", nullptr };
    app2.parse(ec2, 3, argv2, nullptr);
    REQUIRE(ec2 == make_error_code(ParseError_E::UNKNOWN_OPTION));
}

TEST_CASE("runtime: 解析即执行——onSet 回调在 parse 期间即时触发", "[ctx_runtime]")
{
    ContextRuntime app("ut_rt_onset");
    app.addOption<std::string>("step", "streaming step value", "");

    std::vector<std::string> fired;
    app.option("step")->onSet([&](std::string const &_V) { fired.emplace_back(_V); });

    std::error_code ec;
    char const *argv[] = { "test", "--step", "one", nullptr };
    app.parse(ec, 3, argv, nullptr);
    REQUIRE(!ec);
    REQUIRE(fired == std::vector<std::string>{ "one" });   // 流式：赋值即刻回调
}

TEST_CASE("runtime: loop 流式 REPL——识别退出信号、行间独立、与旧 Context.loop 一致", "[ctx_runtime]")
{
    ContextRuntime app("ut_rt_loop");
    app.build<ArgumentNamed>("hello", "h");

    std::istringstream in("--hello a\nexit\n");
    std::ostringstream out;
    std::streambuf *oldIn  = std::cin.rdbuf(in.rdbuf());
    std::streambuf *oldOut = std::cout.rdbuf(out.rdbuf());

    app.loop();

    std::cin.rdbuf(oldIn);
    std::cout.rdbuf(oldOut);

    REQUIRE(out.str().find("(parse error)") == std::string::npos);
    REQUIRE(app.getArgument<std::string>("hello") == "a");   // 每轮独立解释，最后一轮生效
}
