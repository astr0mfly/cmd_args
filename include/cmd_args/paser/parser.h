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
#include <unordered_map>
#include <vector>

#include "cmd_args/base/def.h"
#include "cmd_args/base/util.h"
#include "cmd_args/option/option.h"
#include "cmd_args/argment/argument.h"
#include "cmd_args/env/environment.h"

CMD_ARGS_NAMESPACE_BEGIN

class argparser
{
public:
    argparser(std::string description) : description(std::move(description)) {}

    argparser &set_program_name(std::string name)
    {
        program_name = std::move(name);
        return *this;
    }

    std::string usage_string() const
    {
        std::ostringstream oss;
        oss << "usage: " << program_name << " [options]";
        for (auto const &named_arg : named_arguments) {
            std::cout << " [=" << named_arg.name << "]";
        }

        for (auto const &arg : arguments) {
            std::cout << " [" << arg.name << "]";
        }
        
        return oss.str();
    }

    void print_usage() const
    {
        std::cout << usage_string() << std::endl;
    }

    std::string env_string() const
    {
        std::ostringstream oss;
        size_t i = 0;
        for (auto& e : setEnvs) {
            oss << i << " : " << e.strKey << " = " << e.strValue << std::endl;
            ++i;
        }

        return oss.str();
    }

    void print_env() const {
        std::cout << env_string() << std::endl;
    }

    std::size_t calc_desc_max_length() const noexcept
    {
        std::size_t max_name_length = 0;
        for (auto const &opt : short_circuit_options) {
            std::size_t length = opt.long_name.length();
            if (!opt.short_name.empty())
            {
                length += 4;
            }
            max_name_length = std::max(max_name_length, length);
        }

        for (auto const &opt : options) {
            std::size_t length = opt.long_name.length();
            if (!opt.short_name.empty())
            {
                length += 4;
            }
            max_name_length = std::max(max_name_length, length);
        }

        max_name_length = std::max(max_name_length, std::size_t(25));

        return max_name_length;
    }

    void short_circuit_options_help(std::ostringstream &_Out, std::size_t _Max) const
    {
        for (auto const &opt : short_circuit_options) {
            _Out << "  ";
            std::size_t printed_length = 0;
            if (!opt.short_name.empty())
            {
                _Out << opt.short_name << ", ";
                printed_length = 4;
            }
            _Out << opt.long_name;
            printed_length += opt.long_name.length();
            _Out << std::string(_Max - printed_length, ' ');
            _Out << replace(opt.help, "\n", "\n" + std::string(_Max + 2, ' ')) << '\n';
        }
    }

    void options_help(std::ostringstream &_Out, std::size_t _Max) const
    {
        for (auto const &opt : options) {
            _Out << "  ";
            std::size_t printed_length = 0;
            if (!opt.short_name.empty()) {
                _Out << opt.short_name << ", ";
                printed_length = 4;
            }
            _Out << opt.long_name;
            printed_length += opt.long_name.length();
            _Out << std::string(_Max - printed_length, ' ');
            if (opt.type != "bool") {
                _Out << "(" << opt.type << ") ";
            }
            _Out << replace(opt.help, "\n", "\n" + std::string(_Max + 2, ' ')) << '\n';
        }
    }

    void named_arguments_help(std::ostringstream &_Out, std::size_t _Max) const
    {
        if (named_arguments.size() > 0) {
            _Out << "\nNamed arguments:\n";
            _Max = 0;
            for (auto const &arg : named_arguments) {
                _Max = std::max(_Max, arg.name.length());
            }

            _Max = std::max(_Max, std::size_t(25));
            for (auto const &arg : named_arguments) {
                _Out << "  ";
                _Out << arg.name;
                _Out << std::string(_Max - arg.name.length(), ' ') << "(" << arg.type << ") ";
                _Out << replace(arg.help, "\n", "\n" + std::string(_Max + 2, ' ')) << '\n';
            }
        }
    }

   void arguments_help(std::ostringstream &_Out, std::size_t _Max) const
    {
        if (arguments.size() > 0) {
            _Out << "\nPosition arguments:\n";
            _Max = 0;
            for (auto const &arg : arguments)
            {
                _Max = std::max(_Max, arg.name.length());
            }
            _Max = std::max(_Max, std::size_t(25));
            for (auto const &arg : arguments)
            {
                _Out << "  ";
                _Out << arg.name;
                _Out << std::string(_Max - arg.name.length(), ' ') << "(" << arg.type << ") ";
                _Out << replace(arg.help, "\n", "\n" + std::string(_Max + 2, ' ')) << '\n';
            }
        }
    }

