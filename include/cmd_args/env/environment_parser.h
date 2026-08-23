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

#include <cctype>
#include <cstdlib>
#include <functional>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "cmd_args/context/parse_fwd.h"
#include "cmd_args/env/environment.h"

CMD_ARGS_NAMESPACE_BEGIN

/*
    环境变量仓库：解析 envp、展示、交互（设置/删除）、引用与修改追踪、
    ${VAR}/%VAR%/$VAR 展开、并把会话中的变更回写到操作系统环境。

    这是 cmd_args 的差异化模块：多数命令行解析库只是“读一眼”环境变量，
    这里环境变量是完整的一等公民——可展示、可交互、可回写。
*/
class environment_store
{
public:
    environment_store() = default;

    /* 装载进程环境（envp） */
    void parse(Envs_T const &_Envp)
    {
        for (auto const &e : _Envp) {
            auto eq = e.find('=');
            if (eq == std::string::npos) continue;  // 无 '=' 的行不是合法环境项
            m_setEnvs__.emplace(e.substr(0, eq), e.substr(eq + 1), false);
        }
    }

    bool has(std::string const &_Key) const noexcept
    {
        return m_setEnvs__.find(_Key) != m_setEnvs__.cend();
    }

    environment const *find(std::string const &_Key) const noexcept
    {
        auto it = m_setEnvs__.find(_Key);
        if (it == m_setEnvs__.cend()) return nullptr;
        return std::addressof(*it);
    }

    /* 读取并标记“被引用” */
    std::string get(std::string const &_Key) const
    {
        auto const *p = find(_Key);
        if (!p) return {};
        p->bReferenced__ = true;
        return p->strValue;
    }

    /* 读取，缺失时返回兜底值 */
    std::string get_or(std::string const &_Key, std::string const &_Default = {}) const
    {
        auto const *p = find(_Key);
        if (!p) return _Default;
        p->bReferenced__ = true;
        return p->strValue;
    }

    /* 标记“被引用”（不取值） */
    void touch(std::string const &_Key) const noexcept
    {
        auto const *p = find(_Key);
        if (p) p->bReferenced__ = true;
    }

    /* 设置：存在则改值，不存在则新建 */
    void set(std::string _Key, std::string _Value)
    {
        auto it = m_setEnvs__.find(_Key);
        if (it == m_setEnvs__.cend()) {
            m_setEnvs__.emplace(std::move(_Key), std::move(_Value), true);
            return;
        }
        it->strValue    = std::move(_Value);
        it->bModified__ = true;
    }

    /* 删除，返回原先是否存在 */
    bool unset(std::string const &_Key)
    {
        auto it = m_setEnvs__.find(_Key);
        if (it == m_setEnvs__.cend()) return false;
        m_setEnvs__.erase(it);
        return true;
    }

    size_t size() const noexcept { return m_setEnvs__.size(); }
    bool   empty() const noexcept { return m_setEnvs__.empty(); }

    size_t referenced_count() const noexcept
    {
        size_t n = 0;
        for (auto const &e : m_setEnvs__) {
            if (e.bReferenced__) ++n;
        }
        return n;
    }

    size_t changed_count() const noexcept
    {
        size_t n = 0;
        for (auto const &e : m_setEnvs__) {
            if (e.bModified__) ++n;
        }
        return n;
    }

    /* 公开只读遍历（配置导出/快照提取等外部只读消费） */
    void for_each(std::function<void(environment const &)> const &_Fn) const
    {
        for (auto const &e : m_setEnvs__) { _Fn(e); }
    }

    /*
        环境展示：
        ```
        environments: <program>
        No. : "key" = "value"
        0   : "A" = "1"
        1*  : "B" = "2"     (* 被修改)
        2>  : "C" = "3"     (> 被引用)
        3*+ : "D" = "4"     (+ 本次新建)
        ```
    */
    std::string env_string(std::string const &_Program) const
    {
        std::ostringstream oss;
        oss << "environments: " << _Program << "\n";
        oss << "No. : \"key\" = \"value\"\n";
        size_t i = 0;
        for (auto const &e : m_setEnvs__) {
            oss << i;
            if (e.bModified__) oss << '*';
            if (e.bReferenced__) oss << '>';
            if (e.bNew__) oss << '+';
            oss << " : \"" << e.strKey << "\" = \"" << e.strValue << "\"\n";
            ++i;
        }
        return oss.str();
    }

    /*
        环境变量展开：${NAME} / %NAME% / $NAME
        命中则标记引用；未命中保持原样（留给后续处理或用户自省）。
    */
    std::string expand(std::string const &_Text) const
    {
        std::string ret;
        ret.reserve(_Text.size());

        for (size_t i = 0; i < _Text.size();) {
            char ch = _Text[i];
            if (ch == '$') {
                std::string name;
                size_t      endPos = std::string::npos;
                bool        brace  = false;
                if (i + 1 < _Text.size() && _Text[i + 1] == '{') {
                    endPos = _Text.find('}', i + 2);
                    if (endPos != std::string::npos) {
                        name  = _Text.substr(i + 2, endPos - i - 2);
                        brace = true;
                    }
                }
                else if (i + 1 < _Text.size()
                         && (std::isalpha(static_cast<unsigned char>(_Text[i + 1]))
                             || _Text[i + 1] == '_')) {
                    size_t j = i + 1;
                    while (j < _Text.size()
                           && (std::isalnum(static_cast<unsigned char>(_Text[j]))
                               || _Text[j] == '_')) {
                        ++j;
                    }
                    name   = _Text.substr(i + 1, j - i - 1);
                    endPos = j;
                }

                if (!name.empty() && endPos != std::string::npos) {
                    auto const *p = find(name);
                    if (p) {
                        p->bReferenced__ = true;
                        ret += p->strValue;
                        i = brace ? endPos + 1 : endPos;
                        continue;
                    }
                }
                ret += ch;
                ++i;
                continue;
            }
            if (ch == '%') {
                auto endPos = _Text.find('%', i + 1);
                if (endPos != std::string::npos) {
                    auto name = _Text.substr(i + 1, endPos - i - 1);
                    if (!name.empty()) {
                        auto const *p = find(name);
                        if (p) {
                            p->bReferenced__ = true;
                            ret += p->strValue;
                            i = endPos + 1;
                            continue;
                        }
                    }
                }
                ret += ch;
                ++i;
                continue;
            }
            ret += ch;
            ++i;
        }

        return ret;
    }

    /*
        把会话中的修改/新建回写到操作系统环境变量。
        返回实际写回的 "KEY=VALUE" 列表；写回成功后清除 modified/new 标记。
    */
    std::vector<std::string> apply()
    {
        std::vector<std::string> applied;
        for (auto &e : m_setEnvs__) {
            // 被修改的（--env-set 改已有值）与本次新建的（--env-set 新建）都回写
            if (!e.bModified__ && !e.bNew__) continue;
#ifdef _WIN32
            _putenv_s(e.strKey.c_str(), e.strValue.c_str());
#else
            setenv(e.strKey.c_str(), e.strValue.c_str(), 1);
#endif
            e.bModified__ = false;
            e.bNew__      = false;
            applied.emplace_back(e.strKey + "=" + e.strValue);
        }
        return applied;
    }

private:
    envs_t m_setEnvs__;
};

/* 兼容旧拼写：原拼写错误（envirionment_parser）按项目约定保留，不得删除 */
using envirionment_parser = environment_store;

CMD_ARGS_NAMESPACE_END