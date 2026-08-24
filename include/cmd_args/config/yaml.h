/*
 * Copyright [2023] [ValenciaFly] <pengwanring@live.com>
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
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "cmd_args/base/def.h"
#include "cmd_args/base/util.h"

CMD_ARGS_NAMESPACE_BEGIN

/*
    极简 YAML 子集文档（扁平 map，仅覆盖参数配置类需求）：
    - `key: value` 顶层键值；`#` 行注释；空行忽略；前导空白容忍
    - 标量：双引号字符串（支持 \" \\ \n \t \r 转义）、plain 值、空值
    - 不支持（留给未来完整 YAML / JSON 扩展）：嵌套 map/list、`---` 文档
      分隔、锚点、flow 集合、多行块
    - 发射（dump）时所有值一律带双引号输出，保证解析/发射往返一致
*/
class yaml_doc
{
public:
    using Entries = std::vector<std::pair<std::string, std::string>>;

    yaml_doc() = default;

    /*
        解析文本；成功返回文档，失败返回 nullopt 并把错误描述写入 _Error（可空）。
        失败情形：非空非注释行缺少 ':'、双引号未闭合、引号串后存在非注释尾巴。
    */
    static std::optional<yaml_doc> parse(std::string_view _Text, std::string *_Error = nullptr)
    {
        yaml_doc         doc;
        std::string      line;
        std::string      err;
        std::size_t      n = 0;
        std::size_t      pos = 0;
        while (pos <= _Text.size()) {
            auto endPos = _Text.find('\n', pos);
            line       = _Text.substr(pos, endPos == std::string_view::npos
                                             ? _Text.size() - pos
                                             : endPos - pos);
            pos        = (endPos == std::string_view::npos) ? _Text.size() + 1 : endPos + 1;
            ++n;

            // 去行尾 \r（CRLF 兼容）
            if (!line.empty() && line.back() == '\r') { line.pop_back(); }

            std::string trimmed = util::trim(line);
            if (trimmed.empty() || trimmed.front() == '#') continue;  // 空行 / 注释行

            auto kv = __parseLine(trimmed, n, err);
            if (!kv) {
                if (_Error) *_Error = err;
                return std::nullopt;
            }
            doc.entries__.emplace_back(std::move(kv->first), std::move(kv->second));
        }
        return doc;
    }

    /* 序列化为扁平 YAML 文本（值一律双引号） */
    std::string dump() const
    {
        std::string ret;
        for (auto const &[k, v] : entries__) {
            ret += k;
            ret += ": \"";
            ret += __escape(v);
            ret += "\"\n";
        }
        return ret;
    }

    bool has(std::string const &_Key) const noexcept
    {
        return __find(_Key) != entries__.cend();
    }

    /* 读取语义值（已去引号/反转义）；缺失返回 _Fallback */
    std::string get(std::string const &_Key, std::string const &_Fallback = {}) const
    {
        auto it = __find(_Key);
        return it == entries__.cend() ? _Fallback : it->second;
    }

    void set(std::string _Key, std::string _Value)
    {
        auto it = __find(_Key);
        if (it != entries__.end()) {
            it->second = std::move(_Value);
            return;
        }
        entries__.emplace_back(std::move(_Key), std::move(_Value));
    }

    bool erase(std::string const &_Key) noexcept
    {
        auto it = __find(_Key);
        if (it == entries__.end()) return false;
        entries__.erase(it);
        return true;
    }

    size_t                    size() const noexcept { return entries__.size(); }
    bool                      empty() const noexcept { return entries__.empty(); }
    Entries const            &entries() const noexcept { return entries__; }

private:
    using KV    = std::pair<std::string, std::string>;
    using const_it = Entries::const_iterator;
    using it      = Entries::iterator;

    const_it __find(std::string const &_Key) const noexcept
    {
        for (auto it = entries__.cbegin(); it != entries__.cend(); ++it) {
            if (it->first == _Key) return it;
        }
        return entries__.cend();
    }
    it __find(std::string const &_Key) noexcept
    {
        for (auto it = entries__.begin(); it != entries__.end(); ++it) {
            if (it->first == _Key) return it;
        }
        return entries__.end();
    }

