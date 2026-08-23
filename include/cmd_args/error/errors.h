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

/*
    ErrorGeneral：携带 std::error_code 的通用运行时异常，
    供 Errors::raise / tryRaise 使用（what() 即 ec.message()）。
*/
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

/*
    Errors：Context 的错误堆栈（双通道）：
    - setLast(ec)    记录最近一次错误码（并压入消息堆栈）
    - setMsg(msg)    只压栈消息（warning 类提示，如 YAML 未知键）
    - getLast/getLastMsg/getLastVal  读取最近错误码
    - dump()         按序输出整条消息栈（倒序，最新在前）
    - isOk()         最近错误码为 0 且消息栈为空
    - tryRaise/raise 按需转异常
    Context 私有继承本类并把 dump/getLast 等公开给用户。
*/
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

    /* 记录最近错误码（消息文本同时入栈） */
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

    /* 追加一条普通消息（无错误码），用于 warning 类提示 */
    void setMsg(std::string &&_Desc) { m_stkErros__.emplace_back(std::move(_Desc)); }
    void setMsg(std::string const &_Desc) { m_stkErros__.emplace_back(_Desc); }

    ErrCode_T const &getLast() const noexcept { return m_ecDefault__; }
    auto             getLastVal() const noexcept { return getLast().value(); }
    auto             getLastMsg() const noexcept { return getLast().message(); }

    /* 清空消息栈与最近错误码（loop 每次调用前调用，保证行间隔离） */
    void clearLast() noexcept
    {
        while (!m_stkErros__.empty()) m_stkErros__.clear();
        m_ecDefault__ = ErrCode_T{};
    }

    /*
        是否无错：最近错误码 value()==0 且消息栈为空。
    */
    [[nodiscard]] inline bool isOk() const noexcept;

    /*
        有错时抛出 ErrorGeneral（无错则静默返回）。
    */
    void tryRaise() const
    {
        if (isOk()) return;
        throw ErrorGeneral(m_ecDefault__);
    }

    /* 记录错误码并抛出 ErrorGeneral（先记录后抛，错误堆栈保留现场） */
    void raise(ErrCode_T _Ec)
    {
        setLast(_Ec);
        throw ErrorGeneral(m_ecDefault__);
    }

    /* 消息栈 dump（最新在前的倒序文本，形如 { n : msg ... }） */
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