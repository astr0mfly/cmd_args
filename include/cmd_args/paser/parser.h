#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <typeinfo>
#include <vector>

#include "cmd_args/argment/argument.h"
#include "cmd_args/env/environment.h"
#include "cmd_args/option/option.h"

#include "argument_parser.h"
#include "environment_parse.h"
#include "option_parser.h"
#include "parse_fwd.h"

CMD_ARGS_NAMESPACE_BEGIN

class parser final :
    private argument_parser,
    private option_parser,
    private envirionment_parser,
    private Errors
{
public:
    parser(std::string description)
        : description(std::move(description))
        , argument_parser(this)
        , option_parser(this)
        , envirionment_parser(this)
    {}

    parser &set_program_name(Name_T name)
    {
        program_name = std::move(name);
        return *this;
    }
    template <typename T = option>
    void add(NameLong_T _Long, Desc_T _Desc)
    {
        if constexpr (std::is_base_of_v<Argument, T>) { argument_parser::add<T>(_Long, _Desc); }
        else {
            option_parser::add_option(' ', _Long, _Desc);
        }
    }
    void add(NameLong_T _Long, Desc_T _Desc, NameShort_T _Short)
    {
        option_parser::add_option(_Short, _Long, _Desc);
    }

    // add argument or option
    void add(NameLong_T _Long, Desc_T _Desc, NameShort_T _Short, bool _Required) {}

    // parse arguments
    void parse(int argc, char const *argv[])
    {
        Tokens_T vecArgs;
        for (int i = 0; i < argc; ++i) { vecArgs.emplace_back(argv[i]); }

        __parse(vecArgs.size(), vecArgs, {});
    }

    void parse(int argc, char const *argv[], char *envp[])
    {
        Tokens_T vecArgs;
        for (int i = 0; i < argc; ++i) { vecArgs.emplace_back(argv[i]); }

        std::vector<std::string> vecEnv;
        for (int i = 0; envp[i]; ++i) { vecEnv.emplace_back(envp[i]); }

        __parse(vecArgs.size(), vecArgs, vecEnv);
    }

    [[nodiscard]] std::error_code tryParse(int argc, char const *argv[]) noexcept
    {
        return make_error_code(ParseError_E::OK);
    }

    [[nodiscard]] std::error_code tryParse(int argc, char const *argv[], char *envp[]) noexcept
    {
        return make_error_code(ParseError_E::OK);
    }

    bool parsed(NameShort_T _Name) const { return false; }
    bool parsed(NameLong_T _Name) const { return false; }

    parser &add_help_option()
    {
        option_parser::add_sc_option("-?", "--help", "show this help message", [this]() {
            print_help();
        });

        return *this;
    }

    parser &add_env_option()
    {
        option_parser::add_sc_option("-!", "--env", "show this env message", [this]() {
            print_env();
        });

        return *this;
    }

    // be used in once mode and return last_val
    [[noreturn]] void once() const noexcept { exit(getLastVal()); }

    using Errors::dump;
    using Errors::getLast;
    using Errors::getLastMsg;
    using Errors::getLastVal;
    using Errors::isOk;

    void print_usage() const { std::cout << __descUsage() << std::endl; }
    void print_help() const { std::cout << __descHelp() << std::endl; }
    void print_env() const { std::cout << __descEnv() << std::endl; }

private:
    void __parse(size_t _CntArg, Tokens_T &_Args, std::vector<std::string> const &_Env)
    {
        if (program_name == "") {
            // if not set program name, use argv.front()
            program_name = _Args.front();
        }

        if (_CntArg == 1) {
            // if not input paramater, print usage
            print_usage();

            return;
        }

        envirionment_parser::parse(_Env);

        _Args.pop_front();
        argument_parser::parse(_Args);
        option_parser::parse(_Args);

        return;
    }

    std::string __descEnv() const
    {
        std::ostringstream oss;
        oss << "environments: " << program_name << "\nNo. : \"key\" = \"value\"\n";
        oss << env_string();
        return oss.str();
    }

    std::string __descUsage() const
    {
        std::ostringstream oss;
        oss << "usage: " << program_name << " [arguments]\n";
        oss << usage_string();
        return oss.str();
    }

    std::string __descHelp() const
    {
        std::ostringstream oss(usage_string(), std::ios::app);
        oss << "\n" << description << "\n\n";
        oss << "Options:\n";
        // calculate the longest option name
        auto max_name_length = option_parser::calc_desc_max_length();
        option_parser::appendHelp(oss, max_name_length);
        argument_parser::appendHelp(oss, max_name_length);

        return oss.str();
    }

    Name_T program_name;
    Desc_T description;
};

CMD_ARGS_NAMESPACE_END