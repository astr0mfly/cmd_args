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

#define CMD_ARGS_NAMESPACE_BEGIN namespace cmd_args {
#define CMD_ARGS_NAMESPACE_END   }  // end of namespace cmd_args
#define CMD_ARGS_NAMESPACE(_X)   cmd_args::##_X

namespace detail {
template <typename T, unsigned int N>
char (&fakeArraySize(T (&)[N]))[N];
}

#define ARRAY_SIZE(_A) sizeof(::detail::fakeArraySize(_A))
#define ENUM_NAME(_E)  #_E
