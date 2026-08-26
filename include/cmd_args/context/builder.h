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

#include "declarations.h"
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

    /* ---------- 声明式批量注册 ---------- */

    /**
     * @brief 批量声明参数/选项/标志（声明式入口）
     * @tparam Args_T 参数包：ArgumentNamed / ArgumentPosition / OptionDef<T> / FlagDef
     * @param _Args 声明项（通过折叠表达式逐一分发）
     * @return Builder& 支持链式调用
     */
    template <typename... Args_T>
    Builder &declare(Args_T &&..._Args)
    {
        (_declareOne(std::forward<Args_T>(_Args)), ...);
        return *this;
    }

private:
    /// @brief 分发单个声明项到对应的 Builder 方法
    template <typename T>
    void _declareOne(T &&_Arg)
    {
        using Decay_T = std::remove_cvref_t<T>;
        if constexpr (std::is_same_v<Decay_T, ArgumentNamed>) {
            ArgumentBuilder::build(std::forward<T>(_Arg));
        }
        else if constexpr (std::is_same_v<Decay_T, ArgumentPosition>) {
            ArgumentBuilder::build(std::forward<T>(_Arg));
        }
        else if constexpr (std::is_same_v<Decay_T, FlagDef>) {
            OptionBuilder::addFlag(
                _Arg.shortName, std::move(_Arg.longName), std::move(_Arg.help));
        }
        else {
            static_assert(sizeof(Decay_T) == 0,
                "declare() 不支持的类型。可用：ArgumentNamed / ArgumentPosition / OptionDef<T> / FlagDef");
        }
    }

    /// @brief OptionDef<T> 特化：推导 T 并转发给 addOption<T>
    template <typename T>
    void _declareOne(OptionDef<T> &&_Def)
    {
        OptionBuilder::addOption<T>(
            _Def.shortName, std::move(_Def.longName),
            std::move(_Def.help), std::move(_Def.defaultValue));
    }
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