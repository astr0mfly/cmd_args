#include "catch2/catch.hpp"

#include <string>
#include <vector>

#include "cmd_args/cmd_args.hpp"
#include "cmd_args/expression/expression_recognizer.h"

using namespace cmd_args;

/* ==================== ExpressionRecognizer：Token → Expression 类型化归约 ==================== */

TEST_CASE("recognizer: --name=value 内联值立即识别为 Option", "[classifier]")
{
    Model model;
    OptionBuilder ob(&model.opts);
    ob.addOption<int>("timeout", "t", 30);
    OptionParser op(&model.opts);
    ArgumentParser ap(&model.args);
    EnvParser ep(&model.envs);
    ExpressionRecognizer rec(&op, &ap, &ep);

    TokenStream_T toks{
        Token(Token::Type_E::LongName, "timeout", 0),
        Token(Token::Type_E::Variable, "9", 0)
    };
    toks[0].setInlineEq(true);

    std::vector<ClassifiedExpr> out;
    rec.feed(toks[0], out);
    REQUIRE(out.empty());
    REQUIRE(rec.hasPending());

    rec.feed(toks[1], out);
    REQUIRE(out.size() == 1);
    REQUIRE(out[0].kind == ExpressionKind_E::Option);
    REQUIRE(out[0].name == "timeout");
    REQUIRE(out[0].value == "9");
    REQUIRE(out[0].bExplicit);
    REQUIRE_FALSE(rec.hasPending());
}

TEST_CASE("recognizer: --name value 分离值两个 token", "[classifier]")
{
    Model model;
    OptionBuilder ob(&model.opts);
    ob.addOption<int>("timeout", "t", 30);
    OptionParser op(&model.opts);
    ArgumentParser ap(&model.args);
    EnvParser ep(&model.envs);
    ExpressionRecognizer rec(&op, &ap, &ep);

    std::vector<ClassifiedExpr> out;
    rec.feed(Token(Token::Type_E::LongName, "timeout", 0), out);
    REQUIRE(out.empty());
    REQUIRE(rec.hasPending());

    rec.feed(Token(Token::Type_E::Variable, "9", 0), out);
    REQUIRE(out.size() == 1);
    REQUIRE(out[0].value == "9");
}

TEST_CASE("recognizer: -abc 聚合 bool 选项", "[classifier]")
{
    Model model;
    OptionBuilder ob(&model.opts);
    ob.addFlag('a', "ena", "a");
    ob.addFlag('b', "enb", "b");
    ob.addFlag('c', "enc", "c");
    OptionParser op(&model.opts);
    ArgumentParser ap(&model.args);
    EnvParser ep(&model.envs);
    ExpressionRecognizer rec(&op, &ap, &ep);

    std::vector<ClassifiedExpr> out;
    rec.feed(Token(Token::Type_E::ContinualName, "abc", 0), out);
    REQUIRE(out.empty());
    REQUIRE(rec.getBufferSize() == 1);

    rec.feed(Token(Token::Type_E::Variable, "x", 0), out);
    REQUIRE(out.size() == 2);
    REQUIRE(out[0].kind == ExpressionKind_E::Option);
    REQUIRE(out[0].cShortName == 'a');
    REQUIRE(out[0].strRaw == "-abc");
    REQUIRE(out[1].bPositional);
}

TEST_CASE("recognizer: 关系 -a->b 识别为 Dependency", "[classifier]")
{
    Model model;
    OptionBuilder ob(&model.opts);
    ob.addFlag('a', "ena", "a");
    ob.addFlag('b', "enb", "b");
    OptionParser op(&model.opts);
    ArgumentParser ap(&model.args);
    EnvParser ep(&model.envs);
    ExpressionRecognizer rec(&op, &ap, &ep);

    std::vector<ClassifiedExpr> out;
    rec.feed(Token(Token::Type_E::ContinualName, "a", 0), out);
    rec.feed(Token(Token::Type_E::ContinualName, ">b", 0), out);
    REQUIRE(out.size() == 1);
    REQUIRE(out[0].kind == ExpressionKind_E::Dependency);
    REQUIRE(out[0].strDepSource == "a");
    REQUIRE(out[0].strDepTarget == "b");
}

TEST_CASE("recognizer: 方位参数识别", "[classifier]")
{
    Model model;
    OptionBuilder ob(&model.opts);
    ob.addOption<int>("timeout", "t", 30);
    OptionParser op(&model.opts);
    ArgumentParser ap(&model.args);
    EnvParser ep(&model.envs);
    ExpressionRecognizer rec(&op, &ap, &ep);

    std::vector<ClassifiedExpr> out;
    rec.feed(Token(Token::Type_E::Variable, "hello", 0), out);
    REQUIRE(out.size() == 1);
    REQUIRE(out[0].kind == ExpressionKind_E::Argument);
    REQUIRE(out[0].bPositional);
    REQUIRE(out[0].value == "hello");
}

TEST_CASE("recognizer: recognizeAll 批量识别", "[classifier]")
{
    Model model;
    OptionBuilder ob(&model.opts);
    ob.addOption<int>("timeout", "t", 30);
    OptionParser op(&model.opts);
    ArgumentParser ap(&model.args);
    EnvParser ep(&model.envs);
    ExpressionRecognizer rec(&op, &ap, &ep);

    TokenStream_T toks{
        Token(Token::Type_E::LongName, "timeout", 0),
        Token(Token::Type_E::Variable, "9", 0),
        Token(Token::Type_E::Variable, "hello", 0)
    };
    auto exprs = rec.recognizeAll(toks);
    REQUIRE(exprs.size() == 2);
    REQUIRE(exprs[0].kind == ExpressionKind_E::Option);
    REQUIRE(exprs[0].value == "9");
    REQUIRE(exprs[1].bPositional);
}

TEST_CASE("recognizer: 流尾 pending 缺值产出 valueless Expression", "[classifier]")
{
    Model model;
    OptionBuilder ob(&model.opts);
    ob.addOption<int>("timeout", "t", 30);
    OptionParser op(&model.opts);
    ArgumentParser ap(&model.args);
    EnvParser ep(&model.envs);
    ExpressionRecognizer rec(&op, &ap, &ep);

    std::vector<ClassifiedExpr> out;
    rec.feed(Token(Token::Type_E::LongName, "timeout", 0), out);
    rec.finish(out);
    REQUIRE(out.size() == 1);
    REQUIRE(out[0].value.empty());
}

TEST_CASE("recognizer: aggregate -abc 后跟 positional 走快速路径", "[classifier]")
{
    Model model;
    OptionBuilder ob(&model.opts);
    ob.addFlag('a', "ena", "a");
    ob.addFlag('b', "enb", "b");
    OptionParser op(&model.opts);
    ArgumentParser ap(&model.args);
    EnvParser ep(&model.envs);
    ExpressionRecognizer rec(&op, &ap, &ep);

    TokenStream_T toks{
        Token(Token::Type_E::ContinualName, "ab", 0),
        Token(Token::Type_E::Variable, "val", 0),
        Token(Token::Type_E::Variable, "extra", 0)
    };
    auto exprs = rec.recognizeAll(toks);
    // 聚合头 + 两个方位参数：快速路径只提前吐出聚合候选，不会吞掉后续 token
    REQUIRE(exprs.size() == 3);
    REQUIRE(exprs[0].kind == ExpressionKind_E::Option);
    REQUIRE(exprs[0].cShortName == 'a');
    REQUIRE(exprs[1].bPositional);
    REQUIRE(exprs[1].name == "val");
    REQUIRE(exprs[2].bPositional);
    REQUIRE(exprs[2].name == "extra");
}


