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
    using cmd_args::expression_detail::Scanner;
    using cmd_args::Token;

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
    using cmd_args::expression_detail::Scanner;

    Scanner sc({});
    sc.step('"');
    REQUIRE_FALSE(sc.step('a'));
    REQUIRE_FALSE(sc.step('b'));
    REQUIRE_THROWS_AS(sc.step('\0'), ErrorLexer);  // IN_STRING → UNTERMINATED_STRING
}

TEST_CASE("lexer: Scanner::step --timeout=9 产出 inline eq 标记", "[lexer]")
{
    using cmd_args::expression_detail::Scanner;

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
    using cmd_args::expression_detail::Scanner;

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

/* ==================== Lexer 关系语法合并 ==================== */

TEST_CASE("lexer: -a?b:c 条件关系语法合并为单个 token", "[lexer]")
{
    auto lineToks = Lexer::tokensFromLine("-verbose?debug:quiet");
    REQUIRE(lineToks.size() == 1);
    REQUIRE(lineToks.front() == "-verbose?debug:quiet");
}

TEST_CASE("lexer: -a?b:_ 条件关系（无操作分支）", "[lexer]")
{
    auto lineToks = Lexer::tokensFromLine("-verbose?debug:_");
    REQUIRE(lineToks.size() == 1);
    REQUIRE(lineToks.front() == "-verbose?debug:_");
}

TEST_CASE("lexer: -a?b 无冒号条件关系", "[lexer]")
{
    auto lineToks = Lexer::tokensFromLine("-verbose?debug");
    REQUIRE(lineToks.size() == 1);
    REQUIRE(lineToks.front() == "-verbose?debug");
}

TEST_CASE("lexer: -a->b 依赖关系语法", "[lexer]")
{
    auto lineToks = Lexer::tokensFromLine("-compile->link");
    REQUIRE(lineToks.size() == 1);
    REQUIRE(lineToks.front() == "-compile->link");
}

TEST_CASE("lexer: 普通聚合 -abc 不受关系语法影响", "[lexer]")
{
    auto lineToks = Lexer::tokensFromLine("-abc");
    REQUIRE(lineToks.size() == 1);
    REQUIRE(lineToks.front() == "-abc");
}

TEST_CASE("lexer: 关系语法与普通选项混合", "[lexer]")
{
    auto lineToks = Lexer::tokensFromLine("-verbose?debug:quiet -abc hello");
    // 应产出：关系token + 聚合token + 方位参数
    REQUIRE(lineToks.size() == 3);
    REQUIRE(lineToks.front() == "-verbose?debug:quiet");
    // 跳过中间的
    auto it = lineToks.begin();
    ++it;
    REQUIRE(*it == "-abc");
    ++it;
    REQUIRE(*it == "hello");
}

/* ==================== Scanner Env 行模式 ==================== */

TEST_CASE("lexer: scanEnvLine(A::B::C=D) 产出 ScopeSep", "[lexer][envscope]")
{
    using cmd_args::expression_detail::Scanner;
    using cmd_args::expression_detail::Source_T;
    using cmd_args::Token;

    Source_T src{ "A::B::C=D" };
    Scanner sc(src, Scanner::LexMode_E::EnvLine);
    auto toks = sc.scan();
    REQUIRE(toks.size() == 7);
    REQUIRE(toks[0].getAttr() == Token::Type_E::Variable);   REQUIRE(toks[0].getName() == "A");
    REQUIRE(toks[1].getAttr() == Token::Type_E::ScopeSep);   REQUIRE(toks[1].getName() == "::");
    REQUIRE(toks[2].getAttr() == Token::Type_E::Variable);   REQUIRE(toks[2].getName() == "B");
    REQUIRE(toks[3].getAttr() == Token::Type_E::ScopeSep);   REQUIRE(toks[3].getName() == "::");
    REQUIRE(toks[4].getAttr() == Token::Type_E::Variable);   REQUIRE(toks[4].getName() == "C");
    REQUIRE(toks[5].getAttr() == Token::Type_E::Delimiter);  REQUIRE(toks[5].getName() == "=");
    REQUIRE(toks[6].getAttr() == Token::Type_E::String);     REQUIRE(toks[6].getName() == "D");
}

TEST_CASE("lexer: scanEnvLine 单 ':' 产出 Delimiter", "[lexer][envscope]")
{
    using cmd_args::expression_detail::Scanner;
    using cmd_args::expression_detail::Source_T;
    Source_T src{ "KEY:value" };
    Scanner sc(src, Scanner::LexMode_E::EnvLine);
    auto toks = sc.scan();
    REQUIRE(toks.size() == 3);
    REQUIRE(toks[1].getName() == ":");
}

TEST_CASE("lexer: scanEnvLine 值为空 (KEY=)", "[lexer][envscope]")
{
    using cmd_args::expression_detail::Scanner;
    using cmd_args::expression_detail::Source_T;
    Source_T src{ "KEY=" };
    Scanner sc(src, Scanner::LexMode_E::EnvLine);
    auto toks = sc.scan();
    REQUIRE(toks.size() == 2);
    REQUIRE(toks[0].getName() == "KEY");
    REQUIRE(toks[1].getName() == "=");
}

TEST_CASE("lexer: scanEnvLine 值含路径分隔符 (VAL=/usr/bin)", "[lexer][envscope]")
{
    using cmd_args::expression_detail::Scanner;
    using cmd_args::expression_detail::Source_T;
    Source_T src{ "PATH=/usr/bin" };
    Scanner sc(src, Scanner::LexMode_E::EnvLine);
    auto toks = sc.scan();
    // 值含 '/' 应被完整吸收为一个 token
    REQUIRE(toks.size() == 3);
    REQUIRE(toks[0].getName() == "PATH");
    REQUIRE(toks[1].getName() == "=");
    REQUIRE(toks[2].getName() == "/usr/bin");
}

/* ==================== Lexer 类型化 scan() ==================== */

TEST_CASE("lexer: scan() 产出类型化 Token 流", "[lexer][typed]")
{
    Lexer lx("--timeout 9");
    auto stream = lx.scan();
    REQUIRE(stream.size() == 2);
    REQUIRE(stream[0].getAttr() == Token::Type_E::LongName);
    REQUIRE(stream[0].getName() == "timeout");
    REQUIRE_FALSE(stream[0].hasInlineEq());
    REQUIRE(stream[1].getAttr() == Token::Type_E::Integer);
    REQUIRE(stream[1].getName() == "9");
}

TEST_CASE("lexer: scan() --opt=val 内联等号", "[lexer][typed]")
{
    Lexer lx("--opt=42");
    auto stream = lx.scan();
    // LongName("opt",inlineEq) Delimiter("=") Integer("42") = 3 tokens
    REQUIRE(stream.size() == 3);
    REQUIRE(stream[0].getAttr() == Token::Type_E::LongName);
    REQUIRE(stream[0].hasInlineEq());
    REQUIRE(stream[1].getAttr() == Token::Type_E::Delimiter);
    REQUIRE(stream[1].getName() == "=");
    REQUIRE(stream[2].getAttr() == Token::Type_E::Integer);
    REQUIRE(stream[2].getName() == "42");
}

TEST_CASE("lexer: scan() -t=5 短选项等号分隔", "[lexer][typed]")
{
    Lexer lx("-t=5");
    auto stream = lx.scan();
    REQUIRE(stream.size() == 3);
    REQUIRE(stream[0].getAttr() == Token::Type_E::ShortName);
    REQUIRE(stream[1].getName() == "=");
    REQUIRE(stream[2].getName() == "5");
}

TEST_CASE("lexer: scanEnvLine 批量产出", "[lexer][typed]")
{
    auto stream = Lexer::scanEnvLine("A::B::C=D");
    REQUIRE(stream.size() == 7);
    REQUIRE(stream[0].getAttr() == Token::Type_E::Variable);
    REQUIRE(stream[1].getAttr() == Token::Type_E::ScopeSep);
    REQUIRE(stream[5].getName() == "=");
    REQUIRE(stream[6].getName() == "D");
}

/* ==================== Lexer 增量 pop() ==================== */

TEST_CASE("lexer: pop() 逐个吐出 Token，行为与 scan() 等价", "[lexer][pop]")
{
    Lexer lx("--timeout 9");
    std::vector<Token> collected;
    while (lx.hasNext()) { collected.push_back(lx.pop()); }
    auto batch = Lexer("--timeout 9").scan();
    REQUIRE(collected.size() == batch.size());
    for (size_t i = 0; i < collected.size(); ++i) {
        REQUIRE(collected[i].getAttr() == batch[i].getAttr());
        REQUIRE(collected[i].getName() == batch[i].getName());
    }
}

TEST_CASE("lexer: pop() 多行 argv 逐个吐出", "[lexer][pop]")
{
    Lexer lx("--hello a --timeout 9");
    size_t count = 0;
    while (lx.hasNext()) { lx.pop(); ++count; }
    REQUIRE(count == 4);
}

/* ==================== scanStrings 往返 ==================== */

TEST_CASE("lexer: scanStrings 还原归一化串的类型化信息", "[lexer][typed]")
{
    Tokens_T normalized{ "--timeout=9", "-abc", "hello" };
    auto stream = Lexer::scanStrings(normalized);
    // "--timeout=9" → LongName(timeout,eq) Delimiter("=") Integer(9) = 3 tokens
    // "-abc" → ContinualName(abc) = 1 token
    // "hello" → Variable(hello) = 1 token
    REQUIRE(stream.size() == 5);
    REQUIRE(stream[0].getAttr() == Token::Type_E::LongName);
    REQUIRE(stream[0].hasInlineEq());
    REQUIRE(stream[1].getAttr() == Token::Type_E::Delimiter);
    REQUIRE(stream[1].getName() == "=");
    REQUIRE(stream[2].getAttr() == Token::Type_E::Integer);
    REQUIRE(stream[3].getAttr() == Token::Type_E::ContinualName);
    REQUIRE(stream[4].getAttr() == Token::Type_E::Variable);
}

TEST_CASE("lexer: tokens() 兼容外观行为不变", "[lexer]")
{
    auto toks = Lexer("--timeout=9 -abc hello").tokens();
    REQUIRE(toks.size() == 3);
    REQUIRE(toks.front() == "--timeout=9");
}
