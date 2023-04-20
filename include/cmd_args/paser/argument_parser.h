#pragma once

#include "cmd_args/argment/argument.h"

CMD_ARGS_NAMESPACE_BEGIN

class argument_parse
{
public:
    void parse(std::vector<std::string> &_Tokens)
    {
        if (_Tokens.size() < named_args.doSize()) {
            std::cerr << "(parse error) not enough named_arguments" << std::endl;
            std::exit(-1);
        }

        for (auto pos = _Tokens.begin(); pos != _Tokens.end();) {
            if (named_args.set(*pos)) {
                pos = _Tokens.erase(pos);
                break;
            }
            ++pos;
        }

        // start parse position arguments
        if (_Tokens.size() != pos_args.doSize()) {
            std::cerr << "(parse error) position argument number missmatching, give " << _Tokens.size() << ", but need "
                      << pos_args.doSize() << '\n';
            std::cerr << "uncaptured command line arguments:\n";
            for (auto const &tok : _Tokens) { std::cerr << tok << '\n'; }
            std::cerr << std::flush;
            std::exit(-1);
        }

        for (std::size_t i = 0; i < _Tokens.size(); ++i) { pos_args.set(i, _Tokens[i]); }
    }

    template <typename T>
    void check_add_argument_name(std::string const &key) const
    {
        using namespace util;
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
        if (pos_args.doFind(key) || named_args.doFind(key)) {
            std::cerr << "(build error) argument name " << key << " already exists" << std::endl;
            std::exit(-1);
        }
    }

    template <typename T>
    T get_argument(std::string const &name) const
    {
        using namespace util;
        auto *p = pos_args.doGet(name);
        if (p) {
            if (p->strType != util::type_string<T>()) {
                std::cerr << "(get error) argument type mismatch: set '" << p->strType << "' but you try get with '"
                          << util::type_string<T>() << "'" << std::endl;
                std::exit(-1);
            }

            return util::parse_value<T>(p->strValue);
        }

        p = named_args.doGet(name);
        if (!p) {
            std::cerr << "(get error) argument not found: " << name << std::endl;
            std::exit(-1);
        }

        if (p->strType != util::type_string<T>()) {
            std::cerr << "(get error) argument type mismatch: set '" << p->strType << "' but you try get with '"
                      << util::type_string<T>() << "'" << std::endl;
            std::exit(-1);
        }

        return util::parse_value<T>(p->strValue);
    }

    template <typename T>
    void add_argument(std::string name, std::string help)
    {
        check_add_argument_name<T>(name);
        pos_args.doPush(std::move(name), std::move(help), util::type_string<T>());
    }

    template <typename T>
    void add_named_argument(std::string name, std::string help)
    {
        check_add_argument_name<T>(name);
        named_args.doPush(std::move(name), std::move(help), util::type_string<T>());
    }

    // some alias for get_argument
    int         get_argument_int(std::string const &name) const { return get_argument<int>(name); }
    int64_t     get_argument_int64(std::string const &name) const { return get_argument<int64_t>(name); }
    double      get_argument_double(std::string const &name) const { return get_argument<double>(name); }
    std::string get_argument_string(std::string const &name) const { return get_argument<std::string>(name); }

    std::string usage_string() const
    {
        std::string res;
        res += named_args.doGetUsage();
        res += pos_args.doGetUsage();

        return res;
    }

    void appendHelp(std::ostringstream &_Oss, size_t &_Max) const
    {
        named_args.doGetHelp(_Oss, _Max);
        pos_args.doGetHelp(_Oss, _Max);
    }

private:
    named_arg_mgr    named_args;
    position_arg_mgr pos_args;
};

CMD_ARGS_NAMESPACE_END