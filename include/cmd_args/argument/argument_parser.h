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

/*
    参数解析器：
    - 命名参数  : --name=value / --name value
    - 方位参数  : 剩余的非选项 token，按声明顺序依次赋值
    - 数量校验  : 方位参数数量不一致抛 UNKNWON_ARGS（携带明细）
*/
class argument_parser
{
public:
    argument_parser(ArgumentData *_Data)
        : m_pData__(_Data)
    {}

    void parse(Tokens_T &_Tokens)
    {
        parseNamedOnly(_Tokens);

        // 2. 方位参数：剩余的非选项 token
        std::vector<std::string> vecPos;
        for (auto const &t : _Tokens) {
            if (!util::starts_with(t, "-")) { vecPos.emplace_back(t); }
        }

        if (vecPos.size() != m_pData__->mgrPos.doSize()) {
            std::ostringstream oss;
            oss << "give " << vecPos.size() << ", but need " << m_pData__->mgrPos.doSize() << '\n';
            oss << "uncaptured command line arguments:\n";
            for (auto const &tok : _Tokens) { oss << tok << '\n'; }
            throw ErrorParse(ParseError_E::UNKNWON_ARGS, oss.str());
        }

        size_t i = 0;
        for (auto pos = _Tokens.begin(); pos != _Tokens.end();) {
            if (!util::starts_with(*pos, "-")) {
                auto *p = m_pData__->mgrPos.mutGetBySeq(i);
                if (p) { p->set(vecPos[i]); }
                pos = _Tokens.erase(pos);
                ++i;
                continue;
            }
            ++pos;
        }
    }

    /*
        只消化命名参数（--name=value / --name value），不动方位参数。
        用于 `--` 结束选项区之前先消费命名参数的场景。
    */
    void parseNamedOnly(Tokens_T &_Tokens)
    {
        for (auto pos = _Tokens.begin(); pos != _Tokens.end();) {
            std::string const &tok = *pos;
            if (!util::starts_with(tok, "--")) {
                ++pos;
                continue;
            }

            auto        body = tok.substr(2);
            auto        eq   = body.find('=');
            std::string name = (eq == std::string::npos) ? body : body.substr(0, eq);
            std::string value;
            if (eq != std::string::npos) { value = body.substr(eq + 1); }

            auto *p = m_pData__->mgrName.mutGet(name);
            if (!p) {
                ++pos;  // 未知长选项/命名参数，留给上层报错
                continue;
            }

            if (value.empty()) {
                auto nx = std::next(pos);
                if (nx == _Tokens.end()) {
                    throw ErrorParse(
                        ParseError_E::NEED_VALUE,
                        "named argument '--" + name + "' need a value");
                }
                value = *nx;
                _Tokens.erase(nx);
            }

            p->set(std::move(value));
            pos = _Tokens.erase(pos);
        }
    }

    /*
        `--` 之后进入纯方位模式：剩余的全部 token（含 - 开头）都当方位参数
    */
    void parsePositionalOnly(Tokens_T &_Tokens)
    {
        std::vector<std::string> vecPos(_Tokens.begin(), _Tokens.end());
        if (vecPos.size() != m_pData__->mgrPos.doSize()) {
            std::ostringstream oss;
            oss << "give " << vecPos.size() << ", but need " << m_pData__->mgrPos.doSize() << '\n';
            throw ErrorParse(ParseError_E::UNKNWON_ARGS, oss.str());
        }

        for (size_t i = 0; i < vecPos.size(); ++i) {
            auto *p = m_pData__->mgrPos.mutGetBySeq(i);
            if (p) { p->set(vecPos[i]); }
        }
        _Tokens.clear();
    }

    void parse(std::string const &_Line)
    {
        auto [k, v] = util::parse_line(_Line);
        auto *p     = m_pData__->mgrName.mutGet(k);
        if (!p) { throw ErrorParse(ParseError_E::UNKNWON_ARGS, "unknown named argument: " + k); }
        p->set(std::move(v));
    }

    template <typename T>
    T get_argument(std::string const &name) const
    {
        using namespace util;
        auto const *p = m_pData__->mgrName.doGet(name);
        if (!p) { p = m_pData__->mgrPos.doGet(name); }
        if (!p) {
            throw ErrorParse(ParseError_E::UNKNWON_ARGS, "argument not found: " + name);
        }

        auto const &type = p->getType();
        if (!type.empty() && type != util::type_string<T>()) {
            throw ErrorParse(
                ParseError_E::UNKNWON_ARGS,
                "argument type mismatch: set '" + type + "' but try get with '"
                    + util::type_string<T>() + "'");
        }

        return util::parse_value<T>(p->getValue());
    }

    bool parsed(NameLong_T const &_Name) const noexcept
    {
        auto const *p = m_pData__->mgrName.doGet(_Name);
        return p && p->hasValue();
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