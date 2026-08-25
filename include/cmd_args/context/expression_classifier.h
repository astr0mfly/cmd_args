/*
 * Copyright[2026][ValenciaFly]
 * Email: pengwanring@live.com
 *
 *     Licensed under the Apache License,
 *     Version 2.0(the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "cmd_args/base/def.h"
#include "cmd_args/base/util.h"
#include "cmd_args/context/classified_expr.h"
#include "cmd_args/context/expression.h"
#include "cmd_args/context/lexer.h"
#include "cmd_args/option/option_parser.h"
#include "cmd_args/argument/argument_parser.h"
#include "cmd_args/env/env_parser.h"

CMD_ARGS_NAMESPACE_BEGIN

/**
 * @brief 表达式识别器：编译前端——把 Token 流通过模式匹配识别为 Expression 序列。
 *
 * 编译前端思想：
 * - 词法分析（Lexer/Scanner）：字符流 → Token 流（已完成）
 * - 语法分析（ExpressionRecognizer）：Token 流 → Expression 流（本类）
 * - 语义分析（Compiler/Interpreter）：Expression 流 → Model（下游）
 *
 * 一个 Expression 由一个或多个 Token 组成：
 * - --timeout=9     → LongName("timeout=9")                         → Option(name="timeout", value="9")
 * - --timeout 9     → LongName("timeout") + Variable("9")           → Option(name="timeout", value="9")
 * - -t 9            → ShortName("t") + Variable("9")                → Option(shortName='t', value="9")
 * - -abc            → ContinualName("abc")                           → Option(shortName='a', isBool=true)
 * - -a?b:c          → ContinualName("a") + "?" + Var("b") + ":" + Var("c") → Conditional
 * - -a->b           → ContinualName("a") + "->" + Var("b")          → Dependency
 * - KEY=VALUE       → Variable("KEY=VALUE")                          → Env(key, value)
 * - hello           → Variable("hello")                              → Argument(name="hello")
 *
 * 使用方式：
 * - 流式模式（Interpreter）：逐 token 调用 feed()，返回 nullopt 表示还在收集，
 *   返回 ClassifiedExpr 表示识别出一个完整 Expression
 * - 批量模式（Compiler）：调用 recognizeAll() 一次识别全部
 */
class ExpressionRecognizer
{
public:
    /// @brief 默认构造（使用内置硬编码规则，向后兼容）
    ExpressionRecognizer() = default;

    /// @brief 注入 Parser 规则构造（委托给 Parser 的 recognize() 方法）
    /// @param _Opt OptionParser 引用
    /// @param _Arg ArgumentParser 引用
    /// @param _Env EnvParser 引用
    ExpressionRecognizer(OptionParser *_Opt, ArgumentParser *_Arg, EnvParser *_Env)
        : pOptParser__(_Opt)
        , pArgParser__(_Arg)
        , pEnvParser__(_Env)
        , bUseParserRules__(_Opt != nullptr || _Arg != nullptr || _Env != nullptr)
    {}
    /// @brief 流式识别：逐 Token 喂入，凑满一个 Expression 时返回
    /// @param _Tok 词法单元
    /// @return 识别出完整 Expression 时返回之，否则 nullopt（仍在收集）
    auto feed(detail::Token const &_Tok) -> std::optional<ClassifiedExpr>
    {
        std::string text = _Tok.toText();
        if (text.empty()) return std::nullopt;

        // 空白 token 跳过
        if (util::trim(text).empty()) return std::nullopt;

        return __feedText(text, _Tok.hasInlineEq());
    }

    /// @brief 批量识别：Token 字符串列表 → ClassifiedExpr 列表
    /// @param _Tokens 已归一化的 Token 流（Tokens_T = std::list<std::string>）
    /// @return 识别后的 ClassifiedExpr 列表
    auto recognizeAll(Tokens_T const &_Tokens) -> std::vector<ClassifiedExpr>
    {
        std::vector<ClassifiedExpr> result;
        reset();
        for (auto const &tok : _Tokens) {
            if (tok.empty()) continue;
            auto expr = __feedText(tok, false);
            if (expr) { result.emplace_back(std::move(*expr)); }
        }
        // 处理残留的 pending（如流尾缺值）
        __flushPending(result);
        return result;
    }

