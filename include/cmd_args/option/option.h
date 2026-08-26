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

#include <algorithm>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

#include "cmd_args/base/def.h"
#include "cmd_args/base/util.h"
#include "cmd_args/expression/expression.h"

CMD_ARGS_NAMESPACE_BEGIN

namespace option_detail {

/// @brief 选项基结构体（短名 / 长名 / 帮助文本，构造期确定）
struct Option : public Expression
{
    using NameShort_T   = char;
    using NameLong_T    = std::string;
    using Desc_T        = std::string;

    NameShort_T cShortName = '\0';  ///< 短名（"-t" 的 't'；'\0' 表示无短名）
    NameLong_T  strLongName;        ///< 长名（"--test" 的 "test"）
    Desc_T      strHelp;            ///< 帮助文本

    /// @brief 构造选项（基类以 Option 种类登记名字与帮助）
    /// @param _SName 短名（'\0' 表示无短名）
    /// @param _LName 长名（不含 "--" 前缀）
    /// @param _Help 帮助文本
    Option(NameShort_T _SName, NameLong_T _LName, Desc_T _Help)
        : Expression(ExpressionKind_E::Option, _LName, _Help)
        , cShortName(_SName)
        , strLongName(std::move(_LName))
        , strHelp(std::move(_Help))
    {}

    virtual ~Option() = default;

    /// @brief 覆盖基类 getName()：返回自身 strLongName（避免双份存储同步问题）
    auto getName() const noexcept -> Name_T const & override { return strLongName; }

    /// @brief 覆盖基类 getHelp()：返回自身 strHelp
    auto getHelp() const noexcept -> Desc_T const & override { return strHelp; }
};

/**
 * @brief 值选项：带类型与默认值，比 Option 多出值/类型/状态标记。
 *
 * 优先级：命令行显式值 > 环境变量 > 内置默认值。
 * FIXME 对于 bool 选项，不需要默认值，有对应的命令行参数就是 TRUE；
 *       其他类型添加时需要默认值。
 */
struct OptionValue : Option
{
    using Self_T      = OptionValue;
    using Type_T      = std::string;
    using Value_T     = std::string;
    using NameEnv_T   = std::string;
    using FnOnSet_T   = std::function<void(Value_T const &)>;
    using ActionCmd_T = std::string;

    // ===== 公开取值 =====
    Type_T          strType;   ///< 类型标识（"bool" / "int" / "double" / "std::string"）
    mutable Value_T strValue;  ///< 当前值（默认值 / 环境变量兜底值 / CLI 显式值）

    // ===== 运行时状态 =====
    mutable bool    bExplicit__ = false;  ///< 是否被命令行显式赋值过（区别于默认值/环境变量兜底）
    NameEnv_T       strEnvName__;         ///< 绑定的环境变量名：命令行未显式给出时用该变量的值兜底
    bool            bExternal__ = false;  ///< 值来自外部配置（YAML），env 兜底跳过
    bool            bBuiltin__   = false;  ///< 懒注册的内置交互选项（--env-set 等）：不参与参数格式导出/配置序列化

    // ===== 构造期默认与钩子 =====
    Value_T     strDefault__;      ///< 构造时的内置默认值（loop reset 还原用）
    mutable FnOnSet_T fnOnSet__;  ///< 自定义 setter 钩子：命令行赋值成功后回调（用于 --env-set 等内置交互选项）
    ActionCmd_T strActionTemplate__;  ///< 绑定的系统调用命令模板（action）：解析时该选项被显式命中后立即执行，模板中的 {value} 占位符替换为该选项本次命中的值；空串表示未绑定

    /// @brief 构造值选项（含短名）
    /// @param _SName 短名（'\0' 表示无短名）
    /// @param _LName 长名
    /// @param _Help 帮助文本
    /// @param _Type 类型标识
    /// @param _Value 默认值（同时作为初始值）
    OptionValue(Option::NameShort_T _SName, Option::NameLong_T _LName, Option::Desc_T _Help, Type_T _Type, Value_T _Value)
        : Option(_SName, std::move(_LName), std::move(_Help))
        , strType(std::move(_Type))
        , strValue(_Value)
        , strDefault__(std::move(_Value))
    {}