    std::string help_desc() const
    {
        std::ostringstream oss(usage_string(), std::ios::app);
        oss << "\n" << description << "\n\n";
        oss << "Options:\n";
        // calculate the longest option name
        auto max_name_length = calc_desc_max_length();
        short_circuit_options_help(oss, max_name_length);
        options_help(oss, max_name_length);
        named_arguments_help(oss, max_name_length);
        arguments_help(oss, max_name_length);
 
        return oss.str();
    }

    void print_help() const
    {
        std::cout << help_desc() << std::endl;
    }

    argparser &add_help_option()
    {
        return add_sc_option("-?", "--help", "show this help message", [this]() { print_help(); });
    }

    argparser& add_env_option() {
        return add_sc_option("-!", "--env", "show this env message", [this]() { print_env(); });
    }

    // add short circuit option
    argparser &add_sc_option(std::string sname, std::string lname, std::string desc, std::function<void(void)> callback)
    {
        // long name must not be empty
        check_add_option_lname(lname);
        // allow short name to be empty
        if (sname != "") {
            check_add_option_sname(sname);
            short_name_index[sname.back()] = short_circuit_options.size();
        }
        short_circuit_options.emplace_back(std::move(sname), std::move(lname), std::move(desc), std::move(callback));
        
        return *this;
    }

    template <typename T>
    argparser &add_option(std::string sname, std::string lname, std::string help, T &&default_value)
    {
        if (type_string<T>() == "null") {
            std::cerr << "(build error) unsupport type for option: " << typeid(T).name() << std::endl;
            std::exit(-1);
        }

        check_add_option_lname(lname);
        if (sname != "") {
            check_add_option_sname(sname);
            short_name_index[sname.back()] = options.size();
        }

        options.emplace_back(
            std::move(sname),
            std::move(lname),
            std::move(help),
            type_string<T>(),
            to_string(default_value)
        );

        return *this;
    }

    argparser &add_option(std::string sname, std::string lname, std::string help)
    {
        check_add_option_lname(lname);
        if (sname != "")
        {
            check_add_option_sname(sname);
            short_name_index[sname.back()] = options.size();
        }
        options.emplace_back(std::move(sname), std::move(lname), std::move(help), "bool", "0");
        return *this;
    }

    template <typename T>
    argparser &add_argument(std::string name, std::string help)
    {
        check_add_argument_name<T>(name);
        arguments.emplace_back(std::move(name), std::move(help), type_string<T>());
        return *this;
    }

    template <typename T>
    argparser &add_named_argument(std::string name, std::string help)
    {
        check_add_argument_name<T>(name);
        named_arguments.emplace_back(std::move(name), std::move(help), type_string<T>());
        return *this;
    }

    template <typename T>
    T get_option(std::string const &name) const
    {
        using namespace util;
        auto pos = find_option_sname(name);
        if (pos == options.cend()) {
            pos = find_option_lname(name);
        }

        if (pos == options.cend()) {
            std::cerr << "(get error) option not found: " << name << std::endl;
            std::exit(-1);
        }

        if (pos->type != type_string<T>()) {
            std::cerr << "(get error) option type mismatch: set '" << pos->type << "' but you try get with '"
                      << type_string<T>() << "'" << std::endl;
            std::exit(-1);
        }

        return parse_value<T>(pos->value);
    }

    // some alias for get_option
    bool has_option(std::string const &name) const { return get_option<bool>(name); }
    bool get_option_bool(std::string const &name) const { return get_option<bool>(name); }
    int get_option_int(std::string const &name) const { return get_option<int>(name); }
    int64_t get_option_int64(std::string const &name) const { return get_option<int64_t>(name); }
    double get_option_double(std::string const &name) const { return get_option<double>(name); }
    std::string get_option_string(std::string const &name) const { return get_option<std::string>(name); }

    template <typename T>
    T get_argument(std::string const &name) const
    {
        using namespace util;
        auto pos = find_argument(name);
        if (pos == arguments.cend()) {
            pos = find_named_argument(name);
        }

        if (pos == named_arguments.cend()) {
            std::cerr << "(get error) argument not found: " << name << std::endl;
            std::exit(-1);
        }
        
        if (pos->type != type_string<T>()) {
            std::cerr << "(get error) argument type mismatch: set '" << pos->type << "' but you try get with '"
                      << type_string<T>() << "'" << std::endl;
            std::exit(-1);
        }

        return parse_value<T>(pos->value);
    }

    // some alias for get_argument
    int get_argument_int(std::string const &name) const { return get_argument<int>(name); }
    int64_t get_argument_int64(std::string const &name) const { return get_argument<int64_t>(name); }
    double get_argument_double(std::string const &name) const { return get_argument<double>(name); }
    std::string get_argument_string(std::string const &name) const { return get_argument<std::string>(name); }

