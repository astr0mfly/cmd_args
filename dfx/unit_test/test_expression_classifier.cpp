#include "catch2/catch.hpp"

#include <string>
#include <vector>

#include "cmd_args/cmd_args.hpp"
#include "cmd_args/context/expression_classifier.h"

using namespace cmd_args;
using cmd_args::detail::Token;

/* ==================== ExpressionClassifier：Token → ClassifiedExpr ==================== */

TEST_CASE("classifier: 长选项分类为 Option Expression", "[classifier]")
{
    ExpressionClassifier classifier;

    // --timeout=9 → Option, name="timeout", value="9"
    // Token::toText() for LongName returns "--" + name, so name must include "=9"
    Token tok1(Token::Type_E::LongName, "timeout=9", 0);
    tok1.setInlineEq(true);
    auto r1 = classifier.classifyOne(tok1);
    REQUIRE(r1.has_value());
    REQUIRE(r1->kind == ExpressionKind_E::Option);
    REQUIRE(r1->name == "timeout");
    REQUIRE(r1->value == "9");
    REQUIRE(r1->bExplicit);

    // --timeout (无值) → Option, name="timeout", value=""
    Token tok2(Token::Type_E::LongName, "timeout", 0);
    auto r2 = classifier.classifyOne(tok2);
    REQUIRE(r2.has_value());
    REQUIRE(r2->kind == ExpressionKind_E::Option);
    REQUIRE(r2->name == "timeout");
    REQUIRE(r2->value.empty());
}

TEST_CASE("classifier: 短选项分类为 Option Expression", "[classifier]")
{
    ExpressionClassifier classifier;

    // -t → Option, cShortName='t'
    Token tok1(Token::Type_E::ShortName, "t", 0);
    auto r1 = classifier.classifyOne(tok1);
    REQUIRE(r1.has_value());
    REQUIRE(r1->kind == ExpressionKind_E::Option);
    REQUIRE(r1->cShortName == 't');

    // -abc → Option, cShortName='a'（聚合串首字符）
    Token tok2(Token::Type_E::ContinualName, "abc", 0);
    auto r2 = classifier.classifyOne(tok2);
    REQUIRE(r2.has_value());
    REQUIRE(r2->kind == ExpressionKind_E::Option);
    REQUIRE(r2->cShortName == 'a');
}

TEST_CASE("classifier: 方位参数分类为 Argument Expression", "[classifier]")
{
    ExpressionClassifier classifier;

    Token tok(Token::Type_E::Variable, "world", 0);
    auto r = classifier.classifyOne(tok);
    REQUIRE(r.has_value());
    REQUIRE(r->kind == ExpressionKind_E::Argument);
    REQUIRE(r->name == "world");
    REQUIRE(r->value == "world");
}

TEST_CASE("classifier: 环境变量 KEY=VALUE 分类为 Env Expression", "[classifier]")
{
    ExpressionClassifier classifier;

    // 环境变量在 Lexer 归一化后以 "KEY=VALUE" plain token 形式出现
    Token tok(Token::Type_E::Variable, "HOME=/root", 0);
    auto r = classifier.classifyOne(tok);
    REQUIRE(r.has_value());
    REQUIRE(r->kind == ExpressionKind_E::Env);
    REQUIRE(r->name == "HOME");
    REQUIRE(r->value == "/root");
}

TEST_CASE("classifier: 条件关系 -a?b:c 分类为 Conditional", "[classifier]")
{
    ExpressionClassifier classifier;

    // -verbose?debug:quiet → Conditional
    // Token::toText() for ContinualName returns "-" + name, so name = "verbose?debug:quiet"
    Token tok(Token::Type_E::ContinualName, "verbose?debug:quiet", 0);
    auto r = classifier.classifyOne(tok);
    REQUIRE(r.has_value());
    REQUIRE(r->kind == ExpressionKind_E::Conditional);
    REQUIRE(r->strCondition == "verbose");
    REQUIRE(r->strThenBranch == "debug");
    REQUIRE(r->strElseBranch == "quiet");
}

TEST_CASE("classifier: 条件关系 -a?b:_ 分类为 Conditional（无操作分支）", "[classifier]")
{
    ExpressionClassifier classifier;

    // -verbose?debug:_ → Conditional, else="_"
    Token tok(Token::Type_E::ContinualName, "verbose?debug:_", 0);
    auto r = classifier.classifyOne(tok);
    REQUIRE(r.has_value());
    REQUIRE(r->kind == ExpressionKind_E::Conditional);
    REQUIRE(r->strCondition == "verbose");
    REQUIRE(r->strThenBranch == "debug");
    REQUIRE(r->strElseBranch == "_");
}

TEST_CASE("classifier: 条件关系 -a?b 无冒号等价 -a?b:_", "[classifier]")
{
    ExpressionClassifier classifier;

    Token tok(Token::Type_E::ContinualName, "verbose?debug", 0);
    auto r = classifier.classifyOne(tok);
    REQUIRE(r.has_value());
    REQUIRE(r->kind == ExpressionKind_E::Conditional);
    REQUIRE(r->strCondition == "verbose");
    REQUIRE(r->strThenBranch == "debug");
    REQUIRE(r->strElseBranch == "_");
}

TEST_CASE("classifier: 依赖关系 -a->b 分类为 Dependency", "[classifier]")
{
    ExpressionClassifier classifier;

    // -compile->link → Dependency
    Token tok(Token::Type_E::ContinualName, "compile->link", 0);
    auto r = classifier.classifyOne(tok);
    REQUIRE(r.has_value());
    REQUIRE(r->kind == ExpressionKind_E::Dependency);
    REQUIRE(r->strDepSource == "compile");
    REQUIRE(r->strDepTarget == "link");
}

TEST_CASE("classifier: classifyAll 批量分类", "[classifier]")
{
    ExpressionClassifier classifier;

    Tokens_T toks{ "--timeout=9", "-abc", "world" };
    auto results = classifier.classifyAll(toks);

    REQUIRE(results.size() == 3);
    REQUIRE(results[0].kind == ExpressionKind_E::Option);
    REQUIRE(results[0].name == "timeout");
    REQUIRE(results[1].kind == ExpressionKind_E::Option);
    REQUIRE(results[1].cShortName == 'a');
    REQUIRE(results[2].kind == ExpressionKind_E::Argument);
    REQUIRE(results[2].name == "world");
}

TEST_CASE("classifier: 空 token 返回 nullopt", "[classifier]")
{
    ExpressionClassifier classifier;

    Token tok(Token::Type_E::Unknown, "", 0);
    auto r = classifier.classifyOne(tok);
    REQUIRE_FALSE(r.has_value());
}
