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

#include <string>
#include <vector>

#include "cmd_args/base/def.h"
#include "cmd_args/expression/expression.h"

CMD_ARGS_NAMESPACE_BEGIN

/**
 * @brief 分类后的表达式中间表示：多个 Token 经模式匹配后产出的结构化数据。
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

    // ===== 新增：类型化识别扩展 =====
    std::vector<std::string> vecSpacePath;  ///< Env 空间级联路径（A::B::C 的 ["A","B"]，不含叶子）
    std::string strRaw;                     ///< 原始词文本（聚合串展开 / 错误消息用）
    bool        bPositional = false;        ///< true = 方位参数（name 即原词，不查注册表）

    /// @brief Env 全路径键（"A::B::C" 形式）
    auto fullPath() const -> std::string
    {
        std::string key;
        for (auto const &sp : vecSpacePath) { key += sp; key += ENV_SCOPE_SEP; }
        key += name;
        return key;
    }
};

CMD_ARGS_NAMESPACE_END
