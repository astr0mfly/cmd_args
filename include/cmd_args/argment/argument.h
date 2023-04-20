#pragma once

#include <iostream>
#include <map>
#include <sstream>
#include <string>

#include "cmd_args/base/def.h"
#include "cmd_args/base/util.h"

CMD_ARGS_NAMESPACE_BEGIN

/*
argument 是必须提供的，没有对应的参数会报错

named_argument， 命名参数，指的是按照参数名字来设置值，“set name="valens"”
position_argument, 方位参数，指的是，命令行解析完毕之后，依次把剩下的值传给方位参数
*/
namespace detail {

struct argument
{
    std::string         strName;
    std::string         strType;
    mutable std::string strValue;
    std::string         strHelp;

    argument(std::string _name, std::string _Type, std::string _Help)
        : strName(std::move(_name))
        , strType(std::move(_Type))
        , strHelp(std::move(_Help))
    {}
    virtual ~argument() = default;
    bool hasValue() const noexcept { return !strValue.empty(); }
};

struct named_argument : public argument
{
    using argument::argument;
};

struct position_argument : named_argument
{
    size_t iSeq;
    position_argument(size_t _Seq, std::string _Name, std::string _Type, std::string _Help)
        : named_argument(_Name, _Type, _Help)
        , iSeq(_Seq)
    {}
    bool operator<(position_argument const &_Rhs) const noexcept { return iSeq < _Rhs.iSeq; }
};

class arg_mgr_itf
{
public:
    virtual std::string     doGetUsage() const                                              = 0;
    virtual bool            doFind(std::string const &_Key) const noexcept                  = 0;
    virtual bool            doEmpty() const noexcept                                        = 0;
    virtual size_t          doSize() const noexcept                                         = 0;
    virtual void            doVisit(std::function<void(argument const &)> _Fn) const        = 0;
    virtual void            doGetHelp(std::ostringstream &_Out, std::size_t _Max) const     = 0;
    virtual argument const *doGet(std::string const &_Key) const                            = 0;
    virtual void            doPush(std::string _Name, std::string _Type, std::string _Help) = 0;
};

class named_arg_mgr : arg_mgr_itf
{
public:
    bool set(std::string const &line)
    {
        auto p  = util::parse_line(line);
        auto it = mapArgs.find(p.first);
        if (it == mapArgs.end()) return false;

        it->second.strValue = p.second;

        if (!it->second.hasValue()) {
            std::cerr << "(parse error) named_argument " << it->second.strName << " should have value" << std::endl;
            std::exit(-1);
        }

        return true;
    }
    void doPush(std::string _Name, std::string _Type, std::string _Help) override
    {
        mapArgs.emplace(_Name, named_argument{ _Name, _Type, _Help });
    }

    std::string doGetUsage() const override
    {
        std::ostringstream oss;
        for (auto &e : mapArgs) { oss << " [=" << e.second.strName << ']'; }
        return oss.str();
    }
    bool   doFind(std::string const &_Key) const noexcept override { return mapArgs.find(_Key) != mapArgs.cend(); }
    bool   doEmpty() const noexcept override { return mapArgs.empty(); }
    size_t doSize() const noexcept override { return mapArgs.size(); }
    void   doVisit(std::function<void(argument const &)> _Fn) const override
    try {
        for (auto &&e : mapArgs) _Fn(e.second);
    }
    catch (std::exception &ex) {
        std::cerr << __FILE__ << std::string(__FUNCTION__) << __LINE__ << "\n"
                  << "Failed to visit that " << ex.what() << std::endl;
    }
    catch (...) {
        std::cerr << __FILE__ << std::string(__FUNCTION__) << __LINE__ << "\n"
                  << "Failed to visit with unknown exception " << std::endl;
    }

