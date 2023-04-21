#pragma once

#include <string>
#include <system_error>

#include "def.h"

CMD_ARGS_NAMESPACE_BEGIN

enum class ParseError_E
{
    OK = 0,
    NOT_ENOUGH_ARGS,
    UNKNWON_ARGS,
    NEED_VALUE,
    MUST_BE_BOOL,
    UNEXCEPTED_BARE,
    WRONG_PREFIX
};

static inline char const *mapParseError(int _ErrCode) noexcept
{
    switch (static_cast<ParseError_E>(_ErrCode)) {
    case ParseError_E::OK: return "no error";
    case ParseError_E::NOT_ENOUGH_ARGS: return "not enough named_arguments";
    case ParseError_E::UNKNWON_ARGS: return "argument number missmatching";
    case ParseError_E::NEED_VALUE: return "argument need a value";
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
    const char *name() const noexcept override { return ENUM_NAME(ParseError_E); }
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

class ErrorParse : public std::logic_error
{
public:
    explicit ErrorParse(std::error_code _Ec)
        : logic_error("")
        , m_ec__(_Ec)
    {}
    explicit ErrorParse(ParseError_E _E)
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
struct is_error_code_enum<CMD_ARGS_NAMESPACE(ParseError_E)> : true_type
{};
}  // namespace std

CMD_ARGS_NAMESPACE_BEGIN

enum class ArgError_E
{
    OK = 0,
    NEED_NAME,
    NEED_HELP
};

static inline char const *mapArgumentError(int _ErrCode) noexcept
{
    switch (static_cast<ArgError_E>(_ErrCode)) {
    case ArgError_E::OK: return "no error";
    case ArgError_E::NEED_NAME: return "need to named";
    case ArgError_E::NEED_HELP: return "need to set infomation of usage";

    default: return "unknwon error";
    }
}

class ArgErrorCategory : public std::error_category
{
public:
    ArgErrorCategory()
        : error_category()
    {}
    const char *name() const noexcept override { return ENUM_NAME(ArgError_E); }
    std::string message(int _V) const override { return mapArgumentError(_V); }

    static ArgErrorCategory const &instance()
    {
        static ArgErrorCategory instance;
        return instance;
    }
};

std::error_code make_error_code(ArgError_E code)
{
    return {
        static_cast<int>(code),
        ArgErrorCategory::instance(),  // 这里暂时用自带的 category
    };
}

std::error_condition make_error_condition(ArgError_E code)
{
    return {
        static_cast<int>(code),
        ArgErrorCategory::instance(),  // 这里暂时用自带的 category
    };
}

class ErrorArgument : public std::logic_error
{
public:
    explicit ErrorArgument(std::error_code _Ec)
        : logic_error("")
        , m_ec__(_Ec)
    {}
    explicit ErrorArgument(ArgError_E _E)
        : logic_error("")
        , m_ec__(make_error_code(_E))
    {}

    std::error_code const &code() const noexcept { return m_ec__; }
    char const *what() const noexcept override { return mapArgumentError(m_ec__.value()); }

private:
    std::error_code m_ec__;
};

CMD_ARGS_NAMESPACE_END

namespace std {
template <>
struct is_error_code_enum<CMD_ARGS_NAMESPACE(ArgError_E)> : public true_type
{};
}  // namespace std