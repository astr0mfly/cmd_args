/*
 * Copyright[2023][ValenciaFly]
 * Email: pengwanring@live.com
 *
 *     Licensed under the Apache License,
 *     Version 2.0(the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include "cmd_args/base/def.h"
#include "cmd_args/base/util.h"

CMD_ARGS_NAMESPACE_BEGIN

namespace detail {
struct opt_mgr_itf
{
    virtual option const *doGet(char) const noexcept        = 0;
    virtual option const *doGet(std::string) const noexcept = 0;
};

class value_opt_mgr : opt_mgr_itf
{
public:
    option const *doGet(char _Key) const noexcept override
    {
        auto it = std::find_if(
            m_vecValues.cbegin(),
            m_vecValues.cend(),
            [_Key](decltype(m_vecValues)::const_reference _V) { return _V.short_name == _Key; });
        if (it == m_vecValues.cend()) return nullptr;

        return std::addressof(*it);
    }

    option const *doGet(std::string _Key) const noexcept override
    {
        auto it = std::find_if(
            m_vecValues.cbegin(),
            m_vecValues.cend(),
            [_Key](decltype(m_vecValues)::const_reference _V) { return _V.long_name == _Key; });
        if (it == m_vecValues.cend()) return nullptr;

        return std::addressof(*it);
    }

    void push(char sname, std::string lname, std::string help, std::string type, std::string value)
    {
        m_vecValues.emplace_back(
            sname,
            std::move(lname),
            std::move(help),
            std::move(type),
            std::move(value));
    }

    void push(std::string lname, std::string help, std::string type, std::string value)
    {
        m_vecValues
            .emplace_back(std::move(lname), std::move(help), std::move(type), std::move(value));
    }

    option_value const *find(char _Key) const
    {
        for (auto &v : m_vecValues) {
            if (v.short_name == 0) continue;
            if (v.short_name == _Key) return std::addressof(v);
        }

        return nullptr;
    }

    option_value const *find(std::string _Key) const
    {
        for (auto &v : m_vecValues) {
            if (v.long_name == _Key) return std::addressof(v);
        }

        return nullptr;
    }

    std::size_t calc_desc_max_length() const noexcept
    {
        std::size_t max_name_length = 0;
        for (auto const &opt : m_vecValues) {
            std::size_t length = opt.long_name.length();
            if (opt.short_name == 0) { length += 4; }
            max_name_length = std::max(max_name_length, length);
        }

        return max_name_length;
    }

    void appendHelp(std::ostringstream &_Out, std::size_t _Max) const
    {
        for (auto const &opt : m_vecValues) {
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

class action_opt_mgr : opt_mgr_itf
{
public:
    option const *doGet(char _Key) const noexcept override
    {
        auto it = std::find_if(
            m_vecActions.cbegin(),
            m_vecActions.cend(),
            [_Key](decltype(m_vecActions)::const_reference _V) { return _V.short_name == _Key; });
        if (it == m_vecActions.cend()) return nullptr;

        return std::addressof(*it);
    }

    option const *doGet(std::string _Key) const noexcept override
    {
        auto it = std::find_if(
            m_vecActions.cbegin(),
            m_vecActions.cend(),
            [_Key](decltype(m_vecActions)::const_reference _V) { return _V.long_name == _Key; });
        if (it == m_vecActions.cend()) return nullptr;

        return std::addressof(*it);
    }

    void push(char sname, std::string lname, std::string help, std::function<void(void)> callback)
    {
        m_vecActions.emplace_back(sname, std::move(lname), std::move(help), std::move(callback));
    }

    void push(std::string lname, std::string help, std::function<void(void)> callback)
    {
        m_vecActions.emplace_back(std::move(lname), std::move(help), std::move(callback));
    }

    std::size_t calc_desc_max_length() const noexcept
    {
        std::size_t max_name_length = 0;
        for (auto const &opt : m_vecActions) {
            std::size_t length = opt.long_name.length();
            if (opt.short_name == 0) { length += 4; }
            max_name_length = std::max(max_name_length, length);
        }

        return max_name_length;
    }

    void appendHelp(std::ostringstream &_Out, std::size_t _Max) const
    {
        for (auto const &opt : m_vecActions) {
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

struct OptionData
{
    action_opt_mgr              short_circuit_options;
    value_opt_mgr               options;
    std::map<char, std::string> short_name_index;

    
    std::size_t calc_desc_max_length() const noexcept
    {
        std::size_t max_name_length = 0;

        max_name_length = short_circuit_options.calc_desc_max_length();
        max_name_length = std::max(options.calc_desc_max_length(), std::size_t(25));

        return max_name_length;
    }
    void appendHelp(std::ostringstream &_Oss, size_t &_Max) const
    {
        short_circuit_options.appendHelp(_Oss, _Max);
        options.appendHelp(_Oss, _Max);
    }
};

}  // namespace detail

using detail::OptionData;


CMD_ARGS_NAMESPACE_END
