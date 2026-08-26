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
#include "cmd_args/expression/expression.h"
#include "cmd_args/env/env_data.h"
#include "cmd_args/option/option_data.h"
#include "cmd_args/option/option_graph.h"

CMD_ARGS_NAMESPACE_BEGIN

/**
 * @struct OptionGroup
 * @brief 选项分组元数据（用于帮助文档分组展示）
 *
 * 记录一组选项的分组名称和所属选项长名列表。
 * help 输出时按分组渲染，未分组选项单独展示。
 */
struct OptionGroup
{
    Name_T                  name;      ///< 分组名称
    std::vector<NameLong_T> members;   ///< 属于该分组的选项长名
};

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
    bool hasAction() const noexcept { return !opts.actionOrder().empty(); }

    /// @brief 统一 Expression 视图：遍历三类 Data，返回全部 Expression 派生对象指针
    auto expressions() const -> std::vector<Expression const *>
    {
        std::vector<Expression const *> vec;
        vec.reserve(16);
        args.mgrName.doConVisit([&](Argument const &_A) { vec.push_back(&_A); });
        args.mgrPos.doConVisit([&](Argument const &_A) { vec.push_back(&_A); });
        opts.mgrValues.forEachValue([&](OptionValue const &_O) { vec.push_back(&_O); });
        opts.mgrActions.forEachAction([&](OptionAction const &_A) { vec.push_back(&_A); });
        envs.doVisit([&](environment const &_E) { vec.push_back(&_E); });
        return vec;
    }

    ArgumentData args;
    OptionData   opts;
    EnvData      envs;
    OptionGraph  optionGraph;  ///< Option 关系有向图（条件/依赖边）

    /// @brief 选项分组元数据（用于帮助文档分组展示）
    std::vector<OptionGroup> groups_;

    /* argv[0] 探测出的程序名，供内置帮助/环境展示使用 */
    std::string strProgramName;

    /* 是否注册内置的 -?/--help、-!/--env 等选项 */
    bool bBuiltinOptions = true;

    /* 内置选项是否已注册（懒注册，仅一次） */
    bool bBuiltinsRegistered = false;

    /* --save-config PATH：解析完成后把参数+环境状态写出为 YAML 配置 */
    std::string strSaveConfigPath;
};

inline Model::Model(/* args */) {}

inline Model::~Model() {}

CMD_ARGS_NAMESPACE_END