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
#include <utility>

#include "cmd_args/base/def.h"
#include "cmd_args/context/parse_fwd.h"

CMD_ARGS_NAMESPACE_BEGIN

/// @brief 表达式种类：Argument（参数）/ Option（选项）/ Env（环境变量）/ Conditional（条件关系）/ Dependency（依赖关系）
enum class ExpressionKind_E : char
{
    Argument,     ///< 命名/方位参数
    Option,       ///< 值选项/标志/短回路选项
    Env,          ///< 环境变量条目
    Conditional,  ///< 条件关系：-a?b:c（a 为真时激活 b，否则激活 c）
    Dependency    ///< 依赖关系：-a->b（先执行 a，再执行 b）
};

/**
 * @brief 表达式基类：Argument / OptionValue / OptionAction / environment 的统一视图。
 *
 * 解析管线中，各 Parser 把 token 分类为具体派生类型后，
 * 以 Expression 视角统一枚举（kind() 自动完成分类），再存入对应 Data。
 * Compiler 的输出产物 Model 即持有全部 Expression 派生对象。
 */
class Expression
{
public:
    /// @brief 构造表达式
    /// @param _Kind 表达式种类
    /// @param _Name 名字（参数名 / 选项长名 / 环境变量键）
    /// @param _Help 帮助文本（可选，默认空）
    Expression(ExpressionKind_E _Kind, Name_T _Name, Desc_T _Help = {})
        : enKind__(_Kind)
        , strName__(std::move(_Name))
        , strHelp__(std::move(_Help))
    {}

    virtual ~Expression() = default;

    /// @brief 获取表达式种类（分类依据，非虚）
    auto kind() const noexcept -> ExpressionKind_E { return enKind__; }

    /// @brief 获取表达式名字（虚函数，派生类可覆盖使用自有存储）
    virtual auto getName() const noexcept -> Name_T const & { return strName__; }

    /// @brief 获取帮助文本（虚函数，派生类可覆盖）
    virtual auto getHelp() const noexcept -> Desc_T const & { return strHelp__; }

protected:
    /// @brief 设置名字（保护：派生类 setter 使用）
    auto _setName(Name_T &&_Name) noexcept -> Expression &
    {
        strName__ = std::move(_Name);
        return *this;
    }

    /// @brief 设置帮助文本（保护：派生类 setter 使用）
    auto _setHelp(Desc_T &&_Help) noexcept -> Expression &
    {
        strHelp__ = std::move(_Help);
        return *this;
    }

private:
    ExpressionKind_E enKind__;  ///< 种类
    Name_T           strName__; ///< 名字
    Desc_T           strHelp__; ///< 帮助文本
};

CMD_ARGS_NAMESPACE_END
