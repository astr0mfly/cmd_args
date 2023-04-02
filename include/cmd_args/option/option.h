#pragma once

#include <string>
#include <functional>
#include <sstream>
#include "cmd_args/base/def.h"
#include "cmd_args/base/util.h"

CMD_ARGS_NAMESPACE_BEGIN

namespace detail {
struct option {
    char short_name;        // "-t"
    std::string long_name;  // "--test"
    std::string help;       // "it's a test"
    option(char sname, std::string lname, std::string help)
        : short_name(std::move(sname))
        , long_name(std::move(lname))
        , help(std::move(help)) {}
    virtual ~option() = default;
};
/*
option:
FIXME 对于 bool选项，不需要默认值，有对应的命令行参数就是TRUE
其他类型的添加需要默认值
*/
struct option_value : option {
    std::string type;           // bool, int, double, std::string
    mutable std::string value;  // the defaut value

    option_value(char sname, std::string lname, std::string help, std::string type,
                 std::string value)
        : option(sname, std::move(lname), std::move(help))
        , type(std::move(type))
        , value(std::move(value)) {}
    option_value(std::string lname, std::string help, std::string type, std::string value)
        : option(0, std::move(lname), (std::move(help)))
        , type(std::move(type))
        , value(std::move(value)) {}
};

/*
short_circuit_option:
优先级比option高，先得到响应，同时需要一个回调函数，代表行动，之后立即退出程序
*/
struct option_action : option {
    std::function<void(void)> callback;

    option_action(char sname, std::string lname, std::string help,
                  std::function<void(void)> callback)
        : option(sname, std::move(lname), (std::move(help)))
        , callback(std::move(callback)) {}
    option_action(std::string lname, std::string help, std::function<void(void)> callback)
        : option(0, std::move(lname), (std::move(help)))
        , callback(std::move(callback)) {}
};

struct opt_mgr_itf {
    virtual option const* doGet(char) const noexcept = 0;
    virtual option const* doGet(std::string) const noexcept = 0;
};

class value_opt_mgr : opt_mgr_itf {
public:
    option const* doGet(char _Key) const noexcept override {
        auto it = std::find_if(
            m_vecValues.cbegin(), m_vecValues.cend(),
            [_Key](decltype(m_vecValues)::const_reference _V) { return _V.short_name == _Key; });
        if (it == m_vecValues.cend()) return nullptr;

        return std::addressof(*it);
    }

    option const* doGet(std::string _Key) const noexcept override {
        auto it = std::find_if(
            m_vecValues.cbegin(), m_vecValues.cend(),
            [_Key](decltype(m_vecValues)::const_reference _V) { return _V.long_name == _Key; });
        if (it == m_vecValues.cend()) return nullptr;

        return std::addressof(*it);
    }

    void push(char sname, std::string lname, std::string help, std::string type,
              std::string value) {
        m_vecValues.emplace_back(sname, std::move(lname), std::move(help), std::move(type),
                                 std::move(value));
    }

    void push(std::string lname, std::string help, std::string type, std::string value) {
        m_vecValues.emplace_back(std::move(lname), std::move(help), std::move(type),
                                 std::move(value));
    }

    option_value const* find(char _Key) const {
        for (auto& v : m_vecValues) {
            if (v.short_name == 0) continue;
            if (v.short_name == _Key) return std::addressof(v);
        }

        return nullptr;
    }

    option_value const* find(std::string _Key) const {
        for (auto& v : m_vecValues) {
            if (v.long_name == _Key) return std::addressof(v);
        }

        return nullptr;
    }

    std::size_t calc_desc_max_length() const noexcept {
        std::size_t max_name_length = 0;
        for (auto const& opt : m_vecValues) {
            std::size_t length = opt.long_name.length();
            if (opt.short_name == 0) { length += 4; }
            max_name_length = std::max(max_name_length, length);
        }

        return max_name_length;
    }

    void appendHelp(std::ostringstream& _Out, std::size_t _Max) const {
        for (auto const& opt : m_vecValues) {
            _Out << "  ";
            std::size_t printed_length = 0;
            if (opt.short_name) {
                _Out << opt.short_name << ", ";
                printed_length = 4;
            }
            _Out << opt.long_name;
            printed_length += opt.long_name.length();
            _Out << std::string(_Max - printed_length, ' ');
            if (opt.type != "bool") { _Out << "(" << opt.type << ") "; }
            _Out << util::replace(opt.help, "\n", "\n" + std::string(_Max + 2, ' ')) << '\n';
        }
    }

private:
    std::vector<option_value> m_vecValues;
};

class action_opt_mgr : opt_mgr_itf {
public:
    option const* doGet(char _Key) const noexcept override {
        auto it = std::find_if(
            m_vecActions.cbegin(), m_vecActions.cend(),
            [_Key](decltype(m_vecActions)::const_reference _V) { return _V.short_name == _Key; });
        if (it == m_vecActions.cend()) return nullptr;

        return std::addressof(*it);
    }

    option const* doGet(std::string _Key) const noexcept override {
        auto it = std::find_if(
            m_vecActions.cbegin(), m_vecActions.cend(),
            [_Key](decltype(m_vecActions)::const_reference _V) { return _V.long_name == _Key; });
        if (it == m_vecActions.cend()) return nullptr;

        return std::addressof(*it);
    }

    void push(char sname, std::string lname, std::string help, std::function<void(void)> callback) {
        m_vecActions.emplace_back(sname, std::move(lname), std::move(help), std::move(callback));
    }

    void push(std::string lname, std::string help, std::function<void(void)> callback) {
        m_vecActions.emplace_back(std::move(lname), std::move(help), std::move(callback));
    }

    std::size_t calc_desc_max_length() const noexcept {
        std::size_t max_name_length = 0;
        for (auto const& opt : m_vecActions) {
            std::size_t length = opt.long_name.length();
            if (opt.short_name == 0) { length += 4; }
            max_name_length = std::max(max_name_length, length);
        }

        return max_name_length;
    }

    void appendHelp(std::ostringstream& _Out, std::size_t _Max) const {
        for (auto const& opt : m_vecActions) {
            _Out << "  ";
            std::size_t printed_length = 0;
            if (opt.short_name) {
                _Out << opt.short_name << ", ";
                printed_length = 4;
            }
            _Out << opt.long_name;
            printed_length += opt.long_name.length();
            _Out << std::string(_Max - printed_length, ' ');
            _Out << util::replace(opt.help, "\n", "\n" + std::string(_Max + 2, ' ')) << '\n';
        }
    }

private:
    std::vector<option_action> m_vecActions;
};

}  // namespace detail

using detail::option;
using detail::option_value;
using detail::option_action;
using detail::value_opt_mgr;
using detail::action_opt_mgr;

CMD_ARGS_NAMESPACE_END