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

#include <list>
#include <string>
#include <vector>

#include "cmd_args/base/def.h"

CMD_ARGS_NAMESPACE_BEGIN

/* 全局类型前置声明（各子系统共用的轻量别名，避免互相依赖） */
class Errors;

using Name_T      = std::string;   // 通用名字（参数名 / 环境变量键等）
using NameShort_T = char;          // 短名（短选项 -x 的 x；'\0' 表示无短名）
using NameLong_T  = std::string;   // 长名（--long 的 long，不带前缀）
using Desc_T      = std::string;   // 描述/帮助文本
using Tokens_T    = std::list<std::string>;  // token 流（Lexer 输出 / Compiler 消费）
using Envs_T      = std::vector<std::string>;  // "KEY=VALUE" 环境串列表

CMD_ARGS_NAMESPACE_END