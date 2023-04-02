#define CATCH_CONFIG_MAIN
#include "catch2/catch.hpp"
#include "cmd_args/cmd_args.hpp"

int mock_cmd()
{
    using namespace cmd_args;
    

    return 0;
}

TEST_CASE( "Factorials of 1 and higher are computed (pass)", "[single-file]" ) {
    REQUIRE(mock_cmd() == 0);
}