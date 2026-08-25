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
    cmd_args 唯一入口头文件：
    只需包含本文件即可使用全部能力
    （Context / Builder / Lexer / Compiler / 环境变量一等公民 / YAML 配置 / 选项 action）。

    使用示例见 example/once/main.cpp 与 README.md（中英镜像）。
*/
#include "cmd_args/context/context.h"
#include "cmd_args/context/context_step.h"
#include "cmd_args/context/context_runtime.h"
#include "cmd_args/context/factory.h"