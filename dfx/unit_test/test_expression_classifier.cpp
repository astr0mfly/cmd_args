#include "catch2/catch.hpp"

#include <string>
#include <vector>

#include "cmd_args/cmd_args.hpp"
#include "cmd_args/context/expression_classifier.h"

using namespace cmd_args;
using cmd_args::detail::Token;

/* ==================== ExpressionRecognizer：Token 序列 → Expression 模式匹配 ==================== */

TEST_CASE("recognizer: --name=value 内联值立即识别为 Option", "[classifier]")
{
    ExpressionRecognizer rec;

    // --timeout=9：LongName 内联值
    Token tok(Token::Type_E::LongName, "timeout=9", 0);
    tok.setInlineEq(true);
    auto r = rec.feed(tok);
    REQUIRE(r.has_value());
    REQUIRE(r->kind == ExpressionKind_E::Option);
    REQUIRE(r->name == "timeout");
    REQUIRE(r->value == "9");
    REQUIRE(r->bExplicit);
    REQUIRE_FALSE(rec.hasPending());
}

TEST_CASE("recognizer: --name value 分离值需要两个 token", "[classifier]")
{
    ExpressionRecognizer rec;

    // 第一个 token：--timeout（无值，挂起）
    Token tok1(Token::Type_E::LongName, "timeout", 0);
    auto r1 = rec.feed(tok1);
    REQUIRE_FALSE(r1.has_value());  // 还在收集
    REQUIRE(rec.hasPending());

    // 第二个 token：9（值）
    Token tok2(Token::Type_E::Variable, "9", 0);
    auto r2 = rec.feed(tok2);
    REQUIRE(r2.has_value());
    REQUIRE(r2->kind == ExpressionKind_E::Option);
    REQUIRE(r2->name == "timeout");
    REQUIRE(r2->value == "9");
    REQUIRE_FALSE(rec.hasPending());
}

TEST_CASE("recognizer: -x 短选项识别为 Option", "[classifier]")
{
    ExpressionRecognizer rec;

    // -t：短选项
    Token tok(Token::Type_E::ShortName, "t", 0);
    auto r = rec.feed(tok);
    REQUIRE(r.has_value());
    REQUIRE(r->kind == ExpressionKind_E::Option);
    REQUIRE(r->cShortName == 't');
}

TEST_CASE("recognizer: -abc 聚合 bool 选项", "[classifier]")
{
    ExpressionRecognizer rec;

    // -abc：聚合串
    Token tok(Token::Type_E::ContinualName, "abc", 0);
    auto r = rec.feed(tok);
    REQUIRE(r.has_value());
    REQUIRE(r->kind == ExpressionKind_E::Option);
    REQUIRE(r->cShortName == 'a');
    REQUIRE(r->bIsBool);
}

TEST_CASE("recognizer: -t=value 短选项内联值", "[classifier]")
{
    ExpressionRecognizer rec;

    // -t=5：短选项内联值
    Token tok(Token::Type_E::ContinualName, "t=5", 0);
    auto r = rec.feed(tok);
    REQUIRE(r.has_value());
    REQUIRE(r->kind == ExpressionKind_E::Option);
    REQUIRE(r->cShortName == 't');
    REQUIRE(r->value == "5");
    REQUIRE_FALSE(r->bIsBool);
}

TEST_CASE("recognizer: 方位参数识别", "[classifier]")
{
    ExpressionRecognizer rec;

    Token tok(Token::Type_E::Variable, "world", 0);
    auto r = rec.feed(tok);
    REQUIRE(r.has_value());
    REQUIRE(r->kind == ExpressionKind_E::Argument);
    REQUIRE(r->name == "world");
    REQUIRE(r->value == "world");
}

TEST_CASE("recognizer: 环境变量 KEY=VALUE 识别", "[classifier]")
{
    ExpressionRecognizer rec;

    Token tok(Token::Type_E::Variable, "HOME=/root", 0);
    auto r = rec.feed(tok);
    REQUIRE(r.has_value());
    REQUIRE(r->kind == ExpressionKind_E::Env);
    REQUIRE(r->name == "HOME");
    REQUIRE(r->value == "/root");
}

TEST_CASE("recognizer: -- 终止选项区后全部为方位参数", "[classifier]")
{
    ExpressionRecognizer rec;

    // --
    Token tok1(Token::Type_E::Delimiter, "--", 0);
    auto r1 = rec.feed(tok1);
    REQUIRE_FALSE(r1.has_value());  // '--' 自身不产出 Expression
    REQUIRE_FALSE(rec.hasPending());

    // -x（在 -- 之后应作为方位参数）
    Token tok2(Token::Type_E::ContinualName, "x", 0);
    auto r2 = rec.feed(tok2);
    REQUIRE(r2.has_value());
    REQUIRE(r2->kind == ExpressionKind_E::Argument);
    REQUIRE(r2->name == "-x");
}

