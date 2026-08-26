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

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "cmd_args/base/def.h"

CMD_ARGS_NAMESPACE_BEGIN

// 前向声明
class ContextBase;

/// 子命令工厂函数类型：创建并返回子 Context
using SubcommandFactory = std::function<std::unique_ptr<ContextBase>()>;

/*
    子命令注册表：管理子命令名 → 工厂函数的映射。
    ContextBase 持有此对象以支持分层子命令注册与查找。
*/
class SubcommandRegistry
{
public:
    /// @brief 注册子命令
    /// @param _Name 子命令名称（如 "commit", "branch"）
    /// @param _Factory 创建子 Context 的工厂函数
    void registerSubcommand(std::string _Name, SubcommandFactory _Factory)
    {
        mapRegistry__[std::move(_Name)] = std::move(_Factory);
    }

    /// @brief 查找子命令
    /// @param _Name 子命令名称
    /// @return 工厂函数指针，未找到返回 nullptr
    auto find(std::string const &_Name) const -> SubcommandFactory const *
    {
        auto it = mapRegistry__.find(_Name);
        return (it != mapRegistry__.end()) ? &it->second : nullptr;
    }

    /// @brief 是否包含指定子命令
    bool contains(std::string const &_Name) const
    {
        return mapRegistry__.count(_Name) > 0;
    }

    /// @brief 获取所有注册的子命令名称
    auto names() const -> std::vector<std::string>
    {
        std::vector<std::string> result;
        result.reserve(mapRegistry__.size());
        for (auto const &[name, _] : mapRegistry__) {
            result.push_back(name);
        }
        return result;
    }

    /// @brief 是否为空
    bool empty() const noexcept { return mapRegistry__.empty(); }

    /// @brief 清空注册表
    void clear() { mapRegistry__.clear(); }

private:
    std::unordered_map<std::string, SubcommandFactory> mapRegistry__; ///< 子命令名 → 工厂函数映射
};

CMD_ARGS_NAMESPACE_END
