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

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "def.h"

CMD_ARGS_NAMESPACE_BEGIN

namespace util {

/*
    类型名称反射：返回 T 的规范化类型名，
    用于选项/参数的类型匹配检查（get<T> 时校验）与 schema 展示。
    未特化的 T 一律返回 "null"（视为"未定型"，跳过严格匹配）。
*/
template <typename T>
inline std::string type_string()
{
    return "null";
}

template <>
inline std::string type_string<bool>()
{
    return "bool";
}

template <>
inline std::string type_string<int>()
{
    return "int";
}

template <>
inline std::string type_string<int64_t>()
{
    return "int64_t";
}

template <>
inline std::string type_string<double>()
{
    return "double";
}

template <>
inline std::string type_string<float>()
{
    return "float";
}

template <>
inline std::string type_string<char>()
{
    return "char";
}

template <>
inline std::string type_string<unsigned int>()
{
    return "unsigned int";
}

template <>
inline std::string type_string<size_t>()
{
    return "size_t";
}

template <>
inline std::string type_string<std::string>()
{
    return "string";
}

/* 任意可流输出类型 → 字符串（选项/参数存值统一为字符串） */
template <typename T>
std::string to_string(T const &_Value)
{
    std::ostringstream oss;
    oss << _Value;
    return oss.str();
}

/* 字符串 → 目标类型（读取取值时的还原；经 istringstream 完成） */
template <typename T>
T parse_value(std::string const &_Value)
{
    std::istringstream iss(_Value);
    T                  result{};
    iss >> result;
    return result;
}

/* 宽松布尔解析：true/yes/on 为真，false/no/off 为假，其余按 0/1 数字解析 */
inline bool parse_bool(std::string const &_Value)
{
    if (_Value == "true" || _Value == "TRUE" || _Value == "yes" || _Value == "on") return true;
    if (_Value == "false" || _Value == "FALSE" || _Value == "no" || _Value == "off") return false;
    return parse_value<bool>(_Value);
}

/* 前缀判断（词法扫描用："--"、"-"、"--name=" 等） */
inline bool starts_with(std::string const &_Text, std::string const &_Prefix) noexcept
{
    return _Text.size() >= _Prefix.size()
           && _Text.compare(0, _Prefix.size(), _Prefix) == 0;
}

/* 后缀判断 */
inline bool ends_with(std::string const &_Text, std::string const &_Suffix) noexcept
{
    return _Text.size() >= _Suffix.size()
           && _Text.compare(_Text.size() - _Suffix.size(), _Suffix.size(), _Suffix) == 0;
}

/* 去掉首尾空白（空白 / 制表 / 换行；loop 的整行输入先经此清理） */
inline std::string trim(std::string const &_Text)
{
    auto first = _Text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    auto last = _Text.find_last_not_of(" \t\r\n");
    return _Text.substr(first, last - first + 1);
}

/*
    按分隔符切分；空段保留，与 strtok 语义不同
*/
inline std::vector<std::string> split(std::string const &_Text, char _Delim)
{
    std::vector<std::string> parts;
    std::string              cur;
    for (char ch : _Text) {
        if (ch == _Delim) {
            parts.emplace_back(std::move(cur));
            cur.clear();
        }
        else {
            cur += ch;
        }
    }
    parts.emplace_back(std::move(cur));
    return parts;
}

/*
    在头尾两端切掉一对括号类字符；不匹配则原样返回
*/
inline std::string unwrap(std::string const &_Text, char _Left, char _Right)
{
    std::string ret = _Text;
    if (ret.size() >= 2 && ret.front() == _Left && ret.back() == _Right) {
        ret = ret.substr(1, ret.size() - 2);
    }
    return ret;
}

/* 字符串替换：把 str 中所有 from 替换为 to（action 的 {value} 占位符替换即用它） */
std::string replace(std::string const &str, std::string const &from, std::string const &to)
{
    std::string ret;
    std::size_t pos = 0, pre_pos = 0;
    while ((pos = str.find(from, pre_pos)) != std::string::npos) {
        ret += str.substr(pre_pos, pos - pre_pos) + to;
        pre_pos = pos + from.length();
    }
    ret += str.substr(pre_pos);
    return ret;
}

/* "KEY=VALUE" 行拆分为键值对（环境变量装载/展示用）；无 '=' 返回空对 */
std::pair<std::string, std::string> parse_line(std::string const &_Line)
{
    auto pos = _Line.find('=');
    if (pos == std::string::npos) return {};
    return { _Line.substr(0, pos), _Line.substr(pos + 1) };
}

/*
    Group：initializer_list 的轻量包装，
    让 build({...}) 这种"一组同构构筑"写法有类型化的入口。
*/
template <class T>
class Group : public std::initializer_list<T>
{
public:
    using Super_T   = std::initializer_list<T>;
    using Element_T = typename Super_T::value_type;

    Group() = default;
    Group(std::initializer_list<T> _Builds)
        : std::initializer_list<T>(std::move(_Builds))
    {}
};

}  // namespace util

using util::Group;

CMD_ARGS_NAMESPACE_END