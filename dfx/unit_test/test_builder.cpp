#include "catch2/catch.hpp"

#include "cmd_args/cmd_args.hpp"

using namespace cmd_args;

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
