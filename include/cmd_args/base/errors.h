#pragma once

#include <stack>
#include <system_error>
#include <vector>

#include "cmd_args/base/def.h"

CMD_ARGS_NAMESPACE_BEGIN

class Errors
{
public:
    using ErrCode_T = std::error_code;
    using ConEC_T   = std::stack<ErrCode_T, std::vector<ErrCode_T>>;
    Errors()        = default;

    // moveable
    Errors(Errors &&)            = default;
    Errors &operator=(Errors &&) = default;

    void setLast(ErrCode_T &&_V)
    {
        m_stkErros__.emplace(std::move(_V));
        m_ecDefault__ = m_stkErros__.top();
    }
    void setLast(ErrCode_T const &_V)
    {
        m_stkErros__.emplace(_V);
        m_ecDefault__ = m_stkErros__.top();
    }

    ErrCode_T const &getLast() const noexcept { return m_ecDefault__; }
    auto             getLastVal() const noexcept { return getLast().value(); }
    auto             getLastMsg() const noexcept { return getLast().message(); }

    void clearLast() noexcept
    {
        while (!m_stkErros__.empty()) m_stkErros__.pop();
        m_ecDefault__ = ErrCode_T{};
    }

    /*
        the default errc.value is uqual to 0
    */
    bool isOk() const noexcept { return m_ecDefault__.value() == 0; }

    /*
        raise when has error_code
    */
    void tryRaise() const
    {
        if (isOk()) return;
        throw m_ecDefault__;
    }

private:
    // nocopyable
    Errors(Errors const &)            = delete;
    Errors &operator=(Errors const &) = delete;

    ErrCode_T m_ecDefault__;
    ConEC_T   m_stkErros__;
};

CMD_ARGS_NAMESPACE_END