    /// @brief 是否有未完成的 Expression（等待取值的选项/参数）
    bool hasPending() const noexcept { return !strPendingKey__.empty(); }

    /// @brief 复位内部状态（每次新行/新 parse 前调用）
    void reset()
    {
        vecBuffer__.clear();
        strPendingKey__.clear();
        enPendingKind__    = PendingKind_E::None;
        bPendingInlineEq__ = false;
        bOptEnd__          = false;
    }

private:
    /// @brief 挂起方类型
    enum class PendingKind_E : char
    {
        None,      ///< 无挂起
        LongOpt,   ///< --name 等待值
        ShortOpt,  ///< -x 等待值
        NamedArg   ///< --name (参数) 等待值
    };

    /// @brief 核心识别逻辑：根据 token 文本进行模式匹配
    auto __feedText(std::string const &_Text, bool _InlineEq) -> std::optional<ClassifiedExpr>
    {
        // ── 1. 有挂起取值方：当前 token 就是值 ──
        if (!strPendingKey__.empty()) {
            return __consumePendingValue(_Text);
        }

        // ── 2. '--' 之后一律方位参数 ──
        if (bOptEnd__) {
            return __makeArgument(_Text);
        }

        // ── 3. '--' 本身：切换纯方位模式 ──
        if (_Text == "--") {
            bOptEnd__ = true;
            return std::nullopt;  // '--' 自身不产出 Expression
        }

        // ── 4. 委托 Parser 规则（当注入时） ──
        if (bUseParserRules__) {
            auto expr = __tryParserRules(_Text);
            if (expr) return expr;
        }

        // ── 5. 长选项：--name[=value] ──
        if (util::starts_with(_Text, "--") && _Text.size() > 2) {
            return __recognizeLongOption(_Text, _InlineEq);
        }

        // ── 6. 短选项/关系语法：-x / -abc / -a?b:c / -a->b ──
        if (util::starts_with(_Text, "-") && _Text.size() >= 2) {
            return __recognizeShortOptionOrRelation(_Text);
        }

        // ── 7. 普通 token：环境变量或方位参数 ──
        return __recognizeVariableOrEnv(_Text);
    }

    /// @brief 尝试使用 Parser 规则识别 token
    auto __tryParserRules(std::string const &_Text) -> std::optional<ClassifiedExpr>
    {
        // 构造临时 token 列表供 Parser.recognize() 使用
        Tokens_T tmp{ _Text };
        auto it = tmp.begin();

        // 按优先级尝试各 Parser 规则
        if (pOptParser__) {
            auto expr = pOptParser__->recognize(it, tmp);
            if (expr) {
                // 处理挂起逻辑（长选项无值时）
                if (expr->kind == ExpressionKind_E::Option && expr->value.empty()
                    && util::starts_with(_Text, "--") && _Text.size() > 2) {
                    strPendingKey__    = expr->name;
                    enPendingKind__    = PendingKind_E::LongOpt;
                    bPendingInlineEq__ = false;
                    return std::nullopt;  // 还在收集
                }
                return expr;
            }
        }

        if (pArgParser__) {
            auto expr = pArgParser__->recognize(it, tmp);
            if (expr) {
                // 处理挂起逻辑（命名参数无值时）
                if (expr->kind == ExpressionKind_E::Argument && expr->value.empty()
                    && util::starts_with(_Text, "--") && _Text.size() > 2) {
                    strPendingKey__    = expr->name;
                    enPendingKind__    = PendingKind_E::NamedArg;
                    bPendingInlineEq__ = false;
                    return std::nullopt;  // 还在收集
                }
                return expr;
            }
        }

        if (pEnvParser__) {
            auto expr = pEnvParser__->recognize(_Text);
            if (expr) return expr;
        }

        return std::nullopt;
    }

