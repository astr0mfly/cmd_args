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
#include "cmd_args/env/env_parser.h"
#include "cmd_args/error/errors.h"
#include "cmd_args/error/interpreter_error.h"
#include "cmd_args/error/lexer_error.h"
#include "cmd_args/error/parser_error.h"

#include "builtin_options.h"
#include "context_base.h"

#include "cmd_args/expression/interpreter.h"

CMD_ARGS_NAMESPACE_BEGIN

/*
    ContextRuntime（流式运行时上下文）：
    - 引擎注入：Interpreter（逐字符分词 → Expression 分类 → 即时执行，无需 Executor/actionOrder）
      【不变量：本类不含任何 Compiler / Executor 引用】
    - parse：同样走流式——环境串先行入库，token 流以空格拼回一行交 interpret 即时解释
    - once()：执行一行输入即结束（以退出码退出进程；未解析先调抛 STOPPED）
    - loop()：once 的循环化——每轮 interpret 一行，比 once 多识别 exit/quit/exit() 退出信号
      （命中即正常返回而非退进程），否则自动等待下一段输入
*/
class ContextRuntime : public ContextBase
{
public:
    explicit ContextRuntime(std::string _Name)
        : ContextBase(std::move(_Name))
        , interp__(&instData_)
    {}

    /// @brief 解析完成后以退出码退出进程
    /// @throw ErrorInterp(STOPPED) 未解析先调用
    void once() override
    {
        if (!bParsed_) { throw ErrorInterp(InterpError_E::STOPPED); }
        std::exit(nExitCode_);
    }

    /// @brief 流式 REPL：委托 Interpreter（prompt / exit / 错误展示均由其内聚处理）
    void loop() override
    {
        builtin_detail::registerBuiltinOptions(&instData_);
        interp__.loop(_errors());
    }

protected:
    /// @brief 流式管线：envs 先行入库 → token 流拼行 → interpret 即时执行
    /// @note interpret 返回的错误码经 ErrorParse(ec) 原样透传（保留原始错误域与码值）
    void _doRunPipeline(Tokens_T &_Toks, Envs_T const &_Envs) override
    {
        builtin_detail::registerBuiltinOptions(&instData_);
        EnvParser(&instData_.envs).parse(_Envs);

        std::string line;
        for (auto const &t : _Toks) {
            line += t;
            line += ' ';
        }

        auto ec = interp__.interpret(line, _errors());
        if (ec) { throw ErrorParse(ec); }
        _markParsed();
    }

private:
    Interpreter interp__;   ///< 流式解释引擎
};

CMD_ARGS_NAMESPACE_END
