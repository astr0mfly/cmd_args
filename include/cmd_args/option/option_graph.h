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

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "cmd_args/base/def.h"

CMD_ARGS_NAMESPACE_BEGIN

/**
 * @brief Option 关系有向图：存储选项之间的条件（Conditional）与依赖（Dependency）关系。
 *
 * 边类型：
 * - Conditional（-a?b:c）：a 为真时激活 b，否则激活 c
 * - Dependency（-a->b）：先执行 a，再执行 b
 *
 * 图在 OptionBuilder 注册阶段构建（addEdge），
 * 在 OptionParser 解析时填充节点状态（enable/disable），
 * 在 Executor 执行前进行拓扑排序确定执行顺序。
 */
class OptionGraph
{
public:
    /// @brief 边类型
    enum class EdgeKind_E : char
    {
        Conditional,  ///< 条件边：-a?b:c
        Dependency    ///< 依赖边：-a->b
    };

    /// @brief 节点状态
    struct Node
    {
        std::string strName;          ///< 选项名
        bool        bEnabled = false; ///< 是否被激活（解析期填充）
    };

    /// @brief 有向边
    struct Edge
    {
        std::string strFrom;       ///< 源节点（条件选项 / 依赖源）
        std::string strTo;         ///< 目标节点（激活选项 / 依赖目标）
        EdgeKind_E  enKind;        ///< 边类型
        std::string strFallback;   ///< 条件边的 else 分支（"_" 表示无操作）
    };

    /// @brief 添加关系边
    /// @param _From 源选项名
    /// @param _To 目标选项名
    /// @param _Kind 边类型
    /// @param _Fallback 条件边的 else 分支（仅 Conditional 有意义）
    void addEdge(std::string _From, std::string _To, EdgeKind_E _Kind, std::string _Fallback = "")
    {
        // 确保节点存在
        __ensureNode(_From);
        __ensureNode(_To);
        if (!_Fallback.empty() && _Fallback != "_") { __ensureNode(_Fallback); }

        vecEdges__.emplace_back(Edge{
            std::move(_From),
            std::move(_To),
            _Kind,
            std::move(_Fallback)
        });
    }

    /// @brief 启用/禁用节点（OptionParser 解析时调用）
    /// @param _Name 选项名
    /// @param _Enabled 是否激活
    void setNodeState(std::string const &_Name, bool _Enabled)
    {
        auto it = mapNodes__.find(_Name);
        if (it != mapNodes__.end()) { it->second.bEnabled = _Enabled; }
    }

    /// @brief 查询节点是否激活
    auto isNodeEnabled(std::string const &_Name) const -> bool
    {
        auto it = mapNodes__.find(_Name);
        return it != mapNodes__.end() && it->second.bEnabled;
    }

    /// @brief 拓扑排序：返回节点的执行顺序（入度为零的先执行）
    /// @return 排序后的选项名列表
    auto topologicalSort() const -> std::vector<std::string>
    {
        // 构建入度表
        std::map<std::string, int> inDegree;
        for (auto const &node : mapNodes__) { inDegree[node.first] = 0; }
        for (auto const &edge : vecEdges__) { ++inDegree[edge.strTo]; }

        // BFS 拓扑排序
        std::vector<std::string> queue;
        for (auto const &kv : inDegree) {
            if (kv.second == 0) { queue.emplace_back(kv.first); }
        }

        std::vector<std::string> result;
        size_t idx = 0;
        while (idx < queue.size()) {
            auto const &cur = queue[idx++];
            result.emplace_back(cur);
            for (auto const &edge : vecEdges__) {
                if (edge.strFrom == cur) {
                    if (--inDegree[edge.strTo] == 0) {
                        queue.emplace_back(edge.strTo);
                    }
                }
            }
        }

        return result;
    }

    /// @brief 求值：根据节点状态激活条件边的目标节点
    /// @return 新激活的选项名列表
    auto evaluate() -> std::vector<std::string>
    {
        std::vector<std::string> activated;

        for (auto const &edge : vecEdges__) {
            if (edge.enKind == EdgeKind_E::Conditional) {
                bool condState = isNodeEnabled(edge.strFrom);
                if (condState) {
                    // 条件为真：激活 then 分支
                    if (edge.strTo != "_") {
                        setNodeState(edge.strTo, true);
                        activated.emplace_back(edge.strTo);
                    }
                }
                else {
                    // 条件为假：激活 else 分支
                    if (!edge.strFallback.empty() && edge.strFallback != "_") {
                        setNodeState(edge.strFallback, true);
                        activated.emplace_back(edge.strFallback);
                    }
                }
            }
        }

        return activated;
    }

    /// @brief 获取所有边（只读）
    auto edges() const -> std::vector<Edge> const & { return vecEdges__; }

    /// @brief 获取所有节点（只读）
    auto nodes() const -> std::map<std::string, Node> const & { return mapNodes__; }

    /// @brief 是否为空（无边）
    bool empty() const noexcept { return vecEdges__.empty(); }

    /// @brief 清空图（每次解析前调用）
    void clear()
    {
        mapNodes__.clear();
        vecEdges__.clear();
    }

private:
    /// @brief 确保节点存在（不存在则创建）
    void __ensureNode(std::string const &_Name)
    {
        if (mapNodes__.find(_Name) == mapNodes__.end()) {
            mapNodes__[_Name] = Node{ _Name, false };
        }
    }

    std::map<std::string, Node> mapNodes__;  ///< 节点表
    std::vector<Edge>           vecEdges__;  ///< 边列表
};

CMD_ARGS_NAMESPACE_END
