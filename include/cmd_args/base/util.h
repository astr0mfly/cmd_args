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

}  // namespace util

CMD_ARGS_NAMESPACE_END