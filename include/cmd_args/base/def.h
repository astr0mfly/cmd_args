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

/*
    基础设施宏：命名空间开关 + 计数/名字工具。
    - CMD_ARGS_NAMESPACE_BEGIN/END：整个库统一经该宏展开命名空间
      （默认 cmd_args；改名/加前缀只需在此调整）
    - ARRAY_SIZE(_A)   : 数组元素个数（编译期，经 detail::fakeArraySize 推导）
    - ENUM_NAME(_E)    : 枚举名转字符串（如 ENUM_NAME(BuildError_E) -> "BuildError_E"，
                          供 error_category::name() 返回）
*/
#define CMD_ARGS_NAMESPACE_BEGIN namespace cmd_args {
#define CMD_ARGS_NAMESPACE_END   }  // end of namespace cmd_args
// #define CMD_ARGS_NAMESPACE_PREFIX cmd_args::
// #define CMD_ARGS_CONCAT(_A, _B) _A##_B
// #define CMD_ARGS_NAMESPACE(_X)   CMD_ARGS_CONCAT(cmd_args::, _X)

namespace detail {
/* 仅供 ARRAY_SIZE 推导数组长度，不生成实体 */
template <typename T, unsigned int N>
char (&fakeArraySize(T (&)[N]))[N];
}

#define ARRAY_SIZE(_A) sizeof(::detail::fakeArraySize(_A))
#define ENUM_NAME(_E)  #_E