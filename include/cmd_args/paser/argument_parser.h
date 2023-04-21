#pragma once

#include <sstream>
#include <vector>

#include "cmd_args/base/errors.h"

#include "cmd_args/argment/argument.h"

#include "parse_fwd.h"

CMD_ARGS_NAMESPACE_BEGIN

class argument_parser
{
private:
public:
    argument_parser(Errors *_Obj)
        : m_pErrs__(_Obj)
    {}

    template <typename T>
    void add(std::string _Name, std::string _Help)
    {
        if constexpr (std::is_same_v<T, ArgumentNamed>) {
            named_args.doPush(std::make_unique<ArgumentNamed>(_Name, _Help));
        }
        else if constexpr (std::is_same_v<T, position_argument>) {
            pos_args.doPush(std::make_unique<position_argument>(pos_args.doSize(), _Name, _Help));
        }
        else {
            static_assert(true, "Failed to construct Argument::ptr.");
        }
    }

    void parse(Tokens_T &_Tokens)
    {
        __parse(
            std::bind(
                &argument_parser::__throwErr,
                this,
                std::placeholders::_1,
                std::placeholders::_2),
            _Tokens);
    }

    void parse(std::error_code &_Errc, Tokens_T &_Tokens) noexcept
    {
        __parse(
            std::bind(
                &argument_parser::__LastErr,
                this,
                std::placeholders::_1,
                std::placeholders::_2),
            _Tokens);

        _Errc = m_pErrs__->getLast();
    }

    template <typename T>
    void check_add_argument_name(std::string const &key) const
    {
        using namespace util;
        if (type_string<T>() == "null") {
            std::cerr << "(build error) argument type is not supported: " << typeid(T).name()
                      << std::endl;
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
            if (p->getType() != util::type_string<T>()) {
                std::cerr << "(get error) argument type mismatch: set '" << p->getType()
                          << "' but you try get with '" << util::type_string<T>() << "'"
                          << std::endl;
                std::exit(-1);
            }

            return util::parse_value<T>(p->getValue());
        }

        p = named_args.doGet(name);
        if (!p) {
            std::cerr << "(get error) argument not found: " << name << std::endl;
            std::exit(-1);
        }

        if (p->getType() != util::type_string<T>()) {
            std::cerr << "(get error) argument type mismatch: set '" << p->getType()
                      << "' but you try get with '" << util::type_string<T>() << "'" << std::endl;
            std::exit(-1);
        }

        return util::parse_value<T>(p->getValue());
    }

    // some alias for get_argument
    int     get_argument_int(std::string const &name) const { return get_argument<int>(name); }
    int64_t get_argument_int64(std::string const &name) const
    {
        return get_argument<int64_t>(name);
    }
    double get_argument_double(std::string const &name) const { return get_argument<double>(name); }
    std::string get_argument_string(std::string const &name) const
    {
        return get_argument<std::string>(name);
    }

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
    inline void __throwErr(ParseError_E _E, std::string _Msg)
    {
        if (!_Msg.empty()) { m_pErrs__->setMsg(std::move(_Msg)); }
        throw ErrorParse(_E);
    }

    inline void __LastErr(ParseError_E _E, std::string _Msg) noexcept
    {
        if (!_Msg.empty()) { m_pErrs__->setMsg(std::move(_Msg)); }
        m_pErrs__->setLast(_E);
    }

    void __parse(std::function<void(ParseError_E, std::string)> &&_ProcErr, Tokens_T &_Tokens)
    {
        if (_Tokens.size() < named_args.doSize()) { _ProcErr(ParseError_E::NOT_ENOUGH_ARGS, ""); }

        for (auto pos = _Tokens.begin(); pos != _Tokens.end();) {
            std::error_code ec;
            named_args.parse(ec, *pos);
            if (ec) {
                pos = _Tokens.erase(pos);
                break;
            }
            ++pos;
        }

        // start parse position arguments
        if (_Tokens.size() != pos_args.doSize()) {
            std::ostringstream oss;
            oss << "give " << _Tokens.size() << ", but need " << pos_args.doSize() << '\n';
            oss << "uncaptured command line arguments:\n";
            for (auto const &tok : _Tokens) { oss << tok << '\n'; }

            _ProcErr(ParseError_E::UNKNWON_ARGS, oss.str());
        }

        std::size_t i = 0;
        for (auto it{ _Tokens.begin() }; it != _Tokens.end(); ++i, ++it) { pos_args.parse(i, *it); }
    }

    named_arg_mgr    named_args;
    position_arg_mgr pos_args;
    Errors          *m_pErrs__ = nullptr;
};

CMD_ARGS_NAMESPACE_END