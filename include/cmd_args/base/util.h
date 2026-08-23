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

template <typename T>
std::string to_string(T const &_Value)
{
    std::ostringstream oss;
    oss << _Value;
    return oss.str();
}

template <typename T>
T parse_value(std::string const &_Value)
{
    std::istringstream iss(_Value);
    T                  result{};
    iss >> result;
    return result;
}

inline bool parse_bool(std::string const &_Value)
{
    if (_Value == "true" || _Value == "TRUE" || _Value == "yes" || _Value == "on") return true;
    if (_Value == "false" || _Value == "FALSE" || _Value == "no" || _Value == "off") return false;
    return parse_value<bool>(_Value);
}

inline bool starts_with(std::string const &_Text, std::string const &_Prefix) noexcept
{
    return _Text.size() >= _Prefix.size()
           && _Text.compare(0, _Prefix.size(), _Prefix) == 0;
}

inline bool ends_with(std::string const &_Text, std::string const &_Suffix) noexcept
{
    return _Text.size() >= _Suffix.size()
           && _Text.compare(_Text.size() - _Suffix.size(), _Suffix.size(), _Suffix) == 0;
}

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

std::pair<std::string, std::string> parse_line(std::string const &_Line)
{
    auto pos = _Line.find('=');
    if (pos == std::string::npos) return {};
    return { _Line.substr(0, pos), _Line.substr(pos + 1) };
}

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