    void doGetHelp(std::ostringstream &_Out, std::size_t _Max) const override
    {
        if (mapArgs.empty()) return;

        _Out << "\nNamed arguments:\n";
        _Max = 0;
        for (auto const &arg : mapArgs) { _Max = std::max(_Max, arg.second.strName.length()); }

        _Max = std::max(_Max, std::size_t(25));
        for (auto const &arg : mapArgs) {
            _Out << "  ";
            _Out << arg.second.strName;
            _Out << std::string(_Max - arg.second.strName.length(), ' ') << "(" << arg.second.strType << ") ";
            _Out << util::replace(arg.second.strHelp, "\n", "\n" + std::string(_Max + 2, ' ')) << '\n';
        }
    }
    argument const *doGet(std::string const &_Key) const override
    {
        auto it = mapArgs.find(_Key);
        if (it != mapArgs.end()) return nullptr;

        return dynamic_cast<argument const *>(std::addressof(it->second));
    }

private:
    std::map<std::string, named_argument> mapArgs;
};

class position_arg_mgr : arg_mgr_itf
{
public:
    void set(size_t _pos, std::string _Value)
    {
        auto it = mapArgs.find(_pos);
        if (it == mapArgs.end()) return;

        it->second.strValue = _Value;
    }
    void doPush(std::string _Name, std::string _Type, std::string _Help) override
    {
        mapArgs.emplace(mapArgs.size(), position_argument{ mapArgs.size(), _Name, _Type, _Help });
    }
    std::string doGetUsage() const override
    {
        std::ostringstream oss;
        for (auto &e : mapArgs) { oss << " [" << e.second.strName << '@' << e.first << ']'; }
        return oss.str();
    }
    bool doFind(std::string const &_Key) const noexcept override
    {
        return std::find_if(
                   mapArgs.cbegin(),
                   mapArgs.cend(),
                   [&_Key](decltype(mapArgs)::const_reference arg) { return arg.second.strName == _Key; })
               != mapArgs.cend();
    }
    bool   doEmpty() const noexcept { return mapArgs.empty(); }
    size_t doSize() const noexcept override { return mapArgs.size(); }
    void   doVisit(std::function<void(argument const &)> _Fn) const override
    try {
        for (auto &&e : mapArgs) _Fn(e.second);
    }
    catch (std::exception &ex) {
        std::cerr << __FILE__ << std::string(__FUNCTION__) << __LINE__ << "\n"
                  << "Failed to visit that " << ex.what() << std::endl;
    }
    catch (...) {
        std::cerr << __FILE__ << std::string(__FUNCTION__) << __LINE__ << "\n"
                  << "Failed to visit with unknown exception " << std::endl;
    }

    void doGetHelp(std::ostringstream &_Out, std::size_t _Max) const override
    {
        if (mapArgs.empty()) return;
        _Out << "\nPosition arguments:\n";
        _Max = 0;
        for (auto const &arg : mapArgs) { _Max = std::max(_Max, arg.second.strName.length()); }
        _Max = std::max(_Max, std::size_t(25));
        for (auto const &arg : mapArgs) {
            _Out << "  ";
            _Out << arg.second.strName;
            _Out << std::string(_Max - arg.second.strName.length(), ' ') << "(" << arg.second.strType << ") ";
            _Out << util::replace(arg.second.strHelp, "\n", "\n" + std::string(_Max + 2, ' ')) << '\n';
        }
    }

    argument const *doGet(std::string const &_Key) const override
    {
        auto it = std::find_if(mapArgs.begin(), mapArgs.end(), [&](decltype(mapArgs)::const_reference _V) {
            return _V.second.strName == _Key;
        });
        if (it != mapArgs.end()) return nullptr;

        return dynamic_cast<argument const *>(std::addressof(it->second));
    }

private:
    std::map<size_t, position_argument> mapArgs;
};

}  // namespace detail

using detail::argument;
using detail::named_arg_mgr;
using detail::named_argument;
using detail::position_arg_mgr;
using detail::position_argument;

CMD_ARGS_NAMESPACE_END