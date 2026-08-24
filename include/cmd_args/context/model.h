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

#include <vector>

#include "cmd_args/base/def.h"

#include "cmd_args/argument/argument_data.h"
#include "cmd_args/context/expression.h"
#include "cmd_args/env/env_data.h"
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

    // ===== 查询方法 =====

    /// @brief 是否存在待执行的 OptionAction（fnOnSet__ 或 action template）
    /// @note Executor 按 actionOrder 顺序执行；hasAction 为 false 时无需调用 Executor
    bool hasAction() const noexcept { return !m_opts__.actionOrder().empty(); }

    /// @brief 统一 Expression 视图：遍历三类 Data，返回全部 Expression 派生对象指针
    auto expressions() const -> std::vector<Expression const *>
    {
        std::vector<Expression const *> vec;
        vec.reserve(16);
        m_args__.mgrName.doConVisit([&](Argument const &_A) { vec.push_back(&_A); });
        m_args__.mgrPos.doConVisit([&](Argument const &_A) { vec.push_back(&_A); });
        m_opts__.mgrValues.forEachValue([&](OptionValue const &_O) { vec.push_back(&_O); });
        m_opts__.mgrActions.forEachAction([&](OptionAction const &_A) { vec.push_back(&_A); });
        m_envs__.doVisit([&](environment const &_E) { vec.push_back(&_E); });
        return vec;
    }

    ArgumentData m_args__;
    OptionData   m_opts__;
    EnvData      m_envs__;

    /* argv[0] 探测出的程序名，供内置帮助/环境展示使用 */
    std::string m_strProgramName__;

    /* 是否注册内置的 -?/--help、-!/--env 等选项 */
    bool m_bBuiltinOptions__ = true;

    /* 内置选项是否已注册（懒注册，仅一次） */
    bool m_bBuiltinsRegistered__ = false;

    /* --save-config PATH：解析完成后把参数+环境状态写出为 YAML 配置 */
    std::string m_strSaveConfigPath__;
};

Model::Model(/* args */) {}

Model::~Model() {}

CMD_ARGS_NAMESPACE_END