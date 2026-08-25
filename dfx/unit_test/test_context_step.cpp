#include "catch2/catch.hpp"

#include <iostream>
#include <sstream>
#include <string>

#include "cmd_args/cmd_args.hpp"

using namespace cmd_args;

/* ==================== ContextStep：批量步进引擎 ==================== */

TEST_CASE("step: 未 parse 直接 once() 抛 ErrorInterp(STOPPED)", "[ctx_step]")
{
    ContextStep app("ut_step_stopped");
    app.addOption<int>("timeout", "timeout in seconds", 30);

    REQUIRE_THROWS_AS(app.once(), ErrorInterp);
}

TEST_CASE("step: parse 批量管线与旧 Context 结果一致", "[ctx_step]")
{
    ContextStep app("ut_step_parse");
    app.build<ArgumentNamed>("hello", "Say hello");
    app.addOption<int>("timeout", "timeout in seconds", 30);

    std::error_code ec;
    char const *argv[] = { "test", "--hello", "a", "--timeout", "9", nullptr };
    app.parse(ec, 5, argv, nullptr);
    REQUIRE(!ec);
    REQUIRE(app.getArgument<std::string>("hello") == "a");
    REQUIRE(app.getOption<int>("timeout") == 9);
}

TEST_CASE("step: 内置 -? 短回路打印 usage 且中止后续解析", "[ctx_step]")
{
    ContextStep app("ut_step_help");
    app.addOption<int>("timeout", "timeout in seconds", 30);

    std::error_code ec;
    char const *argv[] = { "test", "-?", nullptr };

    std::ostringstream out;
    std::streambuf *oldOut = std::cout.rdbuf(out.rdbuf());
    app.parse(ec, 2, argv, nullptr);
    std::cout.rdbuf(oldOut);

    REQUIRE(!ec);
    REQUIRE(out.str().find("usage:") != std::string::npos);
}

TEST_CASE("step: loop 批量 REPL——行间独立、错误行展示后继续、exit 退出", "[ctx_step]")
{
    ContextStep app("ut_step_loop");
    app.build<ArgumentNamed>("hello", "h");
    app.addOption<int>("timeout", "t", 30);

    std::istringstream in("--hello a --timeout 9\n--nope x\n--hello b\nexit\n");
    std::ostringstream out;
    std::streambuf *oldIn  = std::cin.rdbuf(in.rdbuf());
    std::streambuf *oldOut = std::cout.rdbuf(out.rdbuf());

    app.loop();

    std::cin.rdbuf(oldIn);
    std::cout.rdbuf(oldOut);

    // 错误行被展示
    REQUIRE(out.str().find("(parse error)") != std::string::npos);
    // 行间独立（每轮等价一次 once 输入，不合并）：最后一次成功输入生效，未给项回默认
    REQUIRE(app.getArgument<std::string>("hello") == "b");
    REQUIRE(app.getOption<int>("timeout") == 30);
}
