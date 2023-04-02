#define CATCH_CONFIG_MAIN
#include "catch2/catch.hpp"
#define private public

#include "cmd_args/cmd_args.hpp"

using namespace cmd_args;

TEST_CASE("description of once", "[single-file]") {
    parser p("this is a parser of ut");
    p.set_program_name("start");

    REQUIRE(p.description == "this is a parser of ut");
    REQUIRE(p.program_name == "start");

    REQUIRE(p.__descEnv() == "environments: start\nNo. : \"key\" = \"value\"\n");
    REQUIRE(p.__descUsage() == "usage: start [arguments]\n");
    REQUIRE(p.__descHelp() == "\nthis is a parser of ut\n\nOptions:\n");
}