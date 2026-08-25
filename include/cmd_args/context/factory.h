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

#include <memory>
#include <stdexcept>
#include <string>

#include "cmd_args/base/def.h"
#include "cmd_args/context/parse_fwd.h"

#include "context_base.h"
#include "context_runtime.h"
#include "context_step.h"

CMD_ARGS_NAMESPACE_BEGIN

/**
 * @brief 上下文工厂：按引擎种类注入对应派生上下文
 * @param _Kind 引擎种类（E_STEP=批量步进 / E_RUN_TIME=流式运行时）
 * @param _Name 工程名
 * @return std::unique_ptr<ContextBase> 多态上下文（调用方持有所有权）
 * @throw std::invalid_argument 未知引擎种类
 */
inline auto make_context(EngineKind_E _Kind, Name_T _Name) -> std::unique_ptr<ContextBase>
{
    switch (_Kind) {
    case EngineKind_E::E_STEP:
        return std::make_unique<ContextStep>(std::move(_Name));
    case EngineKind_E::E_RUN_TIME:
        return std::make_unique<ContextRuntime>(std::move(_Name));
    default: break;
    }
    throw std::invalid_argument("unknown engine kind");
}

CMD_ARGS_NAMESPACE_END
