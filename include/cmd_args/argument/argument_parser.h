/*
 * Copyright [2023] [ValenciaFly] <pengwanring@live.com>
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

#include <sstream>
#include <vector>

#include "cmd_args/argument/argument.h"
#include "cmd_args/argument/argument_data.h"
#include "cmd_args/context/parse_fwd.h"
#include "cmd_args/error/parser_error.h"

CMD_ARGS_NAMESPACE_BEGIN

class argument_parser
{
private:
public:
    argument_parser(ArgumentData *_Data)
        : m_pData__(_Data)
    {}

    void parse(Tokens_T &_Tokens)
    {
        if (_Tokens.size() < m_pData__->mgrName.doSize()) {
            throw ErrorParse(ParseError_E::NOT_ENOUGH_ARGS);
        }

        for (auto pos = _Tokens.begin(); pos != _Tokens.end();) {
            std::error_code ec;
            /*           m_pData__->mgrName.parse(ec, *pos);
                       if (ec) {
                           pos = _Tokens.erase(pos);
                           break;
                       }*/
            ++pos;
        }

        // start parse position arguments
        if (_Tokens.size() != m_pData__->mgrPos.doSize()) {
            std::ostringstream oss;
            oss << "give " << _Tokens.size() << ", but need " << m_pData__->mgrPos.doSize() << '\n';
            oss << "uncaptured command line arguments:\n";
            for (auto const &tok : _Tokens) { oss << tok << '\n'; }

            throw ErrorParse(ParseError_E::UNKNWON_ARGS, oss.str());
        }

        std::size_t i = 0;
        for (auto it{ _Tokens.begin() }; it != _Tokens.end(); ++i, ++it) {
            // m_pData__->mgrPos.parse(i, *it);
        }
    }

    void parse(std::string const &_Line)
    {
        // auto &&[k, v] = util::parse_line(_Line);
        // auto it       = mapArgs.find(k);
        // if (it == mapArgs.end()) { throw ErrorParse(ParseError_E::UNKNWON_ARGS); }

        // if (v.empty()) { throw ErrorParse(ParseError_E::NEED_VALUE); }

        // it->second->set(std::move(v));
    }

    // template <typename T>
    // void check_add_argument_name(std::string const &key) const
    //{
    //     using namespace util;
    //     if (type_string<T>() == "null") {
    //         std::cerr << "(build error) argument type is not supported: " << typeid(T).name()
    //                   << std::endl;
    //         std::exit(-1);
    //     }
    //     if (type_string<T>() == "bool") {
    //         std::cerr << "(build error) argument type cannot be bool" << std::endl;
    //         std::exit(-1);
    //     }
    //     if (key == "") {
    //         std::cerr << "(build error) argument name cannot be empty" << std::endl;
    //         std::exit(-1);
    //     }
    //     if (pos_args.doFind(key) || named_args.doFind(key)) {
    //         std::cerr << "(build error) argument name " << key << " already exists" << std::endl;
    //         std::exit(-1);
    //     }
    // }

    template <typename T>
    T get_argument(std::string const &name) const
    {
        using namespace util;
        auto *p = m_pData__->mgrPos.doGet(name);
        if (p) {
            if (p->getType() != util::type_string<T>()) {
                std::cerr << "(get error) argument type mismatch: set '" << p->getType()
                          << "' but you try get with '" << util::type_string<T>() << "'"
                          << std::endl;
                std::exit(-1);
            }

            return util::parse_value<T>(p->getValue());
        }

        p = m_pData__->mgrPos.doGet(name);
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
        res += m_pData__->mgrName.doGetUsage();
        res += m_pData__->mgrPos.doGetUsage();

        return res;
    }

    void appendHelp(std::ostringstream &_Oss, size_t &_Max) const
    {
        m_pData__->mgrName.doGetHelp(_Oss, _Max);
        m_pData__->mgrPos.doGetHelp(_Oss, _Max);
    }

private:
    ArgumentData *m_pData__ = nullptr;
};

CMD_ARGS_NAMESPACE_END