    /// @brief 构造值选项（仅长名）
    /// @param _LName 长名
    /// @param _Help 帮助文本
    /// @param _Type 类型标识
    /// @param _Value 默认值（同时作为初始值）
    OptionValue(Option::NameLong_T _LName, Option::Desc_T _Help, Type_T _Type, Value_T _Value)
        : Option('\0', std::move(_LName), std::move(_Help))
        , strType(std::move(_Type))
        , strValue(_Value)
        , strDefault__(std::move(_Value))
    {}

    /// @brief 绑定环境变量：命令行显式值 > 环境变量 > 内置默认值
    /// @param _EnvName 环境变量名
    /// @return OptionValue& 支持链式调用
    auto env(NameEnv_T _EnvName) -> Self_T &
    {
        strEnvName__ = std::move(_EnvName);
        return *this;
    }

    /// @brief 绑定自定义赋值回调（--env-set 等内置交互选项）
    /// @param _Fn 命令行赋值成功后回调
    /// @return OptionValue& 支持链式调用
    auto onSet(FnOnSet_T _Fn) -> Self_T &
    {
        fnOnSet__ = std::move(_Fn);
        return *this;
    }

    /**
     * @brief 绑定系统调用 action（命令模板）
     * @param _CmdTemplate 命令模板，{value} 占位符替换为本次命中的值
     * @return OptionValue& 支持链式调用
     * @note 仅在 CLI 显式命中时触发（环境变量兜底 / YAML 外部赋值 / 内置默认均不触发）
     * @note 同一选项多次命中则每次执行；std::system 返回非 0 时打印告警，不中断解析
     */
    auto action(ActionCmd_T _CmdTemplate) -> Self_T &
    {
        strActionTemplate__ = std::move(_CmdTemplate);
        return *this;
    }

    /// @brief 外部赋值（来自配置文件 YAML）：写入值并标记 bExternal__
    /// @param _V 配置值
    /// @return OptionValue& 支持链式调用
    auto setExternal(Value_T const &_V) -> Self_T &
    {
        strValue    = _V;
        bExternal__ = true;
        return *this;
    }

    /// @brief 外部赋值（右值版本，支持移动语义）
    /// @param _V 配置值（会被移动）
    /// @return OptionValue& 支持链式调用
    auto setExternal(Value_T &&_V) -> Self_T &
    {
        strValue    = std::move(_V);
        bExternal__ = true;
        return *this;
    }

    /// @brief 还原为内置默认值、清除显式/外部标记（loop 每次调用前执行）
    /// @return OptionValue& 支持链式调用
    /// @note 还原 strValue 为 strDefault__，清除 bExplicit__ / bExternal__ 标记
    auto reset() noexcept -> Self_T &
    {
        strValue    = strDefault__;
        bExplicit__ = false;
        bExternal__ = false;
        return *this;
    }
};

/**
 * @brief 短回路选项：优先级比普通选项高，先得到响应；
 *        需要一个回调函数代表行动，命中后立即退出程序。
 */
struct OptionAction : Option
{
    using FnCallback_T = std::function<void(void)>;

    FnCallback_T fnCallback;  ///< 命中时立即执行的回调

    /// @brief 构造短回路选项（含短名）
    /// @param _SName 短名（'\0' 表示无短名）
    /// @param _LName 长名
    /// @param _Help 帮助文本
    /// @param _Cb 命中回调
    OptionAction(Option::NameShort_T _SName, Option::NameLong_T _LName, Option::Desc_T _Help, FnCallback_T _Cb)
        : Option(_SName, std::move(_LName), std::move(_Help))
        , fnCallback(std::move(_Cb))
    {}

    /// @brief 构造短回路选项（仅长名）
    /// @param _LName 长名
    /// @param _Help 帮助文本
    /// @param _Cb 命中回调
    OptionAction(Option::NameLong_T _LName, Option::Desc_T _Help, FnCallback_T _Cb)
        : Option('\0', std::move(_LName), std::move(_Help))
        , fnCallback(std::move(_Cb))
    {}
};

}  // namespace option_detail

using option_detail::Option;
using option_detail::OptionValue;
using option_detail::OptionAction;

CMD_ARGS_NAMESPACE_END
