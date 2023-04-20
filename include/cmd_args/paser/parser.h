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

#include "cmd_args/base/def.h"
#include "cmd_args/base/errors.h"
#include "cmd_args/base/util.h"

#include "cmd_args/argment/argument.h"
#include "cmd_args/env/environment.h"
#include "cmd_args/option/option.h"

#include "argument_parser.h"
#include "environment_parse.h"
#include "option_parser.h"

CMD_ARGS_NAMESPACE_BEGIN

class parser final : argument_parser, option_parser, envirionment_parser, Errors

{
public:
    parser(std::string description)
        : description(std::move(description))
        , argument_parser(this)
        , option_parser(this)
        , envirionment_parser(this)
        , Errors()
    {}

    parser &set_program_name(std::string name)
    {
        program_name = std::move(name);
        return *this;
    }

    parser &add_help_option()
    {
        option_parser::add_sc_option("-?", "--help", "show this help message", [this]() { print_help(); });

        return *this;
    }

    parser &add_env_option()
    {
        option_parser::add_sc_option("-!", "--env", "show this env message", [this]() { print_env(); });

        return *this;
    }

    // parse arguments
    parser &parse(int argc, char const *argv[])
    {
        std::vector<std::string> vecArgs;
        for (int i = 0; i < argc; ++i) { vecArgs.emplace_back(argv[i]); }

        parse(vecArgs.size(), std::move(vecArgs), {});

        return *this;
    }

    parser &parse(int argc, char const *argv[], char *envp[])
    {
        std::vector<std::string> vecArgs;
        for (int i = 0; i < argc; ++i) { vecArgs.emplace_back(argv[i]); }

        std::vector<std::string> vecEnv;
        for (int i = 0; envp[i]; ++i) { vecEnv.emplace_back(envp[i]); }

        parse(vecArgs.size(), std::move(vecArgs), std::move(vecEnv));

        return *this;
    }

    parser &parse(size_t argc, std::vector<std::string> &&argv, std::vector<std::string> &&envp)
    {
        parse(argc, argv, envp);

        return *this;
    }

    parser &parse(size_t argc, std::vector<std::string> &argv, std::vector<std::string> &envp)
    {
        if (program_name == "") {
            // if not set program name, use argv[0]
            program_name = argv[0];
        }

        if (argc == 1) {
            print_usage();
            std::exit(0);
        }

        envirionment_parser::parse(envp);

        std::vector<std::string> tokens;
        for (decltype(argc) i = 1; i < argc; ++i) { tokens.emplace_back(argv[i]); }
        option_parser::parse(tokens);
        argument_parser::parse(tokens);

        return *this;
    }

    bool exist(char _Name) const { return false; }

    void add(std::string _Long, char _Short, std::string _Desc) { option_parser::add_option(_Short, _Long, _Desc); }

    void print_env() const { std::cout << __descEnv() << std::endl; }

    void print_usage() const { std::cout << __descUsage() << std::endl; }

    void print_help() const { std::cout << __descHelp() << std::endl; }

private:
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

    std::string program_name;
    std::string description;
};

CMD_ARGS_NAMESPACE_END