    // parse arguments
    argparser& parse(int argc, char const* argv[])
    {
        std::vector<std::string> vecArgs;
        for (int i = 0; i < argc; ++i) {
            vecArgs.emplace_back(argv[i]);
        }

        parse(vecArgs.size(), std::move(vecArgs), {});
    }

    argparser& parse(int argc, char const* argv[], char*envp[])
    {
        std::vector<std::string> vecArgs;
        for (int i = 0; i < argc; ++i) {
            vecArgs.emplace_back(argv[i]);
        }

        std::vector<std::string> vecEnv;
        for (int i = 0; envp[i]; ++i) {
            vecEnv.emplace_back(envp[i]);
        }

        parse(vecArgs.size(), std::move(vecArgs), std::move(vecEnv));
    }

    argparser& parse(size_t argc, std::vector<std::string> &&argv, std::vector<std::string>  &&envp)
    {
        parse(argc, argv, envp);
    }

    argparser &parse(size_t argc, std::vector<std::string> &argv, std::vector<std::string> &envp)
    {
        if (program_name == "") {
            // if not set program name, use argv[0]
            program_name = argv[0];
        }
        if (argc == 1) {
            print_usage();
            std::exit(0);
        }

        // kv supported
        for (auto& e : envp) setEnvs.emplace(e);
        
        std::vector<std::string> tokens;
        for (int i = 1; i < argc; ++i) {
            tokens.emplace_back(argv[i]);
        }
        // start parse short circuit options
        for (auto &&sc_opt : short_circuit_options) {
            auto pos = std::find_if(tokens.cbegin(), tokens.cend(),
                                    [&sc_opt](std::string const &tok)
                                    { return tok == sc_opt.short_name || tok == sc_opt.long_name; });
            if (pos != tokens.cend())
            {
                sc_opt.callback();
                std::exit(0);
            }
        }
        // start parse options
        for (auto &&opt : options) {
            auto pos =
                std::find_if(tokens.cbegin(), tokens.cend(),
                             [&opt](std::string const &tok) { return tok == opt.short_name || tok == opt.long_name; }
                );
            if (pos == tokens.cend()) {
                continue;
            }
            pos = tokens.erase(pos);
            if (opt.type == "bool") {
                opt.value = "1";
            }
            else {
                // other types need to parse next token
                if (pos == tokens.cend())
                {
                    std::cerr << "(parse error) option " << opt.short_name << " " << opt.long_name
                              << " should have value" << std::endl;
                    std::exit(-1);
                }
                opt.value = *pos;
                pos = tokens.erase(pos);
            }
        }
        // if there are short name like options, parse it as aggregation short name options
        {
            auto pos =
                std::find_if(tokens.cbegin(), tokens.cend(), [](std::string const &tok) { return tok.front() == '-'; });
            if (pos != tokens.cend()) {
                if (pos->length() == 1) {
                    std::cerr << "(parse error) bare unexcepted '-'" << std::endl;
                    std::exit(-1);
                }
                if ((*pos)[1] == '-') {
                    std::cerr << "(parse error) unrecognized option" << (*pos) << std::endl;
                    std::exit(-1);
                }
                std::string short_names = pos->substr(1);
                for (char ch : short_names) {
                    std::size_t index = short_name_index[ch];
                    if (index < short_circuit_options.size() && short_circuit_options[index].short_name.back() == ch)
                    {
                        short_circuit_options[index].callback();
                        std::exit(0);
                    }
                }

                for (char ch : short_names) {
                    std::size_t index = short_name_index[ch];
                    if (index < options.size() && options[index].short_name.back() == ch) {
                        if (options[index].type != "bool")
                        {
                            std::cerr << "(parse error) aggregation short name option must be bool" << std::endl;
                            std::exit(-1);
                        }
                        options[index].value = "1";
                    }
                    else {
                        std::cerr << "(parse error) unrecognized short name option '" << ch << "' in " << (*pos)
                                  << std::endl;
                        std::exit(-1);
                    }
                }
                pos = tokens.erase(pos);
            }
        }
        // start parse named arguments
        if (tokens.size() < named_arguments.size()) {
            std::cerr << "(parse error) not enough named_arguments" << std::endl;
            std::exit(-1);
        }
        for (auto &named_arg : named_arguments) {
            for (auto pos = tokens.begin(); pos != tokens.end();) {
                if (try_parse_named_argument(*pos, named_arg))
                {
                    pos = tokens.erase(pos);
                    break;
                }
                ++pos;
            }
            if (named_arg.value == "") {
                std::cerr << "(parse error) named_argument " << named_arg.name << " should have value" << std::endl;
                std::exit(-1);
            }
        }

        // start parse position arguments
        if (tokens.size() != arguments.size()) {
            std::cerr << "(parse error) position argument number missmatching, give " << tokens.size() << ", but need "
                      << arguments.size() << '\n';
            std::cerr << "uncaptured command line arguments:\n";
            for (auto const &tok : tokens)
            {
                std::cerr << tok << '\n';
            }
            std::cerr << std::flush;
            std::exit(-1);
        }

        for (std::size_t i = 0; i < tokens.size(); ++i) {
            arguments[i].value = tokens[i];
        }

        return *this;
    }

private:
    bool try_parse_named_argument(std::string const &line, argument &named_arg)
    {
        auto pos = line.find('=');
        if (pos == std::string::npos) {
            return false;
        }
        auto name = line.substr(0, pos);
        auto value = line.substr(pos + 1);
        if (name != named_arg.name)
        {
            return false;
        }
        else
        {
            named_arg.value = value;
            return true;
        }
    }

