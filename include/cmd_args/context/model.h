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

#include "cmd_args/base/def.h"

#include "cmd_args/argument/argument_data.h"
#include "cmd_args/env/environment_parser.h"
#include "cmd_args/option/option_data.h"

CMD_ARGS_NAMESPACE_BEGIN

/*
    Context 的核心数据仓库：参数、选项、环境变量统一存放。
    解析产生的值在这里跨多次 parse 持久保留。
*/
class Model
{
public:
    Model(/* args */);
    ~Model();

    ArgumentData m_args__;
    OptionData   m_opts__;
    environment_store m_envs__;

    /* argv[0] 探测出的程序名，供内置帮助/环境展示使用 */
    std::string m_strProgramName__;

    /* 是否注册内置的 -?/--help、-!/--env 等选项 */
    bool m_bBuiltinOptions__ = true;

    /* 内置选项是否已注册（懒注册，仅一次） */
    bool m_bBuiltinsRegistered__ = false;
};

Model::Model(/* args */) {}

Model::~Model() {}

CMD_ARGS_NAMESPACE_END