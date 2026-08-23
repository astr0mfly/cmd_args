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
 * 语法期错误域（ParseError_E）：Parser 分配 token 时失败即抛 ErrorParse。
 * 典型场景：参数数量不足/不匹配（NOT_ENOUGH_ARGS / UNKNWON_ARGS，后者为历史拼写勿改）、
 * 选项缺值（NEED_VALUE）、聚合短选项含非 bool（MUST_BE_BOOL）、
 * 裸 '-'（UNEXCEPTED_BARE）、非法前缀（WRONG_PREFIX）、未知选项（UNKNOWN_OPTION）。
 * 三件套惯例与其它错误域一致（枚举 → 映射 → category → make_error_code 特化）。
 */
#include <cstring>
#include <string>
#include <system_error>

#include "cmd_args/base/def.h"

CMD_ARGS_NAMESPACE_BEGIN

/* 语法错误码：OK=0 表示无错 */
enum class ParseError_E
{
    OK = 0,
    NOT_ENOUGH_ARGS,   // 方位参数数量不足
    UNKNWON_ARGS,      // 参数数量不匹配 / 找不到（历史拼写，勿改）
    NEED_VALUE,        // 选项需要值但未给出
    MUST_BE_BOOL,      // 聚合短选项只允许 bool 成员
    UNEXCEPTED_BARE,   // 意外的裸 '-'
    WRONG_PREFIX,      // 无法识别的选项前缀
    UNKNOWN_OPTION     // 未注册的选项
};

/* 错误码 → 人类可读文本 */
static inline char const *mapParseError(int _ErrCode) noexcept
{
    switch (static_cast<ParseError_E>(_ErrCode)) {
    case ParseError_E::OK: return "no error";
    case ParseError_E::NOT_ENOUGH_ARGS: return "not enough named_arguments";
    case ParseError_E::UNKNWON_ARGS: return "argument number missmatching";
    case ParseError_E::NEED_VALUE: return "option need a value";
    case ParseError_E::MUST_BE_BOOL: return "aggregated short option must be bool";
    case ParseError_E::UNEXCEPTED_BARE: return "bare '-' is not accepted";
    case ParseError_E::WRONG_PREFIX: return "unrecognized option prefix";
    case ParseError_E::UNKNOWN_OPTION: return "unrecognized option";
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
struct is_error_code_enum<cmd_args::ParseError_E> : true_type
{};
}  // namespace std