    std::string replace(std::string const &str, std::string const &from, std::string const &to) const
    {
        std::string ret;
        std::size_t pos = 0, pre_pos = 0;
        while ((pos = str.find(from, pre_pos)) != std::string::npos)
        {
            ret += str.substr(pre_pos, pos - pre_pos) + to;
            pre_pos = pos + from.length();
        }
        ret += str.substr(pre_pos);
        return ret;
    }

    using argument_iterator = std::vector<argument>::const_iterator;
    using option_iterator = std::vector<option>::const_iterator;
    using sc_option_iterator = std::vector<short_circuit_option>::const_iterator;

    auto find_argument(std::string const &key) const -> argument_iterator
    {
        return std::find_if(arguments.cbegin(), arguments.cend(),
                            [&key](const argument &arg) { return arg.name == key; });
    }

    auto find_named_argument(std::string const &key) const -> argument_iterator
    {
        return std::find_if(named_arguments.cbegin(), named_arguments.cend(),
                            [&key](const argument &arg) { return arg.name == key; });
    }

    auto find_sc_option_sname(std::string const &key) const -> sc_option_iterator
    {
        return std::find_if(short_circuit_options.cbegin(), short_circuit_options.cend(),
                            [&key](const short_circuit_option &opt) { return opt.short_name == key; });
    }

    auto find_sc_option_lname(std::string const &key) const -> sc_option_iterator
    {
        return std::find_if(short_circuit_options.cbegin(), short_circuit_options.cend(),
                            [&key](const short_circuit_option &opt) { return opt.long_name == key; });
    }

    auto find_option_sname(std::string const &key) const -> option_iterator
    {
        return std::find_if(options.cbegin(), options.cend(),
                            [&key](const option &opt) { return opt.short_name == key; });
    }

    auto find_option_lname(std::string const &key) const -> option_iterator
    {
        return std::find_if(options.cbegin(), options.cend(),
                            [&key](const option &opt) { return opt.long_name == key; });
    }

    void check_add_option_sname(std::string const &key) const
    {
        if (key.size() != 2 || key.front() != '-') {
            std::cerr << "(build error) short option name must be `-` followed by one character" << std::endl;
            std::exit(-1);
        }
        char ch = key.back();
        if (short_name_index.find(ch) != short_name_index.end()) {
            std::cerr << "(build error) short option name " << key << " already exists" << std::endl;
            std::exit(-1);
        }
    }

    void check_add_option_lname(std::string const &key) const
    {
        if (key == "") {
            std::cerr << "(build error) long option name cannot be empty" << std::endl;
            std::exit(-1);
        }
        if (key.substr(0, 2) != "--") {
            std::cerr << "(build error) long option name must be `--` followed by one or more characters" << std::endl;
            std::exit(-1);
        }
        if (find_option_lname(key) != options.cend() || find_sc_option_lname(key) != short_circuit_options.cend()) {
            std::cerr << "(build error) long option name " << key << " already exists" << std::endl;
            std::exit(-1);
        }
    }

    template <typename T>
    void check_add_argument_name(std::string const &key) const
    {
        if (type_string<T>() == "null") {
            std::cerr << "(build error) argument type is not supported: " << typeid(T).name() << std::endl;
            std::exit(-1);
        }
        if (type_string<T>() == "bool") {
            std::cerr << "(build error) argument type cannot be bool" << std::endl;
            std::exit(-1);
        }
        if (key == "") {
            std::cerr << "(build error) argument name cannot be empty" << std::endl;
            std::exit(-1);
        }
        if (find_argument(key) != arguments.cend() || find_named_argument(key) != named_arguments.cend()) {
            std::cerr << "(build error) argument name " << key << " already exists" << std::endl;
            std::exit(-1);
        }
    }

private:
    std::string                             description;
    std::string                             program_name;
    std::vector<short_circuit_option>       short_circuit_options;
    std::vector<option>                     options;
    std::unordered_map<char, std::size_t>   short_name_index;
    arguments_t named_arguments;
    arguments_t arguments;
    envs_t setEnvs;
};

CMD_ARGS_NAMESPACE_END