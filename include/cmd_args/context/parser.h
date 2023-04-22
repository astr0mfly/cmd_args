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

#include "cmd_args/argument/argument_parser.h"
#include "cmd_args/env/environment_parser.h"
#include "cmd_args/option/option_parser.h"

#include "model.h"
#include "parse_fwd.h"

CMD_ARGS_NAMESPACE_BEGIN

// 解析token,并进行分配，直到参数满足
class Parser : private argument_parser, private option_parser, private envirionment_parser
{
public:
    Parser(Model *_Data)
        : argument_parser(&_Data->m_args__)
        , option_parser()
        , envirionment_parser()
    {}

    Parser &set_program_name(Name_T name)
    {
        program_name = std::move(name);
        return *this;
    }

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
        // option_parser::parse(_Args);

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
        // oss << "\n" << description << "\n\n";
        // oss << "Options:\n";
        //// calculate the longest option name
        // auto max_name_length = option_parser::calc_desc_max_length();
        // option_parser::appendHelp(oss, max_name_length);
        // argument_parser::appendHelp(oss, max_name_length);

        return oss.str();
    }

    Name_T program_name;
    Desc_T description;
};

CMD_ARGS_NAMESPACE_END