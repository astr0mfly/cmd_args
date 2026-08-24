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
    Builder：把 Context::build<>()/addOption()/addFlag() 的调用
    转交给 ArgumentBuilder / OptionBuilder，写入 Model。
    是管线第一步（Context → Builder → Lexer → Compiler → Model）的落点。
*/
class Builder : public ArgumentBuilder, private OptionBuilder
{
public:
    Builder(Model *_Data)
        : ArgumentBuilder(&_Data->args)
        , OptionBuilder(&_Data->opts)
    {}
    ~Builder();

    /* 一组同构构筑（Group） */
    template <typename T>
    void build(Group<T> &&_Builds)
    {
        for (auto const &e : _Builds) { ArgumentBuilder::build(e); }
    }

    /* build<ArgumentNamed>("x","h") / build<ArgumentPosition>(...) 形态 */
    template <typename T>
    void build(Argument::Name_T _Long, Argument::Desc_T _Desc)
    {
        static_assert(std::is_base_of_v<Argument, T>, "only argument types are supported here");
        ArgumentBuilder::build<T>(_Long, _Desc);
    }

    using ArgumentBuilder::build;
    using OptionBuilder::addFlag;
    using OptionBuilder::addOption;
    using OptionBuilder::addShortCircuit;
};

/*
    BuildHelper：Context::build() 的链式小助手，
    支持 build(ArgumentNamed{...})(ArgumentNamed{...}) 的链式对象构筑。
*/
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

inline Builder::~Builder() {}

CMD_ARGS_NAMESPACE_END