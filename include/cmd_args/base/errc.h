#pragma once

#include <string>
#include <system_error>

#include "def.h"

CMD_ARGS_NAMESPACE_BEGIN

enum class ParseError_E
{
    OK = 0,
    UNRECOGNIZED_NAME,
    NEED_VALUE,
    MUST_BE_BOOL,
    UNEXCEPTED_BARE,
    WRONG_PREFIX
};

static inline char const *mapParseError(int _ErrCode) noexcept
{
    switch (static_cast<ParseError_E>(_ErrCode)) {
    case ParseError_E::OK: return "no error";
    case ParseError_E::UNRECOGNIZED_NAME: return "create a libmodbus context for TCP/IPv4 failed";
    case ParseError_E::NEED_VALUE: return "The slave number is invalid";
    case ParseError_E::MUST_BE_BOOL: return "establish a Modbus connection failed";
    case ParseError_E::UNEXCEPTED_BARE: return "set timeout for response failed";
    case ParseError_E::WRONG_PREFIX: return "set timeout for response failed";
    default: return "unknwon error";
    }
}

class ParseErrorCategory : public std::error_category
{
public:
    ParseErrorCategory()
        : error_category()
    {}
    const char *name() const noexcept override { return "parse error"; }
    std::string message(int _V) const override { return mapParseError(_V); }

    static ParseErrorCategory const &instance()
    {
        static ParseErrorCategory instance;
        return instance;
    }
};

std::error_code make_error_code(ParseError_E code)
{
    return {
        static_cast<int>(code),
        ParseErrorCategory::instance(),  // 这里暂时用自带的 category
    };
}

std::error_condition make_error_condition(ParseError_E code)
{
    return {
        static_cast<int>(code),
        ParseErrorCategory::instance(),  // 这里暂时用自带的 category
    };
}

class parse_error : public std::logic_error
{
public:
    explicit parse_error(std::error_code _Ec)
        : logic_error("")
        , m_ec__(_Ec)
    {}
    explicit parse_error(ParseError_E _E)
        : logic_error("")
        , m_ec__(make_error_code(_E))
    {}

    std::error_code const &code() const noexcept { return m_ec__; }
    char const            *what() const noexcept override { return mapParseError(m_ec__.value()); }

private:
    std::error_code m_ec__;
};

CMD_ARGS_NAMESPACE_END

namespace std {
template <>
struct is_error_code_enum<CMD_ARGS_NAMESPACE(ParseError_E)> : public true_type
{};
}  // namespace std
