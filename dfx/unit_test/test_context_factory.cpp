#include "catch2/catch.hpp"

#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

#include "cmd_args/cmd_args.hpp"

using namespace cmd_args;

/* ==================== make_context 工厂注入 ==================== */

TEST_CASE("factory: 按 EngineKind 注入对应派生类型", "[ctx_factory]")
{
    auto step = make_context(EngineKind_E::E_STEP, "ut_f_step");
    REQUIRE(dynamic_cast<ContextStep *>(step.get()) != nullptr);
    REQUIRE(dynamic_cast<ContextRuntime *>(step.get()) == nullptr);

    auto rt = make_context(EngineKind_E::E_RUN_TIME, "ut_f_rt");
    REQUIRE(dynamic_cast<ContextRuntime *>(rt.get()) != nullptr);
    REQUIRE(dynamic_cast<ContextStep *>(rt.get()) == nullptr);
}

TEST_CASE("factory: 未知枚举值抛 invalid_argument", "[ctx_factory]")
{
    auto bad = static_cast<EngineKind_E>(99);
    REQUIRE_THROWS_AS(make_context(bad, "ut_f_bad"), std::invalid_argument);
}

TEST_CASE("factory: 基类指针多态分发 once/loop", "[ctx_factory]")
{
    std::unique_ptr<ContextBase> step = make_context(EngineKind_E::E_STEP, "ut_f_poly1");
    step->addOption<int>("timeout", "t", 30);
    REQUIRE_THROWS_AS(step->once(), ErrorInterp);   // 分发到 ContextStep::once

    std::unique_ptr<ContextBase> rt = make_context(EngineKind_E::E_RUN_TIME, "ut_f_poly2");
    rt->build<ArgumentNamed>("hello", "h");

    std::istringstream in("--hello a\nexit\n");
    std::ostringstream out;
    std::streambuf *oldIn  = std::cin.rdbuf(in.rdbuf());
    std::streambuf *oldOut = std::cout.rdbuf(out.rdbuf());

    rt->loop();                                     // 分发到 ContextRuntime::loop

    std::cin.rdbuf(oldIn);
    std::cout.rdbuf(oldOut);

    REQUIRE(rt->getArgument<std::string>("hello") == "a");
}
