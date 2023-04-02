#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <typeinfo>
#include <map>
#include <vector>

#include "cmd_args/base/def.h"
#include "cmd_args/base/util.h"
#include "cmd_args/option/option.h"
#include "cmd_args/argment/argument.h"
#include "cmd_args/env/environment.h"
#include "argument_parser.h"
#include "environment_parse.h"
#include "option_parser.h"

CMD_ARGS_NAMESPACE_BEGIN

class parser {
public:
    parser(std::string description)
        : description(std::move(description)) {}

    parser& set_program_name(std::string name) {
        program_name = std::move(name);
        return *this;
    }

    parser& add_help_option() {
        m_options__.add_sc_option("-?", "--help", "show this help message",
                                  [this]() { print_help(); });

        return *this;
    }

    parser& add_env_option() {
        m_options__.add_sc_option("-!", "--env", "show this env message",
                                  [this]() { print_env(); });

        return *this;
    }

    // parse arguments
    parser& parse(int argc, char const* argv[]) {
        std::vector<std::string> vecArgs;
        for (int i = 0; i < argc; ++i) { vecArgs.emplace_back(argv[i]); }

        parse(vecArgs.size(), std::move(vecArgs), {});
    }

    parser& parse(int argc, char const* argv[], char* envp[]) {
        std::vector<std::string> vecArgs;
        for (int i = 0; i < argc; ++i) { vecArgs.emplace_back(argv[i]); }

        std::vector<std::string> vecEnv;
        for (int i = 0; envp[i]; ++i) { vecEnv.emplace_back(envp[i]); }

        parse(vecArgs.size(), std::move(vecArgs), std::move(vecEnv));
    }

    parser& parse(size_t argc, std::vector<std::string>&& argv, std::vector<std::string>&& envp) {
        parse(argc, argv, envp);
    }

    parser& parse(size_t argc, std::vector<std::string>& argv, std::vector<std::string>& envp) {
        if (program_name == "") {
            // if not set program name, use argv[0]
            program_name = argv[0];
        }

        if (argc == 1) {
            print_usage();
            std::exit(0);
        }

        m_environments__.parse(envp);

        std::vector<std::string> tokens;
        for (int i = 1; i < argc; ++i) { tokens.emplace_back(argv[i]); }
        m_options__.parse(tokens);
        m_arguments__.parse(tokens);
        return *this;
    }

    void print_env() const {
        std::ostringstream oss;
        oss << "usage: " << program_name << " [environments]";
        oss << m_environments__.env_string();

        std::cout << oss.str() << std::endl;
    }

    void print_usage() const {
        std::ostringstream oss;
        oss << "usage: " << program_name << " [arguments]";
        oss << m_arguments__.usage_string();

        std::cout << oss.str() << std::endl;
    }

    std::string help_desc() const {
        std::ostringstream oss(m_arguments__.usage_string(), std::ios::app);
        oss << "\n" << description << "\n\n";
        oss << "Options:\n";
        // calculate the longest option name
        auto max_name_length = m_options__.calc_desc_max_length();
        m_options__.appendHelp(oss, max_name_length);
        m_arguments__.appendHelp(oss, max_name_length);

        return oss.str();
    }

    void print_help() const {
        std::cout << help_desc() << std::endl;
    }

private:
    std::string program_name;
    std::string description;

    argument_parse m_arguments__;
    option_parser m_options__;
    envirionment_parser m_environments__;
};

CMD_ARGS_NAMESPACE_END