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

#include <iostream>
#include <string>

#include "cmd_args/base/def.h"
#include "cmd_args/base/util.h"
#include "cmd_args/error/errors.h"
#include "cmd_args/error/interpreter_error.h"
#include "cmd_args/error/lexer_error.h"
#include "cmd_args/error/parser_error.h"

#include "builtin_options.h"
#include "context_base.h"

#include "cmd_args/expression/compiler.h"
#include "cmd_args/expression/executor.h"

CMD_ARGS_NAMESPACE_BEGIN

/*
    ContextStep（批量步进上下文）：
    - 引擎注入：Compiler（纯解析入库）+ Executor（hasAction 门控后按 actionOrder 统一触发）
      【不变量：本类不含任何 Interpreter 引用】
    - parse：与兼容版 Context 相同的批量语义（短回路命中即中止本次解析）
    - once()：一次性执行完命令行参数后以退出码退出进程（未解析先调抛 STOPPED）
    - loop()：once 的循环化——每轮独立 reset → Lexer 分词 → 批量管线，识别 exit/quit/exit()
      后正常返回，否则自动等待下一段输入
*/
class ContextStep : public ContextBase
{
public:
    explicit ContextStep(std::string _Name)
        : ContextBase(std::move(_Name))
        , compiler__(&instData_)
        , executor__(&instData_)
    {}

    /// @brief 解析完成后以退出码退出进程
    /// @throw ErrorInterp(STOPPED) 未解析先调用
    void once() override
    {
        if (!bParsed_) { throw ErrorInterp(InterpError_E::STOPPED); }
        std::exit(nExitCode_);
    }

    /// @brief 批量 REPL：prompt → getline → 行间 reset → 批量管线 → 错误展示，exit/quit 退出
    void loop() override
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

            _errors().clearLast();
            builtin_detail::resetModelData(&instData_);

            Tokens_T toks = Lexer::tokensFromLine(cmd);
            Envs_T   envs;
            try {
                _doRunPipeline(toks, envs);
            }
            catch (ErrorParse const &e) {
                _errors().setLast(e.code());
                std::cout << "(parse error) " << e.code().message() << "\n" << _errors().dump();
            }
            catch (ErrorLexer const &e) {
                _errors().setLast(e.code());
                std::cout << "(parse error) " << e.code().message() << "\n" << _errors().dump();
            }

            std::cout << "cmd_args> " << std::flush;
        }
        std::cout << "\n";
    }

protected:
    /// @brief 批量管线：Compiler 纯解析入库 → hasAction 门控 Executor → finalize
    void _doRunPipeline(Tokens_T &_Toks, Envs_T const &_Envs) override
    {
        compiler__.set_program_name(instData_.strProgramName);
        compiler__.set_description(strPrjName_);

        compiler__.ensureBuiltins();
        compiler__.parse(_Toks, _Envs);

        if (instData_.hasAction()) {
            executor__.execute();
        }

        compiler__.finalize();

        _markParsed();
    }

private:
    Compiler compiler__;   ///< 批量编译引擎
    Executor executor__;   ///< 批量动作执行器
};

CMD_ARGS_NAMESPACE_END
