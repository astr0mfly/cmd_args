/* 
 * Copyright [2023] [ValenciaFly]
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

#include <type_traits>

#include "cmd_args/base/def.h"
#include "cmd_args/base/util.h"

#include "cmd_args/error/builder_error.h"

#include "argument_data.h"

CMD_ARGS_NAMESPACE_BEGIN

/*
关于参数的构筑
接受各种入参，并且做一定的检查
*/
class ArgumentBuilder
{
public:
    ArgumentBuilder(ArgumentData *_Data) noexcept
        : m_pData__(_Data)
    {}

    template <typename T>
    void build(std::string _Name, std::string _Help)
    {
        if constexpr (std::is_same_v<T, ArgumentNamed>) {
            check(_Name, _Help);
            __checkDuplicate(_Name);
            m_pData__->mgrName.doPush(std::make_unique<ArgumentNamed>(_Name, _Help));
        }
        else if constexpr (std::is_same_v<T, ArgumentPosition>) {
            m_pData__->mgrPos.doPush(
                std::make_unique<ArgumentPosition>(m_pData__->mgrPos.doSize(), _Name, _Help));
        }
        else {
            static_assert(true, "Failed to construct Argument::ptr.");
        }
    }

    void build(ArgumentNamed &&_Arg)
    {
        check(_Arg);
        __checkDuplicate(_Arg.getName());
        m_pData__->mgrName.doPush(std::make_unique<ArgumentNamed>(std::move(_Arg)));
    }

    void build(ArgumentNamed const &_Arg)
    {
        check(_Arg);
        __checkDuplicate(_Arg.getName());
        m_pData__->mgrName.doPush(std::make_unique<ArgumentNamed>(_Arg));
    }

    void build(Group<ArgumentNamed> &&_Builds)
    {
        for (auto &a : _Builds) { build(a); }
    }

    static void check(std::string const &_Name, std::string const &_Help)
    {
        if (_Name.empty()) throw ErrorArgument(BuildError_E::NEED_NAME);
        if (_Help.empty()) throw ErrorArgument(BuildError_E::NEED_HELP);
    }

    static void check(ArgumentNamed const &_Arg)
    {
        check(_Arg.getName(), _Arg.getHelp());
    }

private:
    void __checkDuplicate(std::string const &_Name) const
    {
        if (m_pData__->mgrName.doFind(_Name)) {
            throw ErrorArgument(BuildError_E::DUPLICATE_NAME);
        }
    }

private:
    ArgumentData *m_pData__ = nullptr;
};

CMD_ARGS_NAMESPACE_END