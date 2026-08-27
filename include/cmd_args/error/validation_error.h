/*
 * Copyright[2026][ValenciaFly]
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
 * 校验期错误域（ValidationError_E）：赋值完成后、动作执行前的统一校验失败。
 * 典型场景：必选参数/选项/环境变量缺失（MISSING_*），值未通过校验器（VALIDATION_FAILED）。
 * 三件套惯例与其它错误域一致（枚举 → 映射 → category → make_error_code 特化）。
 */
#include <cstring>
#include <string>
#include <system_error>

#include "cmd_args/base/def.h"

CMD_ARGS_NAMESPACE_BEGIN

/* 校验错误码：OK=0 表示无错 */
enum class ValidationError_E
{
    OK = 0,
    MISSING_ARGUMENT,   // 必需参数缺失
    MISSING_OPTION,     // 必需选项缺失
    MISSING_ENV,        // 必需环境变量缺失
    VALIDATION_FAILED   // 值未通过校验器
};

/* 错误码 → 人类可读文本 */
static inline char const *mapValidationError(int _ErrCode) noexcept
{
    switch (static_cast<ValidationError_E>(_ErrCode)) {
    case ValidationError_E::OK: return "no error";
    case ValidationError_E::MISSING_ARGUMENT: return "missing required argument";
    case ValidationError_E::MISSING_OPTION: return "missing required option";
    case ValidationError_E::MISSING_ENV: return "missing required environment";
    case ValidationError_E::VALIDATION_FAILED: return "value validation failed";
    default: return "unknown error";
    }
}

class ValidationErrorCategory : public std::error_category
{
public:
    ValidationErrorCategory()
        : error_category()
    {}
    const char *name() const noexcept override { return ENUM_NAME(ValidationError_E); }
    std::string message(int _V) const override { return mapValidationError(_V); }

    static ValidationErrorCategory const &instance()
    {
        static ValidationErrorCategory instance;
        return instance;
    }
};

inline std::error_code make_error_code(ValidationError_E code)
{
    return {
        static_cast<int>(code),
        ValidationErrorCategory::instance(),
    };
}

inline std::error_condition make_error_condition(ValidationError_E code)
{
    return {
        static_cast<int>(code),
        ValidationErrorCategory::instance(),
    };
}

class ErrorValidation : public std::logic_error
{
public:
    explicit ErrorValidation(ValidationError_E _E, std::string &&_Desc = {})
        : logic_error(_Desc.data())
        , ec__(make_error_code(_E))
    {}

    std::error_code const &code() const noexcept { return ec__; }
    char const            *what() const noexcept override
    {
        auto const *p = logic_error::what();
        if (std::strlen(p)) { return p; }

        return mapValidationError(ec__.value());
    }

private:
    std::error_code ec__;
};

CMD_ARGS_NAMESPACE_END

namespace std {
template <>
struct is_error_code_enum<cmd_args::ValidationError_E> : true_type
{};
}  // namespace std
