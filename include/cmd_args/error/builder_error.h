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
 * 构筑期错误域（BuildError_E）：
 * 在 Builder 构筑参数/选项/标志时做合法性检查，失败即抛 ErrorArgument。
 * 典型场景：重名（DUPLICATE_NAME）、缺名（NEED_NAME）、缺帮助（NEED_HELP）、
 * 不支持的存储类型（UNSUPPORT_TYPE）、非法的名字前缀（BAD_PREFIX）。
 *
 * 三件套惯例（各错误域一致）：
 *   enum 错误码 → mapXxxError 文本映射 → XxxErrorCategory::instance()
 *   → make_error_code / make_error_condition（注册进 std::error_code 体系）
 *   → is_error_code_enum 特化。
 */
#include <string>
#include <system_error>

#include "cmd_args/base/def.h"

CMD_ARGS_NAMESPACE_BEGIN

namespace detail {
/* 构筑期错误码：OK=0 表示无错（错误码体系要求 0 为成功态） */
enum class BuildError_E
{
    OK = 0,
    NEED_NAME,      // 选项/参数缺少名字
    NEED_HELP,      // 缺少帮助文案
    DUPLICATE_NAME, // 名字重复（与已有参数/选项/短回路冲突）
    UNSUPPORT_TYPE, // 该类型不允许作为选项存储类型
    BAD_PREFIX      // 名字以 '-' 开头（应裸名构筑，勿带前缀）
};

/* 错误码 → 人类可读文本（what() 与 ec.message() 共用） */
static inline char const *mapArgumentError(int _ErrCode) noexcept
{
    switch (static_cast<BuildError_E>(_ErrCode)) {
    case BuildError_E::OK: return "no error";
    case BuildError_E::NEED_NAME: return "need to named";
    case BuildError_E::NEED_HELP: return "need to set infomation of usage";
    case BuildError_E::DUPLICATE_NAME: return "name already exists";
    case BuildError_E::UNSUPPORT_TYPE: return "unsupported type";
    case BuildError_E::BAD_PREFIX: return "option name must be `-` or `--` prefixed";

    default: return "unknwon error";
    }
}

/* 构筑期错误域的自定义 error_category 单例 */
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

/* 错误码 → std::error_code / std::error_condition（注册进标准错误体系） */
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

/* 构筑期异常：携带 BuildError_E 错误码，what() 返回可读文本 */
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
