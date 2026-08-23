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

#include <cstdlib>

#include "cmd_args/base/def.h"

CMD_ARGS_NAMESPACE_BEGIN
/*
    解释器基类：解释执行 Context 中的绑定动作。

    - once() : 常驻一次，命令解析完毕之后以指定退出码退出进程
    - loop() : 常驻后台，从标准输入逐行读取命令并执行，直到 exit/quit/EOF

    注：实际的 once()/loop() 语义由 Context 实现（见 context.h）；
    基类仅保留接口形态。
*/
class Interpreter
{
public:
    Interpreter(/* args */);
    ~Interpreter();

    // be used in once mode and return last_val
    [[noreturn]] void once() const noexcept { std::exit(0); }

private:
    /* data */
};

Interpreter::Interpreter(/* args */) {}

Interpreter::~Interpreter() {}

CMD_ARGS_NAMESPACE_END