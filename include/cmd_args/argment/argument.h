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

#include "cmd_args/base/errc.h"
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
    Argument &name(Name_T &&_Name)
    {
        m_strName__ = _Name;
        return *this;
    }
    Argument &help(Desc_T &&_V) noexcept
    {
        m_strHelp__ = std::move(_V);
        return *this;
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

    void check()
    {
        if (m_strName__.empty()) throw ErrorArgument(ArgError_E::NEED_NAME);
        if (m_strHelp__.empty()) throw ErrorArgument(ArgError_E::NEED_HELP);
    }

    void check(std::error_code &_Ec)
    {
        if (m_strName__.empty()) {
            _Ec = make_error_code(ArgError_E::NEED_NAME);
            return;
        }

        if (m_strHelp__.empty()) {
            _Ec = make_error_code(ArgError_E::NEED_HELP);
            return;
        }
    }

    Name_T const  &getName() const noexcept { return m_strName__; }
    Desc_T const  &getHelp() const noexcept { return m_strHelp__; }
    char const    *getType() const noexcept { return m_idType__.get().name(); }
    Value_T const &getValue() const noexcept { return m_strValue__; }

private:
    Name_T          m_strName__;
    std::string     m_strHelp__;
    Type_T          m_idType__;
    mutable Value_T m_strValue__;
};
/*
ArgumentNamed， 命名参数，指的是按照参数名字来设置值，“--name=valens”

*/
struct ArgumentNamed : public Argument
{
    using Argument::Argument;
};

/*
position_argument, 方位参数，指的是，命令行解析完毕之后，依次把剩下的值传给方位参数
*/
struct position_argument : public Argument
{
    size_t iSeq = 0;

    template <typename... Args_T>
    position_argument(size_t _Seq, Args_T &&..._Args)
        : Argument(std::forward<Args_T>(_Args)...)
        , iSeq(_Seq)
    {}
    bool operator<(position_argument const &_Rhs) const noexcept { return iSeq < _Rhs.iSeq; }
    position_argument &No(size_t _Idx) { iSeq = _Idx; }
    size_t             getSeq() const noexcept { return iSeq; }
};

class arg_mgr_itf
{
public:
    using TaskVisit_T                          = std::function<void(Argument const &)>;
    virtual std::string     doGetUsage() const = 0;
    virtual bool            doFind(std::string const &_Key) const noexcept              = 0;
    virtual bool            doEmpty() const noexcept                                    = 0;
    virtual size_t          doSize() const noexcept                                     = 0;
    virtual void            doVisit(TaskVisit_T &&_Fn) const                            = 0;
    virtual void            doGetHelp(std::ostringstream &_Out, std::size_t _Max) const = 0;
    virtual Argument const *doGet(std::string const &_Key) const                        = 0;
    virtual void            doPush(Argument::ptr)                                       = 0;
};

/*
    命名参数管理，该形式的参数拥有命名，需要被指定设置。
*/
class named_arg_mgr : arg_mgr_itf
{
public:
    void parse(std::string const &_Line)
    {
        auto &&[k, v] = util::parse_line(_Line);
        auto it       = mapArgs.find(k);
        if (it == mapArgs.end()) { throw ErrorParse(ParseError_E::UNKNWON_ARGS); }

        if (v.empty()) { throw ErrorParse(ParseError_E::NEED_VALUE); }

        it->second->set(std::move(v));
    }

    void parse(std::error_code &_Ec, std::string const &_Line)
    {
        auto &&[k, v] = util::parse_line(_Line);
        auto it       = mapArgs.find(k);
        if (it == mapArgs.end()) {
            _Ec = make_error_code(ParseError_E::UNKNWON_ARGS);
            return;
        }

        if (v.empty()) {
            _Ec = make_error_code(ParseError_E::NEED_VALUE);
            return;
        }

        it->second->set(std::move(v));
    }

    void doPush(Argument::ptr _Arg) override { mapArgs.emplace(_Arg->getName(), std::move(_Arg)); }

    std::string doGetUsage() const override
    {
        std::ostringstream oss;
        for (auto &e : mapArgs) { oss << " [=" << e.second->getName() << ']'; }
        return oss.str();
    }

    bool doFind(std::string const &_Key) const noexcept override
    {
        return mapArgs.find(_Key) != mapArgs.cend();
    }
    bool   doEmpty() const noexcept override { return mapArgs.empty(); }
    size_t doSize() const noexcept override { return mapArgs.size(); }
    void   doVisit(TaskVisit_T &&_Fn) const override
    try {
        for (auto &&[k, a] : mapArgs) _Fn(*a);
    }
    catch (std::exception &ex) {
        std::cerr << __FILE__ << std::string(__FUNCTION__) << __LINE__ << "\n"
                  << "Failed to visit that " << ex.what() << std::endl;
    }
    catch (...) {
        std::cerr << __FILE__ << std::string(__FUNCTION__) << __LINE__ << "\n"
                  << "Failed to visit with unknown exception " << std::endl;
    }

