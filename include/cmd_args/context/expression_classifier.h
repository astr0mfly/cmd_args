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
 * @brief 分类后的表达式中间表示：Token 经分类后产出的结构化数据。
 *
 * 与 Expression 基类的区别：
 * - Expression 是 Model 中持久存储的实体（Argument/OptionValue/OptionAction/environment）
 * - ClassifiedExpr 是解析管线中的瞬态中间表示，携带 Token 的分类结果与元数据
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
 * @brief 表达式分类器：把 Token 流分类为 ClassifiedExpr 中间表示。
 *
 * 提供两个入口：
 * - classifyOne()：流式模式，逐 Token 分类（Interpreter 使用）
 * - classifyAll()：批量模式，一次分类全部 Token（Compiler 使用）
 *
 * 分类器是无状态的——每次 classify 调用独立，不依赖前次调用的残留状态。
 * 唯一的内部状态是「当前 Token 是否以 '-' 开头」的判断逻辑，
 * 这与 Token 的 toText() 产出一致。
 */
class ExpressionClassifier
{
public:
    /// @brief 流式分类：单个 Token → ClassifiedExpr（Interpreter __reduceStack 使用）
    /// @param _Tok 待分类的 Token
    /// @return 分类成功返回 ClassifiedExpr，无法分类返回 nullopt（如空白 token）
    auto classifyOne(detail::Token const &_Tok) const -> std::optional<ClassifiedExpr>
    {
        std::string text = _Tok.toText();
        if (text.empty()) return std::nullopt;

        return __classifyText(text, _Tok.hasInlineEq());
    }

    /// @brief 批量分类：Token 列表 → ClassifiedExpr 列表（Compiler 使用）
    /// @param _Tokens 待分类的 Token 流（已归一化）
    /// @return 分类后的 ClassifiedExpr 列表
    auto classifyAll(Tokens_T const &_Tokens) const -> std::vector<ClassifiedExpr>
    {
        std::vector<ClassifiedExpr> result;
        result.reserve(_Tokens.size());
        for (auto const &tok : _Tokens) {
            auto expr = classifyOne(tok);
            if (expr) { result.emplace_back(std::move(*expr)); }
        }
        return result;
    }

private:
    /// @brief 核心分类逻辑：根据 token 文本判定 ExpressionKind_E
    auto __classifyText(std::string const &_Text, bool _InlineEq) const -> ClassifiedExpr
    {
        ClassifiedExpr expr;
        expr.bExplicit = true;

        // ── 1. 关系语法：-x?y:z 或 -x->y ──
        if (__isRelationSyntax(_Text)) {
            return __classifyRelation(_Text);
        }

        // ── 2. 长选项：--name[=value] ──
        if (util::starts_with(_Text, "--") && _Text.size() > 2) {
            auto body = _Text.substr(2);
            auto eq   = body.find('=');
            expr.kind = ExpressionKind_E::Option;
            expr.name = (eq == std::string::npos) ? body : body.substr(0, eq);
            if (eq != std::string::npos) { expr.value = body.substr(eq + 1); }
            return expr;
        }

        // ── 3. 短选项：-x / -xvalue / -abc 聚合 ──
        if (util::starts_with(_Text, "-") && _Text.size() >= 2) {
            expr.kind       = ExpressionKind_E::Option;
            expr.cShortName = _Text[1];
            expr.name       = std::string(1, _Text[1]);
            return expr;
        }

        // ── 4. 环境变量：KEY=VALUE 形态（无 '-' 前缀且含 '='） ──
        if (!util::starts_with(_Text, "-")) {
            auto eq = _Text.find('=');
            if (eq != std::string::npos) {
                expr.kind  = ExpressionKind_E::Env;
                expr.name  = _Text.substr(0, eq);
                expr.value = _Text.substr(eq + 1);
                return expr;
            }
            // 普通 variable token → 方位参数
            expr.kind  = ExpressionKind_E::Argument;
            expr.name  = _Text;
            expr.value = _Text;
            return expr;
        }

        // ── 5. 兜底：普通 token → 方位参数 ──
        expr.kind  = ExpressionKind_E::Argument;
        expr.name  = _Text;
        expr.value = _Text;
        return expr;
    }

    /// @brief 判断 token 文本是否为关系语法（-x?y:z / -x?y:_ / -x->y）
    bool __isRelationSyntax(std::string const &_Text) const
    {
        if (_Text.size() < 4) return false;  // 最短关系：-a?b（5字符）/ -a->b（5字符）
        if (_Text[0] != '-') return false;

        // 查找 '?' 或 '->' 分隔符
        for (size_t i = 1; i < _Text.size(); ++i) {
            if (_Text[i] == '?') return true;
            if (_Text[i] == '-' && i + 1 < _Text.size() && _Text[i + 1] == '>') return true;
        }
        return false;
    }

    /// @brief 分类关系语法 token
    auto __classifyRelation(std::string const &_Text) const -> ClassifiedExpr
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
                // -a?b 无冒号：等价 -a?b:_（条件为真时激活 b，否则无操作）
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
};

CMD_ARGS_NAMESPACE_END
