#include "catch2/catch.hpp"

#include <string>
#include <vector>

#include "cmd_args/cmd_args.hpp"

using namespace cmd_args;

/* ==================== 词法：Lexer / Scanner ==================== */

TEST_CASE("lexer: argv 原始 token 归一化", "[lexer]")
{
    char const *raw[] = { "-?", "--foo=bar", "-abc", "--", "a=b", "x.y", "-5", "", "   " };
    char       *envp[] = { nullptr };
    Lexer lx(9, raw, envp);

    auto toks = lx.tokens();
    REQUIRE(toks == Tokens_T{ "-?", "--foo=bar", "-abc", "--", "a=b", "x.y", "-5" });
}

TEST_CASE("lexer: 值选项取值保持原样（含 . = 等字符）", "[lexer]")
{
    char const *raw[] = { "-o", "out.txt", "--env-set", "A=1" };
    char       *envp[] = { nullptr };
    Lexer lx(4, raw, envp);

    auto toks = lx.tokens();
    REQUIRE(toks == Tokens_T{ "-o", "out.txt", "--env-set", "A=1" });
}

TEST_CASE("lexer: 空输入得到空 token 流", "[lexer]")
{
    char const *raw[] = { "" };
    char       *envp[] = { nullptr };
    Lexer lx(1, raw, envp);

    REQUIRE(lx.tokens().empty());
}

TEST_CASE("lexer: REPL 整行分词（空白分隔、--name=value 合并）", "[lexer]")
{
    REQUIRE(Lexer::tokensFromLine("hello world") == Tokens_T{ "hello", "world" });
    REQUIRE(Lexer::tokensFromLine("--a=1 b") == Tokens_T{ "--a=1", "b" });
    REQUIRE(Lexer::tokensFromLine("").empty());
}

TEST_CASE("lexer: 未闭合字符串报词法错误", "[lexer]")
{
    REQUIRE_THROWS_AS(Lexer::tokensFromLine("\"abc"), ErrorLexer);
    REQUIRE_THROWS_AS(Lexer::tokensFromLine("--a \"x"), ErrorLexer);
}

/* ==================== Scanner 流式增量分词 ==================== */

TEST_CASE("lexer: Scanner::step 增量分词逐字符产出 token", "[lexer]")
{
    using cmd_args::detail::Scanner;
    using cmd_args::detail::Token;

    Scanner sc({});
    // "--timeout 9" 逐字符喂入
    REQUIRE_FALSE(sc.step('-'));    // '-' 触发 IN_INTEGER
    REQUIRE_FALSE(sc.step('-'));    // 第二个 '-' 转入 IN_LONG_NAME
    REQUIRE_FALSE(sc.step('t'));
    REQUIRE_FALSE(sc.step('i'));
    REQUIRE_FALSE(sc.step('m'));
    REQUIRE_FALSE(sc.step('e'));
    REQUIRE_FALSE(sc.step('o'));
    REQUIRE_FALSE(sc.step('u'));
    REQUIRE_FALSE(sc.step('t'));
    auto tok1 = sc.step(' ');       // space 结束 LongName
    REQUIRE(tok1.has_value());
    REQUIRE(tok1->toText() == "--timeout");

    REQUIRE_FALSE(sc.step('9'));    // Integer 未终结
    auto tok2 = sc.step('\0');      // 流结束 flush
    REQUIRE(tok2.has_value());
    REQUIRE(tok2->toText() == "9");
}

TEST_CASE("lexer: Scanner::step 字符串未终结抛 ErrorLexer", "[lexer]")
{
    using cmd_args::detail::Scanner;

    Scanner sc({});
    sc.step('"');
    REQUIRE_FALSE(sc.step('a'));
    REQUIRE_FALSE(sc.step('b'));
    REQUIRE_THROWS_AS(sc.step('\0'), ErrorLexer);  // IN_STRING → UNTERMINATED_STRING
}

TEST_CASE("lexer: Scanner::step --timeout=9 产出 inline eq 标记", "[lexer]")
{
    using cmd_args::detail::Scanner;

    Scanner sc({});
    sc.step('-'); sc.step('-');
    sc.step('t'); sc.step('i'); sc.step('m'); sc.step('e'); sc.step('o'); sc.step('u'); sc.step('t');
    auto tok1 = sc.step('=');       // '=' 结束 LongName → hasInlineEq = true
    REQUIRE(tok1.has_value());
    REQUIRE(tok1->toText() == "--timeout");
    REQUIRE(tok1->hasInlineEq());

    auto tok2 = sc.step('9');
    REQUIRE_FALSE(tok2.has_value()); // 9 仍在 Integer 态
    auto tok3 = sc.step('\0');
    REQUIRE(tok3.has_value());
    REQUIRE_FALSE(tok3->hasInlineEq());
}

TEST_CASE("lexer: Scanner::step 与批量分词在终结符重派上一致", "[lexer]")
{
    using cmd_args::detail::Scanner;

    auto streamTokens = [](std::string const &_Line) {
        Tokens_T res;
        Scanner sc({});
        for (char c : _Line) { if (auto t = sc.step(c)) { res.emplace_back(t->toText()); } }
        if (auto t = sc.step('\0')) { res.emplace_back(t->toText()); }
        return res;
    };

    REQUIRE(streamTokens("-x9") == Tokens_T{ "-x", "9" });
    REQUIRE(streamTokens("1.5x") == Tokens_T{ "1.5", "x" });
    REQUIRE(streamTokens(">out") == Tokens_T{ ">", "out" });
    REQUIRE(streamTokens("env f") == Tokens_T{ "env", "f" });
    REQUIRE(streamTokens("--timeout 9") == Tokens_T{ "--timeout", "9" });
    REQUIRE(streamTokens("a=b") == Tokens_T{ "a=b" });   // Variable 吸收 '='，与批量一致

    // 与批量分词逐行对照（本次修复的核心：终结符重派语义一致）
    for (std::string const &line : std::vector<std::string>{ "-x9", "1.5x", ">out",
                                                             "env f", "--timeout 9", "a=b" }) {
        REQUIRE(streamTokens(line) == Lexer::tokensFromLine(line));
    }
}
