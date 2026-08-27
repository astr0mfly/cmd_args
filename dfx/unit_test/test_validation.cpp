#include "catch2/catch.hpp"

#include <filesystem>
#include <fstream>
#include <string>

#include "cmd_args/cmd_args.hpp"
#include "cmd_args/base/validator.h"

using namespace cmd_args;

TEST_CASE("validator: is_port 接受 1..65535，拒绝越界与非数字", "[validator]")
{
    REQUIRE(validator_detail::is_port("80") == true);
    REQUIRE(validator_detail::is_port("1") == true);
    REQUIRE(validator_detail::is_port("65535") == true);
    REQUIRE(validator_detail::is_port("0") == false);
    REQUIRE(validator_detail::is_port("65536") == false);
    REQUIRE(validator_detail::is_port("-1") == false);
    REQUIRE(validator_detail::is_port("80a") == false);
    REQUIRE(validator_detail::is_port("") == false);
}

TEST_CASE("validator: is_ipv4 四段 0..255", "[validator]")
{
    REQUIRE(validator_detail::is_ipv4("192.168.1.1") == true);
    REQUIRE(validator_detail::is_ipv4("0.0.0.0") == true);
    REQUIRE(validator_detail::is_ipv4("255.255.255.255") == true);
    REQUIRE(validator_detail::is_ipv4("256.0.0.1") == false);
    REQUIRE(validator_detail::is_ipv4("1.2.3") == false);
    REQUIRE(validator_detail::is_ipv4("1.2.3.4.5") == false);
    REQUIRE(validator_detail::is_ipv4("a.b.c.d") == false);
    REQUIRE(validator_detail::is_ipv4("") == false);
}

TEST_CASE("validator: is_ipv6 标准/压缩/内嵌 IPv4", "[validator]")
{
    REQUIRE(validator_detail::is_ipv6("2001:db8::1") == true);
    REQUIRE(validator_detail::is_ipv6("::1") == true);
    REQUIRE(validator_detail::is_ipv6("::") == true);
    REQUIRE(validator_detail::is_ipv6("1:2:3:4:5:6:7:8") == true);
    REQUIRE(validator_detail::is_ipv6("::ffff:192.168.1.1") == true);
    REQUIRE(validator_detail::is_ipv6("2001:db8::1::2") == false);  // 两次压缩
    REQUIRE(validator_detail::is_ipv6("1:2:3:4:5:6:7:8:9") == false);  // 九组
    REQUIRE(validator_detail::is_ipv6("1:2:3:4:5:6:7:") == false);  // 单冒号结尾
    REQUIRE(validator_detail::is_ipv6(":1:2") == false);            // 单冒号开头
    REQUIRE(validator_detail::is_ipv6("g::1") == false);            // 非hex
}

TEST_CASE("validator: is_float / is_double 数值语义", "[validator]")
{
    REQUIRE(validator_detail::is_float("1.5") == true);
    REQUIRE(validator_detail::is_float("-2.25e3") == true);
    REQUIRE(validator_detail::is_float("1e999") == false);  // 溢出 / inf
    REQUIRE(validator_detail::is_float("abc") == false);
    REQUIRE(validator_detail::is_float("1.2x") == false);
    REQUIRE(validator_detail::is_float("") == false);
    REQUIRE(validator_detail::is_double("0.1") == true);
    REQUIRE(validator_detail::is_double("3e-2") == true);
    REQUIRE(validator_detail::is_double("nan") == false);
    REQUIRE(validator_detail::is_double("inf") == false);
}

TEST_CASE("validator: is_date 真实日历与闰年", "[validator]")
{
    REQUIRE(validator_detail::is_date("2026-08-27") == true);
    REQUIRE(validator_detail::is_date("2024-02-29") == true);   // 闰年
    REQUIRE(validator_detail::is_date("2023-02-29") == false);  // 非闰年
    REQUIRE(validator_detail::is_date("2026-13-01") == false);
    REQUIRE(validator_detail::is_date("2026-00-10") == false);
    REQUIRE(validator_detail::is_date("2026-8-27") == false);   // 缺前导零
    REQUIRE(validator_detail::is_date("2026/08/27") == false);  // 分隔符错误
    REQUIRE(validator_detail::is_date("2026-02-30") == false);
}

TEST_CASE("validator: is_duration 数字[.小数] + 单位 s/m/h/d", "[validator]")
{
    REQUIRE(validator_detail::is_duration("30s") == true);
    REQUIRE(validator_detail::is_duration("5m") == true);
    REQUIRE(validator_detail::is_duration("1.5h") == true);
    REQUIRE(validator_detail::is_duration("2d") == true);
    REQUIRE(validator_detail::is_duration("90") == true);   // 裸数字 = 秒
    REQUIRE(validator_detail::is_duration("-5s") == false);
    REQUIRE(validator_detail::is_duration("10x") == false);
    REQUIRE(validator_detail::is_duration("") == false);
    REQUIRE(validator_detail::is_duration(".") == false);
}

TEST_CASE("validator: is_hostname / is_uuid / is_path / 存在性", "[validator]")
{
    REQUIRE(validator_detail::is_hostname("example.com") == true);
    REQUIRE(validator_detail::is_hostname("a-b.c99") == true);
    REQUIRE(validator_detail::is_hostname("-bad.com") == false);
    REQUIRE(validator_detail::is_hostname("a..b") == false);
    REQUIRE(validator_detail::is_hostname("") == false);
    REQUIRE(validator_detail::is_uuid("550e8400-e29b-41d4-a716-446655440000") == true);
    REQUIRE(validator_detail::is_uuid("550e8400-e29b-41d4") == false);
    REQUIRE(validator_detail::is_uuid("550e8400e29b41d4a716446655440000") == false);
    REQUIRE(validator_detail::is_path("C:\\tmp\\x.txt") == true);
    REQUIRE(validator_detail::is_path("") == false);
    REQUIRE(validator_detail::is_existing_file("no-such-file-8492.txt") == false);

    auto f = (std::filesystem::temp_directory_path() / "cmd_args_ut_validator.txt").string();
    { std::ofstream(f) << "x"; }
    REQUIRE(validator_detail::is_existing_file(f) == true);
    std::filesystem::remove(f);
    REQUIRE(validator_detail::is_existing_file(f) == false);
}