    /// @brief 识别长选项模式：--name[=value] 或 --name value
    auto __recognizeLongOption(std::string const &_Text, bool _InlineEq) -> std::optional<ClassifiedExpr>
    {
        auto body = _Text.substr(2);
        auto eq   = body.find('=');

        ClassifiedExpr expr;
        expr.bExplicit = true;
        expr.kind      = ExpressionKind_E::Option;

        if (eq != std::string::npos) {
            // --name=value：内联值，立即完成
            expr.name  = body.substr(0, eq);
            expr.value = body.substr(eq + 1);
            return expr;
        }

        // --name：等待下一个 token 取值（非 bool 选项）
        expr.name = body;
        strPendingKey__    = body;
        enPendingKind__    = PendingKind_E::LongOpt;
        bPendingInlineEq__ = _InlineEq;
        return std::nullopt;  // 还在收集
    }

    /// @brief 识别短选项或关系语法模式
    auto __recognizeShortOptionOrRelation(std::string const &_Text) -> std::optional<ClassifiedExpr>
    {
        // 检测关系语法：-a?b:c / -a?b:_ / -a->b
        if (__isRelationSyntax(_Text)) {
            return __recognizeRelation(_Text);
        }

        // -x（单个短选项）或 -abc（聚合）
        ClassifiedExpr expr;
        expr.bExplicit  = true;
        expr.kind       = ExpressionKind_E::Option;
        expr.cShortName = _Text[1];
        expr.name       = std::string(1, _Text[1]);
        expr.bIsBool    = true;  // 聚合串中每个字符都是 bool

        // 如果是 -x=value 形式，提取内联值
        if (_Text.size() > 2 && _Text[2] == '=') {
            expr.value    = _Text.substr(3);
            expr.bIsBool  = false;
            return expr;
        }

        // -x / -abc：bool 聚合（多个字符 = 多个 bool 标志）
        // 下游 OptionParser 根据注册类型决定是否取值
        return expr;
    }

    /// @brief 识别关系语法模式
    auto __recognizeRelation(std::string const &_Text) -> ClassifiedExpr
    {
        ClassifiedExpr expr;
        expr.bExplicit = true;

        // 查找 '?' 或 '->'
        size_t qPos = std::string::npos;
        size_t dPos = std::string::npos;
        for (size_t i = 1; i < _Text.size(); ++i) {
            if (_Text[i] == '?' && qPos == std::string::npos) { qPos = i; break; }
            if (_Text[i] == '-' && i + 1 < _Text.size() && _Text[i + 1] == '>') {
                dPos = i;
                break;
            }
        }

        if (qPos != std::string::npos) {
            // -a?b:c 或 -a?b:_ 或 -a?b
            expr.kind         = ExpressionKind_E::Conditional;
            expr.strCondition = _Text.substr(1, qPos - 1);  // a
            auto colonPos     = _Text.find(':', qPos);
            if (colonPos != std::string::npos) {
                expr.strThenBranch = _Text.substr(qPos + 1, colonPos - qPos - 1);  // b
                expr.strElseBranch = _Text.substr(colonPos + 1);  // c 或 "_"
            }
            else {
                expr.strThenBranch = _Text.substr(qPos + 1);
                expr.strElseBranch = "_";
            }
        }
        else if (dPos != std::string::npos) {
            // -a->b
            expr.kind         = ExpressionKind_E::Dependency;
            expr.strDepSource = _Text.substr(1, dPos - 1);  // a
            expr.strDepTarget = _Text.substr(dPos + 2);      // b
        }

        return expr;
    }

    /// @brief 识别普通变量：环境变量 KEY=VALUE 或方位参数
    auto __recognizeVariableOrEnv(std::string const &_Text) -> std::optional<ClassifiedExpr>
    {
        // 环境变量：含 '='
        auto eq = _Text.find('=');
        if (eq != std::string::npos) {
            ClassifiedExpr expr;
            expr.kind      = ExpressionKind_E::Env;
            expr.name      = _Text.substr(0, eq);
            expr.value     = _Text.substr(eq + 1);
            expr.bExplicit = true;
            return expr;
        }

        // 方位参数
        return __makeArgument(_Text);
    }

