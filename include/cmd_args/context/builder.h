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

#include "cmd_args/argument/argument_builder.h"
#include "cmd_args/option/option_builder.h"

#include "model.h"

CMD_ARGS_NAMESPACE_BEGIN

/*
    构筑参数，选项，环境变量的对象
*/
class Builder : public ArgumentBuilder, private OptionBuilder
{
public:
    Builder(Model *_Data)
        : ArgumentBuilder(&_Data->m_args__)
        , OptionBuilder(&_Data->m_opts__)
    {}
    ~Builder();

    template <typename T>
    void build(Group<T> &&_Builds)
    {
        for (auto const &e : _Builds) { ArgumentBuilder::build(e); }
    }

    template <typename T>
    void build(Argument::Name_T _Long, Argument::Desc_T _Desc)
    {
        static_assert(std::is_base_of_v<Argument, T>, "only argument types are supported here");
        ArgumentBuilder::build<T>(_Long, _Desc);
    }

    using ArgumentBuilder::build;
    using OptionBuilder::add_flag;
    using OptionBuilder::add_option;
    using OptionBuilder::add_short_circuit;
};

struct BuildHelper
{
    Model *pCon = nullptr;

    BuildHelper(Model *_Data)
        : pCon(_Data)
    {}

    BuildHelper &operator()(ArgumentNamed &&_Arg)
    {
        Builder(pCon).build(std::move(_Arg));
        return *this;
    }
    BuildHelper &operator()(ArgumentNamed const &_Arg)
    {
        Builder(pCon).build(_Arg);
        return *this;
    }
    BuildHelper &operator()(Argument &&_Arg)
    {
        Builder(pCon).build(ArgumentNamed(std::move(_Arg)));
        return *this;
    }
    BuildHelper &operator()(Argument const &_Arg)
    {
        Builder(pCon).build(ArgumentNamed(_Arg));
        return *this;
    }
};

Builder::~Builder() {}

CMD_ARGS_NAMESPACE_END