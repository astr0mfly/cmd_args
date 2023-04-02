#pragma once

#include <string>
#include <vector>
#include "cmd_args/base/def.h"

CMD_ARGS_NAMESPACE_BEGIN

struct argument
{
    std::string name;
    std::string help;
    std::string type;
    std::string value;

    argument(std::string name, std::string help, std::string type)
        : name(std::move(name))
        , help(std::move(help))
        , type(std::move(type))
    {}
};

using arguments_t = std::vector<argument>;

CMD_ARGS_NAMESPACE_END