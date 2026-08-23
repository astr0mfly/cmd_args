/*
 * Copyright [2023] [ValenciaFly] <pengwanring@live.com>
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

#include <string>
#include <utility>
#include <vector>

#include "cmd_args/base/def.h"
#include "cmd_args/context/parse_fwd.h"

CMD_ARGS_NAMESPACE_BEGIN

/*
    参数格式（parameter format）：build 完成后的参数描述，与调用无关。
    三类外界环境（环境变量 / 实时命令行 / 脚本配置）围绕它进行解析与存储。

    kind 取值：
    - "named"         命名参数（--name=value）
    - "positional"    方位参数（剩余 token 按序）
    - "option"        值选项（-x / --long，带默认值）
    - "flag"          布尔标志（出现即 true）
    - "short_circuit" 短回路选项（-?/--help 一类，命中即停）
*/
struct parameter_entry
{
    std::string name;             // 名字（参数名 / 选项长名 / 方位参数名）
    std::string kind;             // 见上
    std::string type;             // "bool"|"int"|"double"|"string"|""（未定型）
    std::string value;            // 当前值（解析后值；pre-parse 时为内置默认）
    std::string env;              // 绑定的环境变量名（未绑定为空）
    bool        explicit_set{false};  // 本会话被 CLI 显式赋值
    bool        external_set{false};  // 本会话被配置(YAML) 外部赋值
};

struct parameter_schema
{
    std::string program;                       // 程序名（argv[0] 或 Context 名）
    std::vector<parameter_entry> entries;      // 顺序：named → positional → option → flag → short_circuit

    parameter_entry const *find(std::string const &_Name) const noexcept
    {
        for (auto const &e : entries) {
            if (e.name == _Name) return std::addressof(e);
        }
        return nullptr;
    }
};

/*
    参数快照（parameter snapshot）：某一次调用（once 输入或 loop 中一次调用）
    的取值结果，是三种外界环境之间"自然转换"的中枢：
    快照 → YAML（save_config）；YAML → 快照（from_yaml）；快照 → 环境变量。
*/
struct parameter_snapshot
{
    std::vector<parameter_entry>   values;     // 与 schema 对齐的当前取值
    std::vector<std::pair<std::string, std::string>> envs;  // 本程序相关的环境键值
};

CMD_ARGS_NAMESPACE_END