TEST_CASE("recognizer: 条件关系 -a?b:c 识别为 Conditional", "[classifier]")
{
    ExpressionRecognizer rec;

    Token tok(Token::Type_E::ContinualName, "verbose?debug:quiet", 0);
    auto r = rec.feed(tok);
    REQUIRE(r.has_value());
    REQUIRE(r->kind == ExpressionKind_E::Conditional);
    REQUIRE(r->strCondition == "verbose");
    REQUIRE(r->strThenBranch == "debug");
    REQUIRE(r->strElseBranch == "quiet");
}

TEST_CASE("recognizer: 条件关系 -a?b:_ 无操作分支", "[classifier]")
{
    ExpressionRecognizer rec;

    Token tok(Token::Type_E::ContinualName, "verbose?debug:_", 0);
    auto r = rec.feed(tok);
    REQUIRE(r.has_value());
    REQUIRE(r->kind == ExpressionKind_E::Conditional);
    REQUIRE(r->strElseBranch == "_");
}

TEST_CASE("recognizer: 依赖关系 -a->b 识别为 Dependency", "[classifier]")
{
    ExpressionRecognizer rec;

    Token tok(Token::Type_E::ContinualName, "compile->link", 0);
    auto r = rec.feed(tok);
    REQUIRE(r.has_value());
    REQUIRE(r->kind == ExpressionKind_E::Dependency);
    REQUIRE(r->strDepSource == "compile");
    REQUIRE(r->strDepTarget == "link");
}

TEST_CASE("recognizer: recognizeAll 批量识别多个 Expression", "[classifier]")
{
    ExpressionRecognizer rec;

    Tokens_T toks{ "--timeout=9", "-abc", "world" };
    auto results = rec.recognizeAll(toks);

    REQUIRE(results.size() == 3);
    REQUIRE(results[0].kind == ExpressionKind_E::Option);
    REQUIRE(results[0].name == "timeout");
    REQUIRE(results[0].value == "9");
    REQUIRE(results[1].kind == ExpressionKind_E::Option);
    REQUIRE(results[1].cShortName == 'a');
    REQUIRE(results[2].kind == ExpressionKind_E::Argument);
    REQUIRE(results[2].name == "world");
}

TEST_CASE("recognizer: recognizeAll 多 token Expression（--name value）", "[classifier]")
{
    ExpressionRecognizer rec;

    Tokens_T toks{ "--timeout", "42", "world" };
    auto results = rec.recognizeAll(toks);

    REQUIRE(results.size() == 2);
    REQUIRE(results[0].kind == ExpressionKind_E::Option);
    REQUIRE(results[0].name == "timeout");
    REQUIRE(results[0].value == "42");
    REQUIRE(results[1].kind == ExpressionKind_E::Argument);
    REQUIRE(results[1].name == "world");
}

TEST_CASE("recognizer: reset 后状态清空", "[classifier]")
{
    ExpressionRecognizer rec;

    Token tok1(Token::Type_E::LongName, "timeout", 0);
    rec.feed(tok1);
    REQUIRE(rec.hasPending());

    rec.reset();
    REQUIRE_FALSE(rec.hasPending());
}

TEST_CASE("recognizer: 空 token 被跳过", "[classifier]")
{
    ExpressionRecognizer rec;

    Token tok(Token::Type_E::Unknown, "", 0);
    auto r = rec.feed(tok);
    REQUIRE_FALSE(r.has_value());
}

TEST_CASE("recognizer: 多 Expression 连续识别（--a 1 --b 2）", "[classifier]")
{
    ExpressionRecognizer rec;

    Token tok1(Token::Type_E::LongName, "a", 0);
    auto r1 = rec.feed(tok1);
    REQUIRE_FALSE(r1.has_value());  // 挂起

    Token tok2(Token::Type_E::Variable, "1", 0);
    auto r2 = rec.feed(tok2);
    REQUIRE(r2.has_value());
    REQUIRE(r2->name == "a");
    REQUIRE(r2->value == "1");

    Token tok3(Token::Type_E::LongName, "b", 0);
    auto r3 = rec.feed(tok3);
    REQUIRE_FALSE(r3.has_value());  // 挂起

    Token tok4(Token::Type_E::Variable, "2", 0);
    auto r4 = rec.feed(tok4);
    REQUIRE(r4.has_value());
    REQUIRE(r4->name == "b");
    REQUIRE(r4->value == "2");
}
