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
#include <iostream>
#include <string>
#include <system_error>

#include "cmd_args/base/def.h"
#include "cmd_args/base/util.h"
#include "cmd_args/error/interpreter_error.h"

CMD_ARGS_NAMESPACE_BEGIN

/*
    Interpreter（解释器）：解释执行 Context 中的绑定动作。

    解释执行框架由本基类实现，子类（Context）只提供四个钩子：

    - once() : 单次输入即退。若尚未执行过任何一次解释（解析），
               抛 ErrorInterp(STOPPED)；否则以设定退出码退出进程
    - loop() : 常驻后台，从标准输入逐行解释执行，直到 exit/quit/EOF 退出；
               每行等价一次 once 的输入（子类解释钩子负责 reset 与行间隔离），
               行错误经 __describeError 展示后继续下一行

    钩子约定：
    - __interpret(line)     解释执行一行：reset + 分词 + 解析 + 动作执行
                            （option action 在解析期按 token 顺序 system_call）
    - __describeError(ec)   行错误的展示文本（错误消息 + 错误堆栈 dump）
    - __prepared()          是否已成功解释过（once 的前置条件）
    - __exitCode()          进程退出码（once 使用）
*/
class Interpreter
{
public:
    virtual ~Interpreter() = default;

    /*
        单次输入即退：解析完毕（已解释）后以设定退出码退出进程。
        未解析直接调用则抛 ErrorInterp(STOPPED)（内置 -?/--help、-!/--env
        已在解析时打印输出）。
    */
    [[noreturn]] void once()
    {
        if (!__prepared()) { throw ErrorInterp(InterpError_E::STOPPED); }
        std::exit(__exitCode());
    }

    /*
        REPL 常驻：标准输入逐行解释执行。
        exit / quit / exit() / EOF 退出；每行独立解释（互不合并），
        行错误只展示不中断循环；环境变量会话态由子类解释钩子跨行保留。
    */
    void loop()
    {
        std::cout << "cmd_args> " << std::flush;
        std::string line;
        while (std::getline(std::cin, line)) {
            auto cmd = util::trim(line);
            if (cmd.empty()) {
                std::cout << "cmd_args> " << std::flush;
                continue;
            }
            if (cmd == "exit" || cmd == "quit" || cmd == "exit()") break;

            auto ec = __interpret(cmd);
            if (ec) { std::cout << __describeError(ec); }

            std::cout << "cmd_args> " << std::flush;
        }
        std::cout << "\n";
    }

protected:
    /* 解释执行一行输入（子类实现：reset + 分词 + 解析 + 动作执行），返回错误码 */
    virtual std::error_code __interpret(std::string const &_Line) = 0;

    /* 行错误的展示文本（子类实现：错误消息 + 错误栈 dump） */
    virtual std::string __describeError(std::error_code const &_Ec) const = 0;

    /* 是否已成功解释过（once 前置条件） */
    virtual bool __prepared() const noexcept = 0;

    /* 进程退出码（once 使用） */
    virtual int __exitCode() const noexcept = 0;
};

CMD_ARGS_NAMESPACE_END
