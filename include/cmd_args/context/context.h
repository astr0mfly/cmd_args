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

#include "cmd_args/base/def.h"

#include "cmd_args/context/builtin_options.h"
#include "cmd_args/context/context_base.h"

#include "compiler.h"
#include "executor.h"
#include "interpreter.h"

CMD_ARGS_NAMESPACE_BEGIN

/*
    Context（兼容完整实现）：
    - 继承 ContextBase 全部引擎无关能力，自身持有三件套引擎（Compiler + Executor + Interpreter）
    - parse 管线：Compiler 纯解析入库 → hasAction 门控 Executor → finalize（批量语义）
    - once()/loop()：沿用历史行为——once 以退出码退出进程；loop 委托 Interpreter 流式 REPL
    - 兼容期保留：新代码请优先使用 make_context(E_STEP / E_RUN_TIME) 注入对应派生上下文
*/
class Context : public ContextBase
{
public:
    Context(std::string _Name)
        : ContextBase(std::move(_Name))
        , compiler__(&instData_)
        , executor__(&instData_)
        , interp__(&instData_)
    {}

    /* ---------- 执行模式（兼容语义） ---------- */

    void once() override
    {
        if (!bParsed_) { throw ErrorInterp(InterpError_E::STOPPED); }
        std::exit(nExitCode_);
    }

    void loop() override
    {
        compiler__.ensureBuiltins();
        interp__.loop(_errors());
    }

protected:
    /// @brief 批量管线：Compiler 纯解析 → hasAction 门控 Executor → finalize
    void _doRunPipeline(Tokens_T &_Toks, Envs_T const &_Envs) override
    {
        compiler__.set_program_name(instData_.strProgramName);
        compiler__.set_description(strPrjName_);

        compiler__.ensureBuiltins();
        compiler__.parse(_Toks, _Envs);      // 编译：Tokens → Expression 分类入库 Model

        if (instData_.hasAction())           // Model.hasAction() 门控
        {
            executor__.execute();            // 按 actionOrder 顺序执行
        }

        compiler__.finalize();               // seed 环境兜底 + 展开 + save-config

        _markParsed();
    }

private:
    Compiler    compiler__;
    Executor    executor__;
    Interpreter interp__;
};

CMD_ARGS_NAMESPACE_END
