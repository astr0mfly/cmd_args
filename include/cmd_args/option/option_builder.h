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

#include <functional>
#include <string>

#include "cmd_args/base/def.h"
#include "cmd_args/base/util.h"
#include "cmd_args/context/parse_fwd.h"

#include "cmd_args/error/builder_error.h"

#include "option_data.h"

CMD_ARGS_NAMESPACE_BEGIN

/*
    选项构筑：接受各种类型的选项定义，并做重名/前缀/类型检查。
    - add_option<T>   值选项（带默认值，命令行 > 环境变量 > 默认值）
    - add_short_circuit 短回路选项（命中立即执行回调，如 -?/--help）
*/
class OptionBuilder
{
public:
    OptionBuilder(OptionData *_Data) noexcept
        : m_pData__(_Data)
    {}

    template <typename T>
    option_value *add_option(char _Short, NameLong_T _Long, Desc_T _Help, T const &_Default)
    {
        if (util::type_string<T>() == "null") {
            throw ErrorArgument(BuildError_E::UNSUPPORT_TYPE);
        }

        check_add_option_sname(_Short);
        check_add_option_lname(_Long);

        m_pData__->options.push(
            _Short,
            _Long,
            _Help,
            util::type_string<T>(),
            util::to_string(_Default));

        return m_pData__->options.mut_find(_Long);
    }

    template <typename T>
    option_value *add_option(NameLong_T _Long, Desc_T _Help, T const &_Default)
    {
        return add_option<T>('\0', std::move(_Long), std::move(_Help), _Default);
    }

    /* bool 选项：出现即 true，不需要默认值 */
    option_value *add_flag(char _Short, NameLong_T _Long, Desc_T _Help)
    {
        check_add_option_sname(_Short);
        check_add_option_lname(_Long);

        m_pData__->options.push(_Short, _Long, _Help, "bool", "0");
        return m_pData__->options.mut_find(_Long);
    }

    option_value *add_flag(NameLong_T _Long, Desc_T _Help)
    {
        return add_flag('\0', std::move(_Long), std::move(_Help));
    }

    /* 短回路选项：短名/长名命中其一即回调并停止解析 */
    void add_short_circuit(char _Short, NameLong_T _Long, Desc_T _Help, std::function<void(void)> _Cb)
    {
        check_add_option_sname(_Short);
        check_add_option_lname(_Long);

        m_pData__->short_circuit_options.push(_Short, _Long, _Help, std::move(_Cb));
    }

private:
    void check_add_option_sname(char _Key) const
    {
        if (_Key == '\0') return;
        if (m_pData__->short_circuit_options.doGet(_Key) || m_pData__->options.doGet(_Key)) {
            throw ErrorArgument(BuildError_E::DUPLICATE_NAME);
        }
    }

    void check_add_option_lname(std::string const &_Key) const
    {
        if (_Key.empty()) { throw ErrorArgument(BuildError_E::NEED_NAME); }
        if (util::starts_with(_Key, "--") || _Key.front() == '-') {
            throw ErrorArgument(BuildError_E::BAD_PREFIX);
        }
        if (m_pData__->short_circuit_options.doGet(_Key) || m_pData__->options.doGet(_Key)) {
            throw ErrorArgument(BuildError_E::DUPLICATE_NAME);
        }
    }

private:
    OptionData *m_pData__ = nullptr;
};

CMD_ARGS_NAMESPACE_END