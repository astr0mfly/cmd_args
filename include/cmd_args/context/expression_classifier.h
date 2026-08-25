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
#include "cmd_args/context/expression.h"
#include "cmd_args/context/lexer.h"

CMD_ARGS_NAMESPACE_BEGIN

/**
 * @brief 表达式中间表示：多个 Token 经模式匹配后产出的结构化数据。
 *
 * 编译前端视角：Token 是词法单元，Expression 是语法单元。
 * 一个 Expression 由一个或多个 Token 组成（如 --timeout=9 由 LongName + "=" + Variable 组成），
 * ExpressionRecognizer 通过模式匹配识别 Token 序列何时构成完整的 Expression。
 *
 * 与 Expression 基类的区别：
 * - Expression 基类是 Model 中持久存储的实体（Argument/OptionValue/OptionAction/environment）
 * - ClassifiedExpr 是解析管线中的瞬态中间表示
 * - 下游（Compiler/Interpreter）根据 kind 分支将 ClassifiedExpr apply 到 Model
 */
struct ClassifiedExpr
{
    ExpressionKind_E kind = ExpressionKind_E::Option;  ///< 分类种类

    // ===== 通用字段 =====
    std::string name;               ///< 选项长名 / 参数名 / 环境变量键
    std::string value;              ///< 值（选项值 / 参数值 / 环境变量值）
    bool        bExplicit = false;  ///< 是否被命令行显式赋值

    // ===== Option 专用 =====
    char        cShortName = '\0';  ///< 短名（'\0' 表示无短名）
    bool        bIsBool    = false; ///< 是否 bool 类型选项
    bool        bIsBuiltin = false; ///< 是否内置交互选项
    bool        bIsAction  = false; ///< 是否短回路选项（命中即回调）
    std::string strType;            ///< 类型标识（"bool" / "int" / "double" / "std::string"）

    // ===== Conditional 关系专用 =====
    std::string strCondition;       ///< 条件选项名（-a?b:c 中的 a）
    std::string strThenBranch;      ///< 条件为真时激活的选项（b）
    std::string strElseBranch;      ///< 条件为假时激活的选项（c 或 "_"）

    // ===== Dependency 关系专用 =====
    std::string strDepSource;       ///< 依赖源（-a->b 中的 a）
    std::string strDepTarget;       ///< 依赖目标（-a->b 中的 b）

    // ===== 环境变量专用 =====
    bool bEnvNew = false;           ///< 是否新建的环境变量

    /// @brief 是否为关系表达式（Conditional 或 Dependency）
    bool isRelation() const noexcept
    {
        return kind == ExpressionKind_E::Conditional
            || kind == ExpressionKind_E::Dependency;
    }
};

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

        // ── 4. 长选项：--name[=value] ──
        if (util::starts_with(_Text, "--") && _Text.size() > 2) {
            return __recognizeLongOption(_Text, _InlineEq);
        }

        // ── 5. 短选项/关系语法：-x / -abc / -a?b:c / -a->b ──
        if (util::starts_with(_Text, "-") && _Text.size() >= 2) {
            return __recognizeShortOptionOrRelation(_Text);
        }

        // ── 6. 普通 token：环境变量或方位参数 ──
        return __recognizeVariableOrEnv(_Text);
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
};

CMD_ARGS_NAMESPACE_END
