#pragma once

#include <set>
#include "cmd_args/base/def.h"
#include "cmd_args/base/util.h"

CMD_ARGS_NAMESPACE_BEGIN

struct environment {
    std::string strKey;
    mutable std::string strValue;

    environment(std::string const& _Env) {
        auto p = util::parse_line(_Env);
        strKey = std::move(p.first);
        strValue = std::move(p.second);
    }

    bool operator<(environment const& _Rhs) const noexcept {
        return strKey < _Rhs.strKey;
    }
};

using envs_t = std::set<environment>;

CMD_ARGS_NAMESPACE_END