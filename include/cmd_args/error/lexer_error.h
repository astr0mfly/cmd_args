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

#include <cstring>
#include <string>
#include <system_error>

#include "cmd_args/base/def.h"

CMD_ARGS_NAMESPACE_BEGIN

enum class LexError_E
{
    OK = 0,
    EMPTY_SOURCE,
    UNTERMINATED_STRING,
    BARE_DASH,
    INVALID_TOKEN
};

static inline char const *mapLexError(int _ErrCode) noexcept
{
    switch (static_cast<LexError_E>(_ErrCode)) {
    case LexError_E::OK: return "no error";
    case LexError_E::EMPTY_SOURCE: return "no input source";
    case LexError_E::UNTERMINATED_STRING: return "string is not terminated";
    case LexError_E::BARE_DASH: return "bare '-' is not accepted";
    case LexError_E::INVALID_TOKEN: return "invalid token";
    default: return "unknwon error";
    }
}

class LexErrorCategory : public std::error_category
{
public:
    LexErrorCategory()
        : error_category()
    {}
    const char *name() const noexcept override { return ENUM_NAME(LexError_E); }
    std::string message(int _V) const override { return mapLexError(_V); }

    static LexErrorCategory const &instance()
    {
        static LexErrorCategory instance;
        return instance;
    }
};

std::error_code make_error_code(LexError_E code)
{
    return {
        static_cast<int>(code),
        LexErrorCategory::instance(),
    };
}

std::error_condition make_error_condition(LexError_E code)
{
    return {
        static_cast<int>(code),
        LexErrorCategory::instance(),
    };
}

class ErrorLexer : public std::logic_error
{
public:
    explicit ErrorLexer(std::error_code _Ec)
        : logic_error("")
        , m_ec__(_Ec)
    {}
    explicit ErrorLexer(LexError_E _E)
        : logic_error("")
        , m_ec__(make_error_code(_E))
    {}
    explicit ErrorLexer(LexError_E _E, std::string &&_Desc)
        : logic_error(_Desc.data())
        , m_ec__(make_error_code(_E))
    {}

    std::error_code const &code() const noexcept { return m_ec__; }
    char const            *what() const noexcept override
    {
        auto const *p = logic_error::what();
        if (p && std::strlen(p)) { return p; }

        return mapLexError(m_ec__.value());
    }

private:
    std::error_code m_ec__;
};

CMD_ARGS_NAMESPACE_END

namespace std {
template <>
struct is_error_code_enum<cmd_args::LexError_E> : true_type
{};
}  // namespace std