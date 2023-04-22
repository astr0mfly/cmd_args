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
#include <system_error>

#include "cmd_args/base/def.h"

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

    explicit ErrorParse(ParseError_E _E, std::string &&_Desc)
        : logic_error(_Desc.data())
        , m_ec__(make_error_code(_E))
    {}

    std::error_code const &code() const noexcept { return m_ec__; }
    char const            *what() const noexcept override
    {
        auto const *p = logic_error::what();
        if (std::strlen(p)) { return p; }

        return mapParseError(m_ec__.value());
    }

private:
    std::error_code m_ec__;
};

CMD_ARGS_NAMESPACE_END

namespace std {
template <>
struct is_error_code_enum<CMD_ARGS_NAMESPACE(ParseError_E)> : true_type
{};
}  // namespace std