    void doGetHelp(std::ostringstream &_Out, std::size_t _Max) const override
    {
        if (mapArgs.empty()) return;

        _Out << "\nNamed arguments:\n";
        _Max = 0;
        for (auto const &[k, arg] : mapArgs) { _Max = std::max(_Max, arg->getName().length()); }

        _Max = std::max(_Max, std::size_t(25));
        for (auto const &[k, arg] : mapArgs) {
            _Out << "  ";
            _Out << arg->getName();
            _Out << std::string(_Max - arg->getName().length(), ' ') << "(" << arg->getType()
                 << ") ";
            _Out << util::replace(arg->getHelp(), "\n", "\n" + std::string(_Max + 2, ' ')) << '\n';
        }
    }

    Argument const *doGet(std::string const &_Key) const override
    {
        auto it = mapArgs.find(_Key);
        if (it != mapArgs.end()) return nullptr;

        return it->second.get();
    }

private:
    using ConArgs_T = std::map<Argument::Name_T, Argument::ptr>;
    ConArgs_T mapArgs;
};

class position_arg_mgr : arg_mgr_itf
{
public:
    void parse(size_t _Pos, Argument::Value_T _Value)
    {
        auto it = mapArgs.find(_Pos);
        if (it == mapArgs.end()) { throw ErrorParse(ParseError_E::UNKNWON_ARGS); }

        if (_Value.empty()) { throw ErrorParse(ParseError_E::NEED_VALUE); }

        it->second->set(std::move(_Value));
    }

    void parse(std::error_code _Ec, size_t _Pos, Argument::Value_T _Value)
    {
        auto it = mapArgs.find(_Pos);
        if (it == mapArgs.end()) {
            _Ec = make_error_code(ParseError_E::UNKNWON_ARGS);
            return;
        }

        if (_Value.empty()) {
            _Ec = make_error_code(ParseError_E::NEED_VALUE);
            return;
        }

        it->second->set(std::move(_Value));
    }

    void doPush(Argument::ptr _Arg) override
    {
        auto seq = dynamic_cast<position_argument *>(_Arg.get())->getSeq();
        mapArgs.emplace(seq, std::move(_Arg));
    }
    std::string doGetUsage() const override
    {
        std::ostringstream oss;
        for (auto &e : mapArgs) { oss << " [" << e.second->getName() << '@' << e.first << ']'; }
        return oss.str();
    }
    bool doFind(std::string const &_Key) const noexcept override
    {
        return std::find_if(
                   mapArgs.cbegin(),
                   mapArgs.cend(),
                   [&_Key](decltype(mapArgs)::const_reference arg) {
                       return arg.second->getName() == _Key;
                   })
               != mapArgs.cend();
    }
    bool   doEmpty() const noexcept { return mapArgs.empty(); }
    size_t doSize() const noexcept override { return mapArgs.size(); }
    void   doVisit(TaskVisit_T &&_Fn) const override
    try {
        for (auto &&e : mapArgs) _Fn(*e.second);
    }
    catch (std::exception &ex) {
        std::cerr << __FILE__ << std::string(__FUNCTION__) << __LINE__ << "\n"
                  << "Failed to visit that " << ex.what() << std::endl;
    }
    catch (...) {
        std::cerr << __FILE__ << std::string(__FUNCTION__) << __LINE__ << "\n"
                  << "Failed to visit with unknown exception " << std::endl;
    }

    void doGetHelp(std::ostringstream &_Out, std::size_t _Max) const override
    {
        if (mapArgs.empty()) return;
        _Out << "\nPosition arguments:\n";
        _Max = 0;
        for (auto const &arg : mapArgs) { _Max = std::max(_Max, arg.second->getName().length()); }
        _Max = std::max(_Max, std::size_t(25));
        for (auto const &arg : mapArgs) {
            _Out << "  ";
            _Out << arg.second->getName();
            _Out << std::string(_Max - arg.second->getName().length(), ' ') << "("
                 << arg.second->getType() << ") ";
            _Out << util::replace(arg.second->getHelp(), "\n", "\n" + std::string(_Max + 2, ' '))
                 << '\n';
        }
    }

    Argument const *doGet(std::string const &_Key) const override
    {
        auto it = std::find_if(
            mapArgs.begin(),
            mapArgs.end(),
            [&](decltype(mapArgs)::const_reference _V) { return _V.second->getName() == _Key; });
        if (it != mapArgs.end()) return nullptr;

        return it->second.get();
    }

private:
    using ConArgs_T = std::map<size_t, Argument::ptr>;
    ConArgs_T mapArgs;
};

}  // namespace detail

using detail::Argument;
using detail::ArgumentNamed;
using detail::named_arg_mgr;
using detail::position_arg_mgr;
using detail::position_argument;

CMD_ARGS_NAMESPACE_END