#pragma once

#include "cmd_args/env/environment.h"

CMD_ARGS_NAMESPACE_BEGIN

class envirionment_parser {
public:
    void parse(std::vector<std::string>& _Envp) {
        for (auto& e : _Envp) m_setEnvs__.emplace(e);
    }
    std::string env_string() const {
        std::ostringstream oss;
        size_t i = 0;
        for (auto& e : m_setEnvs__) {
            oss << i << " : " << e.strKey << " = " << e.strValue << std::endl;
            ++i;
        }

        return oss.str();
    }

    void set(std::string _Key, std::string _Value) {
        auto it = m_setEnvs__.find(_Key);
        if (it == m_setEnvs__.end()) return;
        it->strValue = _Value;
    }

private:
    envs_t m_setEnvs__;
};

CMD_ARGS_NAMESPACE_END