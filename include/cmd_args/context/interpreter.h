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

#include "cmd_args/base/def.h"

CMD_ARGS_NAMESPACE_BEGIN
/*
    解释器，解释执行context中的绑定动作
*/
class Interpreter
{

public:
    Interpreter(/* args */);
    ~Interpreter();

    // be used in once mode and return last_val
    [[noreturn]] void once() const noexcept { exit(0); }

private:
    /* data */
};

Interpreter::Interpreter(/* args */) {}

Interpreter::~Interpreter() {}

CMD_ARGS_NAMESPACE_END