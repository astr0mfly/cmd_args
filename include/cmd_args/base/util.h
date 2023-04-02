#pragma once

#include <string>
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
    T result;
    iss >> result;
    return result;
}

}

CMD_ARGS_NAMESPACE_END