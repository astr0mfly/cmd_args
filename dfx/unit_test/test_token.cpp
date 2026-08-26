#include "catch2/catch.hpp"

#include <string>

#include "cmd_args/expression/token.h"

using namespace cmd_args;

/* ==================== 公共 Token 类型 ==================== */

TEST_CASE("token: ScopeSep 类型与 toText", "[token]")
{
    Token t(Token::Type_E::ScopeSep, "::", 0);
    REQUIRE(t.getAttr() == Token::Type_E::ScopeSep);
    REQUIRE(t.getName() == "::");
    REQUIRE(t.toText() == "::");
}

TEST_CASE("token: 长名词 toText 含 -- 前缀", "[token]")
{
    Token t(Token::Type_E::LongName, "timeout", 0);
    REQUIRE(t.toText() == "--timeout");
}

TEST_CASE("token: Unknown 类型 operator bool 为 false", "[token]")
{
    Token t;
    REQUIRE_FALSE(t);
    REQUIRE(t.getAttr() == Token::Type_E::Unknown);
}

TEST_CASE("token: hasInlineEq 标记保持", "[token]")
{
    Token t(Token::Type_E::LongName, "opt", 0);
    REQUIRE_FALSE(t.hasInlineEq());
    t.setInlineEq(true);
    REQUIRE(t.hasInlineEq());
}

TEST_CASE("token: clear 复位所有字段", "[token]")
{
    Token t(Token::Type_E::LongName, "x", 5);
    t.setInlineEq(true);
    t.clear();
    REQUIRE_FALSE(t);
    REQUIRE(t.getName().empty());
    REQUIRE(t.getLoc() == 0);
}

TEST_CASE("token: TokenStream_T 容器语义", "[token]")
{
    TokenStream_T stream;
    stream.emplace_back(Token::Type_E::LongName, "opt", 0);
    stream.emplace_back(Token::Type_E::Variable, "9", 0);
    REQUIRE(stream.size() == 2);
    REQUIRE(stream[0].toText() == "--opt");
    REQUIRE(stream[1].toText() == "9");
}
