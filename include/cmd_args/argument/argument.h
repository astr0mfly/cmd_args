/*
 * Copyright[2023][ValenciaFly]
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

#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>

#include "cmd_args/base/util.h"

CMD_ARGS_NAMESPACE_BEGIN

/*
    Argument 基类：一条"参数"的载体（命名参数 / 方位参数共用）。
    - 名字 + 帮助 —— 构筑期填写，一经确定不再变化
    - 类型 + 默认值 —— type<T>() 定类型（空 = 未定型），构造时缓存默认值
    - 值（m_strValue__）—— 解析期被 CLI 赋值，或经 env() 绑定环境变量兜底，
      或经 set_external() 被配置文件（YAML）外部赋值
    - 标记：bExternal__（值来自配置，env 兜底跳过）；
      hasValue() 即"当前已持有非空值"
    - reset() 还原默认值并清外部标记（loop 每行调用前执行，等价一次 once 输入）
*/
namespace detail {

class Argument
{
public:
    using ptr     = std::unique_ptr<Argument>;
    using Name_T  = std::string;
    using Type_T  = std::string;
    using Value_T = std::string;
    using Desc_T  = std::string;

    Argument() = default;
    Argument(std::string _name, std::string _Help)
        : m_strName__(std::move(_name))
        , m_strHelp__(std::move(_Help))
    {}

    Argument(std::string _Name, std::string _Help, Type_T _Type, Value_T _Default = "")
        : m_strName__(std::move(_Name))
        , m_strHelp__(std::move(_Help))
        , m_strType__(std::move(_Type))
        , m_strDefault__(_Default)
        , m_strValue__(std::move(_Default))
    {}
    Argument(Argument &&_Other) noexcept
        : m_strName__(std::move(_Other.m_strName__))
        , m_strHelp__(std::move(_Other.m_strHelp__))
        , m_strType__(std::move(_Other.m_strType__))
        , m_strDefault__(std::move(_Other.m_strDefault__))
        , m_strValue__(std::move(_Other.m_strValue__))
        , m_strEnvName__(std::move(_Other.m_strEnvName__))
        , bExternal__(_Other.bExternal__)
    {}
    Argument(Argument const &_Other) noexcept
        : m_strName__(_Other.m_strName__)
        , m_strHelp__(_Other.m_strHelp__)
        , m_strType__(_Other.m_strType__)
        , m_strDefault__(_Other.m_strDefault__)
        , m_strValue__(_Other.m_strValue__)
        , m_strEnvName__(_Other.m_strEnvName__)
        , bExternal__(_Other.bExternal__)
    {}
    Argument &operator=(Argument &&_Rhs) noexcept
    {
        if (this == &_Rhs) return *this;
        m_strName__    = std::move(_Rhs.m_strName__);
        m_strHelp__    = std::move(_Rhs.m_strHelp__);
        m_strType__    = std::move(_Rhs.m_strType__);
        m_strDefault__ = std::move(_Rhs.m_strDefault__);
        m_strValue__   = std::move(_Rhs.m_strValue__);
        m_strEnvName__ = std::move(_Rhs.m_strEnvName__);
        bExternal__    = _Rhs.bExternal__;

        return *this;
    }
    Argument &operator=(Argument const &_Rhs) noexcept
    {
        if (this == &_Rhs) return *this;
        m_strName__    = _Rhs.m_strName__;
        m_strHelp__    = _Rhs.m_strHelp__;
        m_strType__    = _Rhs.m_strType__;
        m_strDefault__ = _Rhs.m_strDefault__;
        m_strValue__   = _Rhs.m_strValue__;
        m_strEnvName__ = _Rhs.m_strEnvName__;
        bExternal__    = _Rhs.bExternal__;

        return *this;
    }
    operator bool() const noexcept { return !m_strValue__.empty(); }
    virtual ~Argument() = default;

    bool      hasValue() const noexcept { return operator bool(); }
    Argument &name(Name_T &&_Name) &
    {
        m_strName__ = _Name;
        return *this;
    }
    Argument name(Name_T &&_Name) &&
    {
        m_strName__ = _Name;
        return std::move(*this);
    }
    Argument &help(Desc_T &&_V) &noexcept
    {
        m_strHelp__ = std::move(_V);
        return *this;
    }
    Argument help(Desc_T &&_V) &&noexcept
    {
        m_strHelp__ = std::move(_V);
        return std::move(*this);
    }
    template <typename T>
    Argument &type()
    {
        m_strType__ = util::type_string<T>();
        return *this;
    }
    Argument &type(Type_T &&_Type)
    {
        m_strType__ = std::move(_Type);
        return *this;
    }
    Argument &set(Value_T &&_V) noexcept
    {
        m_strValue__ = std::move(_V);
        return *this;
    }
    Argument &set(Value_T const &_V)
    {
        m_strValue__ = _V;
        return *this;
    }

