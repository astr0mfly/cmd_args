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
 * @brief 表达式识别器：编译前端——把 Token 流委托给 Parser 规则识别为 Expression 序列。
 *
 * 编译前端思想：
 * - 词法分析（Lexer/Scanner）：字符流 → Token 流（已完成）
 * - 语法分析（ExpressionRecognizer）：Token 流 → Expression 流（本类，委托 Parser 规则）
 * - 语义分析（Compiler/Interpreter）：Expression 流 → Model（下游）
 *
 * 本类不包含硬编码识别逻辑，完全委托给三个 Parser 的 recognize() 方法：
 * - OptionParser::recognize() 识别选项（--name[=value]、-x、-abc、-a?b:c、-a->b）
 * - ArgumentParser::recognize() 识别参数（--name=value、方位参数）
 * - EnvParser::recognize() 识别环境变量（KEY=VALUE）
 *
 * 使用方式：
 * - 流式模式（Interpreter）：逐 token 调用 feed()，返回 nullopt 表示还在收集，
 *   返回 ClassifiedExpr 表示识别出一个完整 Expression
 * - 批量模式（Compiler）：调用 recognizeAll() 一次识别全部
 */
class ExpressionRecognizer
{
public:
    /// @brief 构造表达式识别器（必须注入 Parser 规则）
    /// @param _Opt OptionParser 引用
    /// @param _Arg ArgumentParser 引用
    /// @param _Env EnvParser 引用
    ExpressionRecognizer(OptionParser *_Opt, ArgumentParser *_Arg, EnvParser *_Env)
        : pOptParser__(_Opt)
        , pArgParser__(_Arg)
        , pEnvParser__(_Env)
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

    /// @brief 核心识别逻辑：委托 Parser 规则
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

        // ── 4. 委托 Parser 规则 ──
        return __tryParserRules(_Text, _InlineEq);
    }

    /// @brief 委托 Parser 规则识别 token
    auto __tryParserRules(std::string const &_Text, bool _InlineEq) -> std::optional<ClassifiedExpr>
    {
        // 构造临时 token 列表供 Parser.recognize() 使用
        Tokens_T tmp{ _Text };
        auto it = tmp.begin();

        // 按优先级尝试各 Parser 规则
        // OptionParser 优先：识别选项（--name[=value]、-x、-abc、-a?b:c、-a->b）
        if (pOptParser__) {
            auto expr = pOptParser__->recognize(it, tmp);
            if (expr) {
                // 处理挂起逻辑：长选项无值时（--name 等待下一个 token）
                if (expr->kind == ExpressionKind_E::Option && expr->value.empty()
                    && util::starts_with(_Text, "--") && _Text.size() > 2) {
                    strPendingKey__    = expr->name;
                    enPendingKind__    = PendingKind_E::LongOpt;
                    bPendingInlineEq__ = _InlineEq;
                    return std::nullopt;  // 还在收集
                }
                return expr;
            }
        }

        // EnvParser 次之：识别环境变量（KEY=VALUE）— 必须在 ArgumentParser 之前
        // 因为 ArgumentParser 会将所有非 '-' 开头的 token 视为方位参数
        if (pEnvParser__) {
            auto expr = pEnvParser__->recognize(_Text);
            if (expr) return expr;
        }

        // ArgumentParser 最后：识别参数（--name=value、方位参数）
        if (pArgParser__) {
            auto expr = pArgParser__->recognize(it, tmp);
            if (expr) {
                // 处理挂起逻辑：命名参数无值时（--name 等待下一个 token）
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

        // 无 Parser 匹配：返回 nullopt
        return std::nullopt;
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
                strPendingKey__.clear();
                enPendingKind__    = PendingKind_E::None;
                bPendingInlineEq__ = false;
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

    // ===== Parser 规则引用（必须注入） =====
    OptionParser  *pOptParser__  = nullptr;  ///< 选项解析器
    ArgumentParser *pArgParser__ = nullptr;  ///< 参数解析器
    EnvParser     *pEnvParser__  = nullptr;  ///< 环境变量解析器
};

CMD_ARGS_NAMESPACE_END
