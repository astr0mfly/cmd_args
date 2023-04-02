#pragma once

#include <string>
#include <functional>
#include "cmd_args/base/def.h"

CMD_ARGS_NAMESPACE_BEGIN

struct option
{
    std::string short_name;
    std::string long_name;
    std::string help;
    std::string type;
    std::string value;

    option(std::string sname, std::string lname, std::string help, std::string type,
           std::string value)
        : short_name(std::move(sname))
        , long_name(std::move(lname))
        , help(std::move(help))
        , type(std::move(type))
        , value(std::move(value)) {}
};

struct short_circuit_option
{
    std::string short_name;
    std::string long_name;
    std::string help;
    std::function<void(void)> callback;

    short_circuit_option(std::string sname, std::string lname, std::string help, std::function<void(void)> callback)
        : short_name(std::move(sname))
        , long_name(std::move(lname))
        , help(std::move(help)),
        callback(std::move(callback))
    {}
};



CMD_ARGS_NAMESPACE_END