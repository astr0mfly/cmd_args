/*
 * Copyright [2023] [ValenciaFly]
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
#include <vector>

#include "cmd_args/base/def.h"

CMD_ARGS_NAMESPACE_BEGIN

class ErrorGeneral : public std::runtime_error
{
public:
    explicit ErrorGeneral(std::error_code _Ec)
        : runtime_error("")
        , m_ec__(_Ec)
    {}

    std::error_code const &code() const noexcept { return m_ec__; }
    char const            *what() const noexcept override { return m_ec__.message().c_str(); }

private:
    std::error_code m_ec__;
};

class Errors
{
public:
    using ErrCode_T = std::error_code;
    using ConDesc_T = std::vector<std::string>;
    using Desc_T    = std::string;

    Errors() = default;

    // moveable
    Errors(Errors &&)            = default;
    Errors &operator=(Errors &&) = default;

    void setLast(ErrCode_T &&_V)
    {
        m_stkErros__.emplace_back(_V.message());
        m_ecDefault__ = std::move(_V);
    }
    void setLast(ErrCode_T const &_V)
    {
        m_stkErros__.emplace_back(_V.message());
        m_ecDefault__ = _V;
    }

    void setMsg(std::string &&_Desc) { m_stkErros__.emplace_back(std::move(_Desc)); }
    void setMsg(std::string const &_Desc) { m_stkErros__.emplace_back(_Desc); }

    ErrCode_T const &getLast() const noexcept { return m_ecDefault__; }
    auto             getLastVal() const noexcept { return getLast().value(); }
    auto             getLastMsg() const noexcept { return getLast().message(); }

    void clearLast() noexcept
    {
        while (!m_stkErros__.empty()) m_stkErros__.clear();
        m_ecDefault__ = ErrCode_T{};
    }

    /*
        the default errc.value is uqual to 0
    */
    [[nodiscard]] inline bool isOk() const noexcept;

    /*
        raise when has error_code
    */
    void tryRaise() const
    {
        if (isOk()) return;
        throw ErrorGeneral(m_ecDefault__);
    }

    void raise(ErrCode_T _Ec)
    {
        setLast(_Ec);
        throw ErrorGeneral(m_ecDefault__);
    }

    Desc_T dump() const
    {
        Desc_T r;
        r += "{\n";
        size_t i = 0;
        for (auto rit{ m_stkErros__.rbegin() }; rit != m_stkErros__.rend(); ++rit) {
            r += std::to_string(i++);
            r += " : ";
            r += *rit;
            r += '\n';
        }
        r += "}\n";

        return r;
    }

private:
    // nocopyable
    Errors(Errors const &)            = delete;
    Errors &operator=(Errors const &) = delete;

    ErrCode_T m_ecDefault__;
    ConDesc_T m_stkErros__;
};

bool Errors::isOk() const noexcept { return (m_ecDefault__.value() == 0) && m_stkErros__.empty(); }

CMD_ARGS_NAMESPACE_END