    /* 外部赋值（来自配置文件 YAML）：写入值并标记 bExternal__，
       使 env 兜底跳过本参数（优先级 CLI > 配置 > 环境变量 > 默认值） */
    Argument &set_external(Value_T &&_V) noexcept
    {
        m_strValue__  = std::move(_V);
        bExternal__   = true;
        return *this;
    }
    Argument &set_external(Value_T const &_V)
    {
        m_strValue__ = _V;
        bExternal__  = true;
        return *this;
    }
    bool isExternal() const noexcept { return bExternal__; }

    /* 还原为内置默认值、清除外部标记（loop 每次调用前调用，等价一次 once 输入）；
       名称/说明/类型/env 绑定保持不变 */
    Argument &reset() noexcept
    {
        m_strValue__ = m_strDefault__;
        bExternal__  = false;
        return *this;
    }

    /* 绑定环境变量：命令行显式值 > 环境变量 > 未设置 */
    Argument &env(Name_T &&_EnvName) &
    {
        m_strEnvName__ = std::move(_EnvName);
        return *this;
    }
    Argument env(Name_T &&_EnvName) &&
    {
        m_strEnvName__ = std::move(_EnvName);
        return std::move(*this);
    }
    Argument &env(Name_T const &_EnvName) &
    {
        m_strEnvName__ = _EnvName;
        return *this;
    }
    Argument env(Name_T const &_EnvName) &&
    {
        m_strEnvName__ = _EnvName;
        return std::move(*this);
    }

    Argument       copy() const { return *this; }
    Name_T const  &getName() const noexcept { return m_strName__; }
    Desc_T const  &getHelp() const noexcept { return m_strHelp__; }
    Type_T const  &getType() const noexcept { return m_strType__; }
    Name_T const  &getEnvName() const noexcept { return m_strEnvName__; }
    Value_T const &getValue() const noexcept { return m_strValue__; }

private:
    Name_T          m_strName__;
    std::string     m_strHelp__;
    Type_T          m_strType__;  // 空 = 未定型，任何 get 均可
    Value_T         m_strDefault__;  // 构造时的内置默认值（loop reset 还原用）
    mutable Value_T m_strValue__;
    Name_T          m_strEnvName__;
    mutable bool    bExternal__ = false;  // 值来自外部配置（YAML）而非命令行/环境变量
};
/*
    ArgumentNamed：命名参数，按照参数名赋值，形如 "--name=value" / "--name value"。
*/
struct ArgumentNamed : public Argument
{
public:
    ArgumentNamed() = default;
    ArgumentNamed(std::string _Name, std::string _Help)
        : Argument(_Name, _Help)
    {}
    explicit ArgumentNamed(Argument &&_Other)
        : Argument(std::move(_Other))
    {}
    ArgumentNamed(Argument const &_Other)
        : Argument(_Other)
    {}
    using Argument::help;
    using Argument::name;
    using Argument::env;
};

/*
    ArgumentPosition：方位参数，命令行解析完毕后剩余 token 按声明顺序依次赋值；
    iSeq 记录声明顺序（0 起），用于数量校验与排序。
*/
struct ArgumentPosition : public Argument
{
    size_t iSeq = 0;

    template <typename... Args_T>
    ArgumentPosition(size_t _Seq, Args_T &&..._Args)
        : Argument(std::forward<Args_T>(_Args)...)
        , iSeq(_Seq)
    {}
    bool operator<(ArgumentPosition const &_Rhs) const noexcept { return iSeq < _Rhs.iSeq; }
    ArgumentPosition &No(size_t _Idx)
    {
        iSeq = _Idx;
        return *this;
    }
    size_t getSeq() const noexcept { return iSeq; }
};

}  // namespace detail

using detail::Argument;
using detail::ArgumentNamed;
using detail::ArgumentPosition;

CMD_ARGS_NAMESPACE_END