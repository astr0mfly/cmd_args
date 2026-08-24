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

/*
 * 词法期错误域（LexError_E）：Lexer/Scanner 对输入源做分词时失败即抛 ErrorLexer。
 * 典型场景：空输入源（EMPTY_SOURCE）、未闭合字符串（UNTERMINATED_STRING）、
 * 裸 '-'（BARE_DASH）、非法 token（INVALID_TOKEN）。
 * 三件套惯例与其它错误域一致（枚举 → 映射 → category → make_error_code 特化）。
 */
#include <cstring>
#include <string>
#include <system_error>

#include "cmd_args/base/def.h"

CMD_ARGS_NAMESPACE_BEGIN

/* 词法错误码：OK=0 表示无错 */
enum class LexError_E
{
    OK = 0,
    EMPTY_SOURCE,         // 输入源为空（无任何可分词内容）
    UNTERMINATED_STRING,  // 引号字符串缺少结束引号
    BARE_DASH,            // 裸 '-'（应配短名或用作分隔符）
    INVALID_TOKEN         // 无法识别的 token
};

/* 错误码 → 人类可读文本 */
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

inline std::error_code make_error_code(LexError_E code)
{
    return {
        static_cast<int>(code),
        LexErrorCategory::instance(),
    };
}

inline std::error_condition make_error_condition(LexError_E code)
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
        , ec__(_Ec)
    {}
    explicit ErrorLexer(LexError_E _E)
        : logic_error("")
        , ec__(make_error_code(_E))
    {}
    explicit ErrorLexer(LexError_E _E, std::string &&_Desc)
        : logic_error(_Desc.data())
        , ec__(make_error_code(_E))
    {}

    std::error_code const &code() const noexcept { return ec__; }
    char const            *what() const noexcept override
    {
        auto const *p = logic_error::what();
        if (p && std::strlen(p)) { return p; }

        return mapLexError(ec__.value());
    }

private:
    std::error_code ec__;
};

CMD_ARGS_NAMESPACE_END

namespace std {
template <>
struct is_error_code_enum<cmd_args::LexError_E> : true_type
{};
}  // namespace std