    /// @brief 构造方位参数 Expression
    auto __makeArgument(std::string const &_Text) -> ClassifiedExpr
    {
        ClassifiedExpr expr;
        expr.kind      = ExpressionKind_E::Argument;
        expr.name      = _Text;
        expr.value     = _Text;
        expr.bExplicit = true;
        return expr;
    }

    /// @brief 消费挂起取值：当前 token 作为值完成 Expression
    auto __consumePendingValue(std::string const &_Text) -> std::optional<ClassifiedExpr>
    {
        ClassifiedExpr expr;
        expr.bExplicit = true;

        if (enPendingKind__ == PendingKind_E::LongOpt) {
            // 拒绝 dash 开头值（除非挂起方带内联 '='）
            if (!bPendingInlineEq__ && util::starts_with(_Text, "-")) {
                // dash 值拒绝：抛出错误或返回错误 Expression
                // 此处简化处理：返回 NEED_VALUE 错误
                strPendingKey__.clear();
                enPendingKind__    = PendingKind_E::None;
                bPendingInlineEq__ = false;
                // 返回一个特殊的错误 Expression
                expr.kind = ExpressionKind_E::Option;
                expr.name = strPendingKey__;
                return expr;
            }
            expr.kind = ExpressionKind_E::Option;
            expr.name = strPendingKey__;
            expr.value = _Text;
        }
        else if (enPendingKind__ == PendingKind_E::ShortOpt) {
            expr.kind       = ExpressionKind_E::Option;
            expr.cShortName = strPendingKey__[0];
            expr.name       = strPendingKey__;
            expr.value      = _Text;
        }
        else if (enPendingKind__ == PendingKind_E::NamedArg) {
            expr.kind  = ExpressionKind_E::Argument;
            expr.name  = strPendingKey__;
            expr.value = _Text;
        }

        strPendingKey__.clear();
        enPendingKind__    = PendingKind_E::None;
        bPendingInlineEq__ = false;
        return expr;
    }

    /// @brief 判断 token 文本是否为关系语法（-x?y:z / -x?y:_ / -x->y）
    bool __isRelationSyntax(std::string const &_Text) const
    {
        if (_Text.size() < 4) return false;
        if (_Text[0] != '-') return false;

        for (size_t i = 1; i < _Text.size(); ++i) {
            if (_Text[i] == '?') return true;
            if (_Text[i] == '-' && i + 1 < _Text.size() && _Text[i + 1] == '>') return true;
        }
        return false;
    }

    /// @brief 刷新残留 pending（流尾缺值时生成错误 Expression）
    void __flushPending(std::vector<ClassifiedExpr> &_Result)
    {
        if (strPendingKey__.empty()) return;

        // 残留 pending = 流尾缺值
        ClassifiedExpr expr;
        expr.bExplicit = true;
        expr.kind      = ExpressionKind_E::Option;
        expr.name      = strPendingKey__;
        // value 为空表示缺值（下游报 NEED_VALUE）
        _Result.emplace_back(std::move(expr));

        strPendingKey__.clear();
        enPendingKind__    = PendingKind_E::None;
        bPendingInlineEq__ = false;
    }

    // ===== 内部状态 =====
    std::vector<detail::Token> vecBuffer__;               ///< Token 缓冲区（预留，当前未使用）
    std::string                strPendingKey__    = {};    ///< 等待取值的选项/参数名
    PendingKind_E              enPendingKind__    = PendingKind_E::None;  ///< 挂起类型
    bool                       bPendingInlineEq__ = false; ///< 挂起方是否带内联 '='
    bool                       bOptEnd__          = false; ///< '--' 之后（纯方位模式）

    // ===== Parser 规则引用（可选，为 nullptr 时使用内置硬编码规则） =====
    OptionParser  *pOptParser__     = nullptr;  ///< 选项解析器（可选）
    ArgumentParser *pArgParser__    = nullptr;  ///< 参数解析器（可选）
    EnvParser     *pEnvParser__     = nullptr;  ///< 环境变量解析器（可选）
    bool           bUseParserRules__ = false;   ///< 是否使用 Parser 规则
};

CMD_ARGS_NAMESPACE_END
