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
 * 执行期错误域（InterpError_E）：once()/loop() 解释阶段的状态与错误码。
 * 典型场景：未解析先执行（STOPPED——once() 在 parse 之前调用）、
 * 请求退出（EXIT_REQUESTED / END_OF_INPUT）、未知命令（UNKNOWN_COMMAND，loop 预留）。
 * 三件套惯例与其它错误域一致（枚举 → 映射 → category → make_error_code 特化）。
 */
#include <cstring>
#include <string>
#include <system_error>

#include "cmd_args/base/def.h"

CMD_ARGS_NAMESPACE_BEGIN

/* 执行错误码：OK=0 表示无错 */
enum class InterpError_E
{
    OK = 0,
    EXIT_REQUESTED,  // 用户请求退出（loop 的 exit/quit）
    STOPPED,         // 解释器未就绪（如 once 前未 parse）
    UNKNOWN_COMMAND, // 无法识别的命令（loop 预留）
    END_OF_INPUT     // 输入流结束（EOF）
};

/* 错误码 → 人类可读文本 */
static inline char const *mapInterpError(int _ErrCode) noexcept
{
    switch (static_cast<InterpError_E>(_ErrCode)) {
    case InterpError_E::OK: return "no error";
    case InterpError_E::EXIT_REQUESTED: return "exit requested";
    case InterpError_E::STOPPED: return "interpreter stopped";
    case InterpError_E::UNKNOWN_COMMAND: return "unknown command";
    case InterpError_E::END_OF_INPUT: return "end of input";
    default: return "unknwon error";
    }
}

class InterpErrorCategory : public std::error_category
{
public:
    InterpErrorCategory()
        : error_category()
    {}
    const char *name() const noexcept override { return ENUM_NAME(InterpError_E); }
    std::string message(int _V) const override { return mapInterpError(_V); }

    static InterpErrorCategory const &instance()
    {
        static InterpErrorCategory instance;
        return instance;
    }
};

std::error_code make_error_code(InterpError_E code)
{
    return {
        static_cast<int>(code),
        InterpErrorCategory::instance(),
    };
}

std::error_condition make_error_condition(InterpError_E code)
{
    return {
        static_cast<int>(code),
        InterpErrorCategory::instance(),
    };
}

class ErrorInterp : public std::logic_error
{
public:
    explicit ErrorInterp(std::error_code _Ec)
        : logic_error("")
        , m_ec__(_Ec)
    {}
    explicit ErrorInterp(InterpError_E _E)
        : logic_error("")
        , m_ec__(make_error_code(_E))
    {}

    std::error_code const &code() const noexcept { return m_ec__; }
    char const            *what() const noexcept override
    {
        auto const *p = logic_error::what();
        if (p && std::strlen(p)) { return p; }

        return mapInterpError(m_ec__.value());
    }

private:
    std::error_code m_ec__;
};

CMD_ARGS_NAMESPACE_END

namespace std {
template <>
struct is_error_code_enum<cmd_args::InterpError_E> : true_type
{};
}  // namespace std