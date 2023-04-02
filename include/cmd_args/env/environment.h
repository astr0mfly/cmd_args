#pragma once

#include <set>
#include "cmd_args/base/def.h"

CMD_ARGS_NAMESPACE_BEGIN

struct environment
{
    std::string strKey;
    std::string strValue;

    environment(std::string const& _Env)
    {
        auto p = _Env.find_first_of('_');
        if (p == std::string::npos) return;
        strKey = _Env.substr(0, p++);
        strValue = _Env.substr(p, _Env.size() - p);
    }

    bool operator<(environment const& _Rhs) const noexcept
    {
        return strKey < _Rhs.strKey;
    }
};

using envs_t = std::set<environment>;

CMD_ARGS_NAMESPACE_END