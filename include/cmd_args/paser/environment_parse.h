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

#include "cmd_args/base/errors.h"

#include "cmd_args/env/environment.h"

#include "parse_fwd.h"

CMD_ARGS_NAMESPACE_BEGIN

class envirionment_parser
{
public:
    envirionment_parser(Errors *_Obj)
        : m_pErrs__(_Obj)
    {}

    void parse(Envs_T const &_Envp)
    {
        for (auto &e : _Envp) m_setEnvs__.emplace(e);
    }

    std::string env_string() const
    {
        std::ostringstream oss;
        size_t             i = 0;
        for (auto &e : m_setEnvs__) {
            oss << i << " : " << e.strKey << " = " << e.strValue << std::endl;
            ++i;
        }

        return oss.str();
    }

    void set(std::string _Key, std::string _Value)
    {
        auto it = m_setEnvs__.find(_Key);
        if (it == m_setEnvs__.end()) return;
        it->strValue = _Value;
    }

private:
    envs_t  m_setEnvs__;
    Errors *m_pErrs__ = nullptr;
};

CMD_ARGS_NAMESPACE_END