    /* 行内 '#' 注释：注释符前必须是空白才生效 */
    static std::string __stripComment(std::string const &_V)
    {
        for (std::size_t i = 1; i < _V.size(); ++i) {
            if (_V[i] == '#' && (_V[i - 1] == ' ' || _V[i - 1] == '\t')) {
                return _V.substr(0, i);
            }
        }
        return _V;
    }

    static std::string __escape(std::string const &_V)
    {
        std::string ret;
        for (char ch : _V) {
            switch (ch) {
            case '"': ret += "\\\""; break;
            case '\\': ret += "\\\\"; break;
            case '\n': ret += "\\n"; break;
            case '\t': ret += "\\t"; break;
            case '\r': ret += "\\r"; break;
            default: ret += ch; break;
            }
        }
        return ret;
    }

    /* 反转义：\" \\ \n \t \r；其余 \x 原样保留 */
    static std::string __unescape(std::string const &_V)
    {
        std::string ret;
        for (std::size_t i = 0; i < _V.size(); ++i) {
            if (_V[i] != '\\' || i + 1 >= _V.size()) {
                ret += _V[i];
                continue;
            }
            char nxt = _V[++i];
            switch (nxt) {
            case '"': ret += '"'; break;
            case '\\': ret += '\\'; break;
            case 'n': ret += '\n'; break;
            case 't': ret += '\t'; break;
            case 'r': ret += '\r'; break;
            default: ret += '\\'; ret += nxt; break;
            }
        }
        return ret;
    }

    /*
        解析一行 `key: value`。
        返回 {key, 语义值}；无 ':'、引号未闭合、引号后带非注释尾巴时失败。
    */
    static std::optional<KV> __parseLine(std::string const &_Line, std::size_t _LineNo,
                                         std::string &_Err)
    {
        // 找第一个"裸冒号"（引号内与转义引号不计）
        std::size_t col = std::string::npos;
        bool        inQuote = false;
        for (std::size_t i = 0; i < _Line.size(); ++i) {
            char ch = _Line[i];
            if (ch == '\\' && i + 1 < _Line.size()) {
                ++i;
                continue;
            }
            if (ch == '"') {
                inQuote = !inQuote;
                continue;
            }
            if (ch == ':' && !inQuote) {
                col = i;
                break;
            }
        }
        if (col == std::string::npos) {
            _Err = "yaml: line " + std::to_string(_LineNo) + ": missing ':' separator";
            return std::nullopt;
        }

        std::string key = util::trim(_Line.substr(0, col));
        if (key.empty()) {
            _Err = "yaml: line " + std::to_string(_LineNo) + ": empty key";
            return std::nullopt;
        }

        std::string value = util::trim(_Line.substr(col + 1));

        // 空值（含 `key: # comment`）
        if (value.empty() || value.front() == '#') return KV{ std::move(key), {} };

        if (value.front() == '"') {
            // 匹配闭合引号（处理转义）
            std::size_t end = std::string::npos;
            for (std::size_t i = 1; i < value.size(); ++i) {
                if (value[i] == '\\' && i + 1 < value.size()) {
                    ++i;
                    continue;
                }
                if (value[i] == '"') {
                    end = i;
                    break;
                }
            }
            if (end == std::string::npos) {
                _Err = "yaml: line " + std::to_string(_LineNo)
                       + ": unterminated double-quoted string";
                return std::nullopt;
            }
            std::string inner = value.substr(1, end - 1);
            std::string tail  = util::trim(value.substr(end + 1));
            if (!tail.empty() && tail.front() != '#') {
                _Err = "yaml: line " + std::to_string(_LineNo)
                       + ": trailing characters after quoted string";
                return std::nullopt;
            }
            return KV{ std::move(key), __unescape(inner) };
        }

        // plain 值：截断行内注释（# 前须空白）
        return KV{ std::move(key), util::trim(__stripComment(value)) };
    }

private:
    Entries entries__;
};

CMD_ARGS_NAMESPACE_END