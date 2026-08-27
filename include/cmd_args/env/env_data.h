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

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <string>

#include "cmd_args/base/def.h"

#include "cmd_args/env/env.h"

CMD_ARGS_NAMESPACE_BEGIN

namespace env_detail {

/// @brief 环境变量数据仓库：以键（strKey）为序、去重存储全部 environment 条目
class EnvData
{
public:
    using Key_T   = std::string;  ///< 环境变量键类型
    using Value_T = std::string;  ///< 环境变量值类型

    /// @brief 是否为空容器
    bool doEmpty() const noexcept { return mapEnvs__.empty(); }

    /// @brief 环境条目个数
    auto doSize() const noexcept -> std::size_t { return mapEnvs__.size(); }

    /// @brief 清空所有环境条目（保留声明式条目 mapSpecs__）
    void doClear() noexcept { mapEnvs__.clear(); }

    /// @brief 按键查询是否存在
    bool doFind(Key_T const &_Key) const noexcept
    {
        return mapEnvs__.find(_Key) != mapEnvs__.cend();
    }

    /// @brief 按键查询条目（只读，未找到返回 nullptr）
    auto doGet(Key_T const &_Key) const noexcept -> environment const *
    {
        auto it = mapEnvs__.find(_Key);
        if (it == mapEnvs__.cend()) return nullptr;
        return std::addressof(it->second);
    }

    /// @brief 按键查询条目（可变：改值/改标记用，未找到返回 nullptr）
    auto doMutGet(Key_T const &_Key) noexcept -> environment *
    {
        auto it = mapEnvs__.find(_Key);
        if (it == mapEnvs__.end()) return nullptr;
        return std::addressof(it->second);
    }

    /// @brief 插入环境条目（键已存在则保持原条目）
    void doPush(environment const &_Env) { mapEnvs__.emplace(_Env.strKey, _Env); }

    /// @brief 插入环境条目（移动形态）
    void doPush(environment &&_Env) { mapEnvs__.emplace(_Env.strKey, std::move(_Env)); }

    /// @brief 以键/值/新建标记构造并插入（键已存在则改值并标记 modified）
    void doPush(Key_T const &_Key, Value_T _Value, bool _New)
    {
        auto it = mapEnvs__.find(_Key);
        if (it != mapEnvs__.end()) {
            it->second.strValue    = std::move(_Value);
            it->second.bModified__ = true;
            return;
        }
        mapEnvs__.emplace(_Key, environment(_Key, std::move(_Value), _New));
    }

    /// @brief 按键删除，返回原先是否存在
    bool doErase(Key_T const &_Key) { return mapEnvs__.erase(_Key) != 0; }

    /* ---------- 声明式条目（EnvSpec）---------- */

    /// @brief 注册声明式环境条目（键已存在则保持原条目）
    void pushSpec(EnvSpec const &_Spec) { mapSpecs__.emplace(_Spec.strKey, _Spec); }

    /// @brief 注册声明式环境条目（移动形态）
    void pushSpec(EnvSpec &&_Spec) { mapSpecs__.emplace(_Spec.strKey, std::move(_Spec)); }

    /// @brief 查询声明式条目（可变，未找到返回 nullptr；map 节点稳定不失效）
    auto mutSpec(Key_T const &_Key) noexcept -> EnvSpec *
    {
        auto it = mapSpecs__.find(_Key);
        if (it == mapSpecs__.end()) return nullptr;
        return std::addressof(it->second);
    }

    /// @brief 只读遍历声明式条目（校验引擎用）
    void forEachSpec(std::function<void(EnvSpec const &)> const &_Fn) const
    {
        for (auto const &kv : mapSpecs__) { _Fn(kv.second); }
    }

    /// @brief 只读遍历
    void doVisit(std::function<void(environment const &)> const &_Fn) const
    {
        for (auto const &kv : mapEnvs__) { _Fn(kv.second); }
    }

    /// @brief 可变遍历（apply 清标记等）
    void doMutVisit(std::function<void(environment &)> const &_Fn)
    {
        for (auto &kv : mapEnvs__) { _Fn(kv.second); }
    }

private:
    using EnvMap_T = std::map<Key_T, environment>;  ///< 容器类型（按键排序去重）
    EnvMap_T mapEnvs__;                              ///< 环境条目容器

    using SpecsMap_T = std::map<Key_T, EnvSpec>;  ///< 声明式条目容器（按键排序去重）
    SpecsMap_T mapSpecs__;                         ///< 声明式环境条目（EnvSpec）
};

}  // namespace env_detail

using env_detail::EnvData;

CMD_ARGS_NAMESPACE_END
