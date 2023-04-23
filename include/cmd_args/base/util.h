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

#include <string>
#include <utility>

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
    T                  result;
    iss >> result;
    return result;
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