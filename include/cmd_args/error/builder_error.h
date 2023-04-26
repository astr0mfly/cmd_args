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

namespace detail {
enum class BuildError_E
{
    OK = 0,
    NEED_NAME,
    NEED_HELP
};

static inline char const *mapArgumentError(int _ErrCode) noexcept
{
    switch (static_cast<BuildError_E>(_ErrCode)) {
    case BuildError_E::OK: return "no error";
    case BuildError_E::NEED_NAME: return "need to named";
    case BuildError_E::NEED_HELP: return "need to set infomation of usage";

    default: return "unknwon error";
    }
}

class ArgErrorCategory : public std::error_category
{
public:
    ArgErrorCategory()
        : error_category()
    {}
    const char *name() const noexcept override { return ENUM_NAME(BuildError_E); }
    std::string message(int _V) const override { return mapArgumentError(_V); }

    static ArgErrorCategory const &instance()
    {
        static ArgErrorCategory instance;
        return instance;
    }
};

std::error_code make_error_code(BuildError_E code)
{
    return {
        static_cast<int>(code),
        ArgErrorCategory::instance(),  // 这里暂时用自带的 category
    };
}

std::error_condition make_error_condition(BuildError_E code)
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
    explicit ErrorArgument(BuildError_E _E)
        : logic_error("")
        , m_ec__(make_error_code(_E))
    {}

    std::error_code const &code() const noexcept { return m_ec__; }
    char const *what() const noexcept override { return mapArgumentError(m_ec__.value()); }

private:
    std::error_code m_ec__;
};
}  // namespace detail

using detail::BuildError_E;
using detail::ErrorArgument;

CMD_ARGS_NAMESPACE_END

namespace std {
template <>
struct is_error_code_enum<cmd_args::BuildError_E> : public true_type
{};
}  // namespace std
