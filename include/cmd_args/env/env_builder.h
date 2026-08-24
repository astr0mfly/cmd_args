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

#include <string>

#include "cmd_args/base/def.h"

#include "cmd_args/env/env.h"
#include "cmd_args/env/env_data.h"

CMD_ARGS_NAMESPACE_BEGIN

/// @brief 环境变量构筑（写操作）：把 env 条目写入 EnvData（设置 / 删除）
class EnvBuilder
{
public:
    /// @brief 构造环境变量构筑器
    /// @param _Data 底层环境数据仓库（非空）
    EnvBuilder(EnvData *_Data) noexcept
        : pData__(_Data)
    {}

    /// @brief 设置环境变量：键已存在则改值并标记 modified，否则新建并标记 new
    /// @param _Key 键
    /// @param _Value 值
    void setEnv(EnvData::Key_T _Key, EnvData::Value_T _Value)
    {
        environment *p = pData__->doMutGet(_Key);
        if (!p) {
            pData__->doPush(_Key, std::move(_Value), true);
            return;
        }
        p->strValue    = std::move(_Value);
        p->bModified__ = true;
    }

    /// @brief 删除环境变量，返回原先是否存在
    /// @param _Key 键
    bool unsetEnv(EnvData::Key_T const &_Key) { return pData__->doErase(_Key); }

private:
    EnvData *pData__ = nullptr;  ///< 底层环境数据仓库指针
};

CMD_ARGS_NAMESPACE_END
