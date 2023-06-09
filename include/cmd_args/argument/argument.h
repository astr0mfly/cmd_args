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
#include <typeindex>
#include <typeinfo>

#include "cmd_args/base/util.h"

CMD_ARGS_NAMESPACE_BEGIN

/*
    Argument 是必须提供的，没有对应的参数会报错
*/
namespace detail {

class Argument
{
public:
    using ptr     = std::unique_ptr<Argument>;
    using Name_T  = std::string;
    using Type_T  = std::reference_wrapper<const std::type_info>;
    using Value_T = std::string;
    using Desc_T  = std::string;

    Argument() = default;
    Argument(std::string _name, std::string _Help)
        : m_strName__(std::move(_name))
        , m_strHelp__(std::move(_Help))
        , m_idType__(typeid(bool))
    {}

    Argument(std::string _Name, std::string _Help, std::type_info &&_Type, Value_T _Default = "")
        : m_strName__(std::move(_Name))
        , m_strHelp__(std::move(_Help))
        , m_idType__(_Type)
        , m_strValue__(std::move(_Default))
    {}
    Argument(Argument &&_Other) noexcept
        : m_strName__(std::move(_Other.m_strName__))
        , m_strHelp__(std::move(_Other.m_strHelp__))
        , m_idType__(_Other.m_idType__)
        , m_strValue__(std::move(_Other.m_strValue__))
    {}
    Argument(Argument const &_Other) noexcept
        : m_strName__(_Other.m_strName__)
        , m_strHelp__(_Other.m_strHelp__)
        , m_idType__(_Other.m_idType__)
        , m_strValue__(_Other.m_strValue__)
    {}
    Argument &operator=(Argument &&_Rhs) noexcept
    {
        if (this == &_Rhs) return *this;
        m_strName__  = std::move(_Rhs.m_strName__);
        m_strHelp__  = std::move(_Rhs.m_strHelp__);
        m_idType__   = std::move(_Rhs.m_idType__);
        m_strValue__ = std::move(_Rhs.m_strValue__);

        return *this;
    }
    Argument &operator=(Argument const &_Rhs) noexcept
    {
        if (this == &_Rhs) return *this;
        m_strName__  = _Rhs.m_strName__;
        m_strHelp__  = _Rhs.m_strHelp__;
        m_idType__   = _Rhs.m_idType__;
        m_strValue__ = _Rhs.m_strValue__;

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
        m_idType__ = typeid(T);
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

    Argument       copy() const { return *this; }
    Name_T const  &getName() const noexcept { return m_strName__; }
    Desc_T const  &getHelp() const noexcept { return m_strHelp__; }
    char const    *getType() const noexcept { return m_idType__.get().name(); }
    Value_T const &getValue() const noexcept { return m_strValue__; }

private:
    Name_T          m_strName__;
    std::string     m_strHelp__;
    Type_T          m_idType__ = typeid(void);
    mutable Value_T m_strValue__;
};
/*
ArgumentNamed， 命名参数，指的是按照参数名字来设置值，“--name=valens”

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
};

/*
ArgumentPosition, 方位参数，指的是，命令行解析完毕之后，依次把剩下的值传给方位参数
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
    size_t            getSeq() const noexcept { return iSeq; }
};

}  // namespace detail

using detail::Argument;
using detail::ArgumentNamed;
using detail::ArgumentPosition;

CMD_ARGS_NAMESPACE_END