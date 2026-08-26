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

#include <type_traits>

#include "cmd_args/base/def.h"
#include "cmd_args/base/util.h"

#include "cmd_args/error/builder_error.h"

#include "argument_data.h"

CMD_ARGS_NAMESPACE_BEGIN

/*
    参数构筑：接受各种入参形态，做必要检查后写入 ArgumentData。
    - build<ArgumentNamed>("name", "help")   命名参数（重名检查 / 空名空帮助检查）
    - build<ArgumentPosition>(...)           方位参数（按追加顺序记录 iSeq）
    - build(ArgumentNamed &&/const &/Group)  对象/批量形态
*/
class ArgumentBuilder
{
public:
    /// @brief 构造参数构筑器
    /// @param _Data 底层参数数据仓库（非空）
    ArgumentBuilder(ArgumentData *_Data) noexcept
        : pData__(_Data)
    {}

    /**
     * @brief 构筑参数（按模板类型分发）
     * @tparam T 目标参数类型：ArgumentNamed（命名）或 ArgumentPosition（方位）
     * @param _Name 参数名称
     * @param _Help 帮助文本
     * @note 命名参数做重名/空名/空帮助检查；方位参数按追加顺序记录 iSeq
     */
    template <typename T>
    void build(Argument::Name_T _Name, Argument::Desc_T _Help)
    {
        if constexpr (std::is_same_v<T, ArgumentNamed>) {
            check(_Name, _Help);
            __checkDuplicate(_Name);
            pData__->mgrName.doPush(
                std::make_unique<ArgumentNamed>(std::move(_Name), std::move(_Help)));
        }
        else if constexpr (std::is_same_v<T, ArgumentPosition>) {
            pData__->mgrPos.doPush(
                std::make_unique<ArgumentPosition>(
                    pData__->mgrPos.doSize(), std::move(_Name), std::move(_Help)));
        }
        else {
            static_assert(true, "Failed to construct Argument::ptr.");
        }
    }

    /// @brief 构筑参数（右值对象形态）
    /// @param _Arg 命名参数对象（资源被移动接管）
    void build(ArgumentNamed &&_Arg)
    {
        check(_Arg);
        __checkDuplicate(_Arg.getName());
        pData__->mgrName.doPush(std::make_unique<ArgumentNamed>(std::move(_Arg)));
    }

    /// @brief 构筑参数（const 左值对象形态，拷贝）
    /// @param _Arg 命名参数对象（拷贝）
    void build(ArgumentNamed const &_Arg)
    {
        check(_Arg);
        __checkDuplicate(_Arg.getName());
        pData__->mgrName.doPush(std::make_unique<ArgumentNamed>(_Arg));
    }

    /// @brief 构筑位置参数（右值对象形态，自动分配序号）
    /// @param _Arg 位置参数对象（资源被移动接管）
    void build(ArgumentPosition &&_Arg)
    {
        _Arg.iSeq = pData__->mgrPos.doSize();
        pData__->mgrPos.doPush(std::make_unique<ArgumentPosition>(std::move(_Arg)));
    }

    /// @brief 构筑位置参数（const 左值对象形态，拷贝，自动分配序号）
    /// @param _Arg 位置参数对象（拷贝）
    void build(ArgumentPosition const &_Arg)
    {
        auto copy = _Arg;
        copy.iSeq = pData__->mgrPos.doSize();
        pData__->mgrPos.doPush(std::make_unique<ArgumentPosition>(std::move(copy)));
    }

    /// @brief 批量构筑参数
    /// @param _Builds 一组命名参数（逐条构筑）
    void build(Group<ArgumentNamed> &&_Builds)
    {
        for (auto &a : _Builds) { build(a); }
    }

    /// @brief 校验参数名/帮助非空
    /// @param _Name 参数名称
    /// @param _Help 帮助文本
    /// @note 空名抛 NEED_NAME，空帮助抛 NEED_HELP
    static void check(Argument::Name_T const &_Name, Argument::Desc_T const &_Help)
    {
        if (_Name.empty()) throw ErrorArgument(BuildError_E::NEED_NAME);
        if (_Help.empty()) throw ErrorArgument(BuildError_E::NEED_HELP);
    }

    /// @brief 校验参数对象（代理到名字/帮助非空校验）
    /// @param _Arg 命名参数对象
    static void check(ArgumentNamed const &_Arg)
    {
        check(_Arg.getName(), _Arg.getHelp());
    }

private:
    /// @brief 校验参数名尚未被占用
    /// @param _Name 待校验的参数名
    /// @note 已存在则抛 DUPLICATE_NAME
    void __checkDuplicate(Argument::Name_T const &_Name) const
    {
        if (pData__->mgrName.doFind(_Name)) {
            throw ErrorArgument(BuildError_E::DUPLICATE_NAME);
        }
    }

private:
    ArgumentData *pData__ = nullptr;  ///< 底层参数数据仓库指针
};

CMD_ARGS_NAMESPACE_END