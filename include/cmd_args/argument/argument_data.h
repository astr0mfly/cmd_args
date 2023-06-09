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

#include "cmd_args/base/def.h"

#include "argument.h"

CMD_ARGS_NAMESPACE_BEGIN

namespace detail {

class arg_mgr_itf
{
public:
    using TaskVisit_T = std::function<void(Argument const &)>;

    virtual std::string     doGetUsage() const                                          = 0;
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
    void doPush(Argument::ptr _Arg) override
    {
        auto seq = dynamic_cast<ArgumentPosition *>(_Arg.get())->getSeq();
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

struct ArgumentData
{
    named_arg_mgr    mgrName;
    position_arg_mgr mgrPos;
};

}  // namespace detail

using detail::ArgumentData;

CMD_ARGS_NAMESPACE_END