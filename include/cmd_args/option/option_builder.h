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

#include <functional>
#include <string>

#include "cmd_args/base/def.h"
#include "cmd_args/base/util.h"
#include "cmd_args/context/context_fwd.h"

#include "cmd_args/error/builder_error.h"

#include "option_data.h"

CMD_ARGS_NAMESPACE_BEGIN

/**
 * 选项构筑：接受各种类型的选项定义，并做重名/前缀/类型检查。
 * - addOption<T>   值选项（带默认值，命令行 > 环境变量 > 默认值）
 * - addShortCircuit 短回路选项（命中立即执行回调，如 -?/--help）
 */
class OptionBuilder
{
public:
    /// @brief 构造选项构筑器
    /// @param _Data 底层选项数据仓库（非空）
    OptionBuilder(OptionData *_Data) noexcept
        : pData__(_Data)
    {}

    /// @brief 构筑值选项（含短名）
    /// @tparam T 值类型
    /// @param _Short 短名（'\0' 表示无短名）
    /// @param _Long 长名（不含 -- 前缀）
    /// @param _Help 帮助文本
    /// @param _Default 默认值
    /// @return 命中句柄（可再链式定制 env / action / onSet）
    template <typename T>
    auto addOption(char _Short, NameLong_T _Long, Desc_T _Help, T const &_Default) -> OptionValue *
    {
        if (util::type_string<T>() == "null") {
            throw ErrorArgument(BuildError_E::UNSUPPORT_TYPE);
        }

        checkAddOptionSname(_Short);
        checkAddOptionLname(_Long);

        pData__->mgrValues.push(
            _Short,
            _Long,
            _Help,
            util::type_string<T>(),
            util::to_string(_Default));

        return pData__->mgrValues.mutFind(_Long);
    }

    /// @brief 构筑值选项（仅长名，模板版本）
    /// @tparam T 值类型
    /// @param _Long 长名（不含 -- 前缀）
    /// @param _Help 帮助文本
    /// @param _Default 默认值
    /// @return 命中句柄
    template <typename T>
    auto addOption(NameLong_T _Long, Desc_T _Help, T const &_Default) -> OptionValue *
    {
        return addOption<T>('\0', std::move(_Long), std::move(_Help), _Default);
    }

    /// @brief 构筑值选项（含短名，int 类型推导版本）
    auto addOption(char _Short, NameLong_T _Long, Desc_T _Help, int _Default) -> OptionValue *
    { return addOption<int>(_Short, std::move(_Long), std::move(_Help), _Default); }

    /// @brief 构筑值选项（含短名，double 类型推导版本）
    auto addOption(char _Short, NameLong_T _Long, Desc_T _Help, double _Default) -> OptionValue *
    { return addOption<double>(_Short, std::move(_Long), std::move(_Help), _Default); }

    /// @brief 构筑值选项（含短名，std::string 类型推导版本）
    auto addOption(char _Short, NameLong_T _Long, Desc_T _Help, std::string const &_Default) -> OptionValue *
    { return addOption<std::string>(_Short, std::move(_Long), std::move(_Help), _Default); }

    /// @brief 构筑值选项（仅长名，int 类型推导版本）
    auto addOption(NameLong_T _Long, Desc_T _Help, int _Default) -> OptionValue *
    { return addOption<int>(std::move(_Long), std::move(_Help), _Default); }

    /// @brief 构筑值选项（仅长名，double 类型推导版本）
    auto addOption(NameLong_T _Long, Desc_T _Help, double _Default) -> OptionValue *
    { return addOption<double>(std::move(_Long), std::move(_Help), _Default); }

    /// @brief 构筑值选项（仅长名，std::string 类型推导版本）
    auto addOption(NameLong_T _Long, Desc_T _Help, std::string const &_Default) -> OptionValue *
    { return addOption<std::string>(std::move(_Long), std::move(_Help), _Default); }

    /// @brief 构筑 bool 标志选项：出现即 true，不需要默认值
    /// @param _Short 短名（'\0' 表示无短名）
    /// @param _Long 长名（不含 -- 前缀）
    /// @param _Help 帮助文本
    /// @return 命中句柄
    auto addFlag(char _Short, NameLong_T _Long, Desc_T _Help) -> OptionValue *
    {
        checkAddOptionSname(_Short);
        checkAddOptionLname(_Long);

        pData__->mgrValues.push(_Short, _Long, _Help, "bool", "0");
        return pData__->mgrValues.mutFind(_Long);
    }

    /// @brief 构筑 bool 标志选项（仅长名）
    /// @param _Long 长名（不含 -- 前缀）
    /// @param _Help 帮助文本
    /// @return 命中句柄
    auto addFlag(NameLong_T _Long, Desc_T _Help) -> OptionValue *
    {
        return addFlag('\0', std::move(_Long), std::move(_Help));
    }

    /// @brief 构筑短回路选项：短名/长名命中其一即回调并停止解析
    /// @param _Short 短名（'\0' 表示无短名）
    /// @param _Long 长名（不含 -- 前缀）
    /// @param _Help 帮助文本
    /// @param _Cb 命中回调
    void addShortCircuit(char _Short, NameLong_T _Long, Desc_T _Help, OptionAction::FnCallback_T _Cb)
    {
        checkAddOptionSname(_Short);
        checkAddOptionLname(_Long);

        pData__->mgrActions.push(_Short, _Long, _Help, std::move(_Cb));
    }

private:
    /// @brief 校验短名未被占用（'\0' 跳过）
    /// @param _Key 待校验短名
    /// @note 已存在则抛 DUPLICATE_NAME
    void checkAddOptionSname(char _Key) const
    {
        if (_Key == '\0') return;
        if (pData__->mgrActions.doGet(_Key) || pData__->mgrValues.doGet(_Key)) {
            throw ErrorArgument(BuildError_E::DUPLICATE_NAME);
        }
    }

    /// @brief 校验长名非空、无非法前缀且未被占用
    /// @param _Key 待校验长名
    /// @note 空名抛 NEED_NAME，带前缀抛 BAD_PREFIX，已存在抛 DUPLICATE_NAME
    void checkAddOptionLname(Name_T const &_Key) const
    {
        if (_Key.empty()) { throw ErrorArgument(BuildError_E::NEED_NAME); }
        if (util::starts_with(_Key, "--") || _Key.front() == '-') {
            throw ErrorArgument(BuildError_E::BAD_PREFIX);
        }
        if (pData__->mgrActions.doGet(_Key) || pData__->mgrValues.doGet(_Key)) {
            throw ErrorArgument(BuildError_E::DUPLICATE_NAME);
        }
    }

private:
    OptionData *pData__ = nullptr;  ///< 底层选项数据仓库指针
};

CMD_ARGS_NAMESPACE_END
