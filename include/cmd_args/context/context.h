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

#include "cmd_args/expression/compiler.h"
#include "cmd_args/expression/executor.h"
#include "cmd_args/expression/interpreter.h"

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
    /// @brief 批量管线：扫描子命令路径 → Compiler 纯解析 → hasAction 门控 Executor → finalize
    void _doRunPipeline(Tokens_T &_Toks, Envs_T const &_Envs) override
    {
        // 如果没有子命令注册，直接走原有逻辑
        if (!hasSubcommands()) {
            _doRunPipelineWithoutSubcommands(_Toks, _Envs);
            return;
        }

        // 扫描子命令路径
        size_t consumed = _scanSubcommandPath(_Toks);
        
        // 如果匹配到子命令，路由到子 Context
        if (activeSubContext_) {
            // 剩余 token 传给子 Context
            Tokens_T remainingToks(_Toks.begin() + consumed, _Toks.end());
            activeSubContext_->parse(remainingToks, _Envs);
            _markParsed();
            return;
        }

        // 未匹配到子命令，走原有逻辑
        _doRunPipelineWithoutSubcommands(_Toks, _Envs);
    }

private:
    /// @brief 原有的管线逻辑（无子命令版本）
    void _doRunPipelineWithoutSubcommands(Tokens_T &_Toks, Envs_T const &_Envs)
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

    /// @brief 扫描子命令路径，返回消耗的 token 数
    /// @param _Toks 完整 token 流
    /// @return 消耗的 token 数（子命令名称的数量）
    size_t _scanSubcommandPath(Tokens_T const &_Toks)
    {
        activeSubcommandPath_.clear();
        activeSubContext_.reset();

        ContextBase *current = this;
        size_t consumed = 0;

        // 从 token 流开头扫描连续的子命令名称
        for (size_t i = 0; i < _Toks.size(); ++i) {
            auto const &tok = _Toks[i];

            // 跳过以 '-' 开头的 token（选项）
            if (tok.empty() || tok[0] == '-') {
                break;
            }

            // 检查当前 Context 是否有这个子命令
            auto const *factory = current->subcommandRegistry_.find(tok);
            if (!factory) {
                break;
            }

            // 创建子 Context
            auto child = (*factory)();
            
            // 记录路径
            activeSubcommandPath_.push_back(tok);
            
            // 设置子 Context 的程序名为父程序名 + 子命令路径
            std::string childProgramName = current->instData_.strProgramName;
            for (auto const &seg : activeSubcommandPath_) {
                childProgramName += " " + seg;
            }
            child->instData_.strProgramName = std::move(childProgramName);

            // 传递环境变量
            child->instData_.envs = current->instData_.envs;

            // 传递 builtin 选项设置
            child->instData_.bBuiltinOptions = current->instData_.bBuiltinOptions;

            // 移动到子 Context
            activeSubContext_ = std::move(child);
            current = activeSubContext_.get();
            consumed = i + 1;
        }

        return consumed;
    }

private:
    Compiler    compiler__;
    Executor    executor__;
    Interpreter interp__;
};

CMD_ARGS_NAMESPACE_END
