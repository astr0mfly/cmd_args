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

#include <cstdlib>
#include <iostream>
#include <string>

#include "cmd_args/base/def.h"
#include "cmd_args/base/util.h"
#include "cmd_args/option/option.h"
#include "cmd_args/option/option_data.h"

#include "model.h"

CMD_ARGS_NAMESPACE_BEGIN

/*
    Executor（执行器）：解析完成后，按解析顺序执行 Model 中 OptionData 的
    OptionAction（fnOnSet__ 回调 + action 模板 system_call）。

    使用场景：批量解析模式（Context::parse）下，Compiler 纯解析结束后，
    Executor 按 recordParseOrder 记录的顺序统一触发 action。
    （REPL/Interpreter 模式下 action 由 Interpreter 的 virtual hook 内联触发，
     不使用 Executor。）
*/
class Executor
{
public:
    /// @brief 构造执行器
    /// @param _Model 底层模型（含 OptionData）
    explicit Executor(Model *_Model) noexcept
        : pModel__(_Model)
    {}

    /// @brief 按解析顺序执行含 action 的选项：fnOnSet__ 回调 + action 模板 system_call
    void execute()
    {
        for (auto const &key : pModel__->opts.actionOrder()) {
            auto *pOpt = pModel__->opts.mgrValues.mutFind(key);
            if (!pOpt) continue;

            // 自定义赋值回调（--env-set 等内置交互选项）
            if (pOpt->fnOnSet__) { pOpt->fnOnSet__(pOpt->strValue); }

            // action 模板 → std::system 子进程命令
            if (!pOpt->strActionTemplate__.empty()) {
                auto cmd = util::replace(pOpt->strActionTemplate__, "{value}", pOpt->strValue);
                if (!cmd.empty()) {
                    int ret = std::system(cmd.c_str());
                    if (ret != 0) {
                        std::cerr << "(action) command exit " << ret << ": " << cmd << "\n";
                    }
                }
            }
        }
        pModel__->opts.clearParseOrder();
    }

private:
    Model *pModel__ = nullptr;  ///< 底层模型指针（含 OptionData）
};

CMD_ARGS_NAMESPACE_END
