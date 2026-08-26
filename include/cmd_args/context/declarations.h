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

#include "cmd_args/base/def.h"
#include "cmd_args/context/context_fwd.h"

CMD_ARGS_NAMESPACE_BEGIN

/**
 * @struct OptionDef
 * @brief 值选项声明结构体（仅用于 declare() / group() 批量注册）
 *
 * 轻量 POD，不含运行时逻辑。类型 T 从 defaultValue 推导，
 * 由 declare() 内部转发给 OptionBuilder::addOption<T>。
 *
 * 用法示例：
 * @code
 *   OptionDef<int>{'t', "timeout", "超时(秒)", 30}
 *   OptionDef<std::string>{'o', "output", "输出文件", "out.txt"}
 * @endcode
 */
template <typename T>
struct OptionDef
{
    char        shortName = '\0';
    NameLong_T  longName;
    Desc_T      help;
    T           defaultValue{};
};

/**
 * @struct FlagDef
 * @brief 布尔标志声明结构体（仅用于 declare() / group() 批量注册）
 *
 * 出现即 true，不需要默认值。
 */
struct FlagDef
{
    char        shortName = '\0';
    NameLong_T  longName;
    Desc_T      help;
};

CMD_ARGS_NAMESPACE_END
