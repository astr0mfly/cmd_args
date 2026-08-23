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

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <type_traits>

#include "cmd_args/context/parse_fwd.h"
#include "cmd_args/error/parser_error.h"
#include "cmd_args/option/option.h"
#include "cmd_args/option/option_data.h"

CMD_ARGS_NAMESPACE_BEGIN

/*
    选项解析器：消费 Tokens 中形如
        --long=value / --long value / -x value / -xvalue / -abc(聚合 bool)
    的选项 token。

    - 短回路选项优先级最高：命中其一立即回调并停止解析
    - 未注册的长选项 token 原样保留（可能是命名参数，由上层决定）
    - 未知的短选项在聚合串里直接报 UNKNOWN_OPTION
    - `--` 之后的 token 一律不再作为选项处理
*/
class option_parser
{
public:
    option_parser(OptionData *_Data)
        : m_pData__(_Data)
    {}

    bool parse(Tokens_T &_Tokens)
    {
        m_bFired__  = false;
        m_bOptEnd__ = false;

        for (auto it = _Tokens.begin(); it != _Tokens.end();) {
            std::string const &tok = *it;

            if (tok == "--") {
                // 结束选项区：其后的 token 全部留给位置参数
                _Tokens.erase(it);
                m_bOptEnd__ = true;
                break;
            }

            if (util::starts_with(tok, "--") && tok.size() > 2) {
                auto          body = tok.substr(2);
                auto          eq   = body.find('=');
                std::string   name = (eq == std::string::npos) ? body : body.substr(0, eq);
                std::string   value;
                if (eq != std::string::npos) { value = body.substr(eq + 1); }

                if (__fireOnLong(name, it, _Tokens)) return true;

                auto *pOpt = m_pData__->options.mut_find(name);
                if (pOpt) {
                    // --long=value 与 --long value 两种形式都支持；
                    // 未显式给值且非 bool 选项时，消费下一个 token 作为值
                    if (value.empty() && pOpt->type != "bool") {
                        auto nx = std::next(it);
                        if (nx == _Tokens.end() || util::starts_with(*nx, "-")) {
                            throw ErrorParse(
                                ParseError_E::NEED_VALUE,
                                "option --" + name + " need a value");
                        }
                        value = *nx;
                        _Tokens.erase(nx);
                    }
                    __assign(pOpt, value, pOpt->type == "bool");
                    it = _Tokens.erase(it);
                    continue;
                }

                // 不是选项：可能是命名参数（--name=value），留给上层
                ++it;
                continue;
            }

            if (util::starts_with(tok, "-") && tok.size() >= 2) {
                if (__fireOnShort(tok, it, _Tokens)) return true;

                if (tok.size() == 2) {
                    auto *pOpt = m_pData__->options.mut_find(tok[1]);
                    if (!pOpt) {
                        ++it;  // 可能是未识别短选项，交给上层统一报错
                        continue;
                    }
                    if (pOpt->type == "bool") {
                        __assign(pOpt, "", true);
                    }
                    else {
                        auto nx = std::next(it);
                        if (nx == _Tokens.end()) {
                            throw ErrorParse(
                                ParseError_E::NEED_VALUE,
                                "option -" + std::string(1, tok[1]) + " need a value");
                        }
                        __assign(pOpt, *nx, false);
                        _Tokens.erase(nx);
                    }
                    it = _Tokens.erase(it);
                    continue;
                }

                // 聚合短选项：-abc 或 -tvalue
                bool consumed = false;
                for (size_t i = 1; i < tok.size() && !consumed; ++i) {
                    char key = tok[i];
                    if (__fireOnShortChar(key, it, _Tokens)) return true;

                    auto *pOpt = m_pData__->options.mut_find(key);
                    if (!pOpt) {
                        throw ErrorParse(
                            ParseError_E::UNKNOWN_OPTION,
                            "unrecognized short option '" + std::string(1, key) + "' in " + tok);
                    }
                    if (pOpt->type == "bool") {
                        __assign(pOpt, "", true);
                        continue;
                    }

                    // 值选项出现在聚合串中：余下部分就是它的值（-tvalue）
                    std::string val = tok.substr(i + 1);
                    if (!val.empty() && val.front() == '=') { val = val.substr(1); }
                    if (val.empty()) {
                        auto nx = std::next(it);
                        if (nx == _Tokens.end()) {
                            throw ErrorParse(
                                ParseError_E::NEED_VALUE,
                                "option -" + std::string(1, key) + " need a value");
                        }
                        val = *nx;
                        _Tokens.erase(nx);
                    }
                    __assign(pOpt, val, false);
                    consumed = true;
                }

                it = _Tokens.erase(it);
                continue;
            }

            // 普通 token：留给参数解析
            ++it;
            continue;
        }

        return m_bFired__;
    }

    bool fired() const noexcept { return m_bFired__; }
    bool options_ended() const noexcept { return m_bOptEnd__; }

    bool parsed(NameShort_T _Name) const noexcept
    {
        if (_Name == '\0') return false;
        auto const *p = m_pData__->options.find(_Name);
        return p && p->bExplicit__;
    }
    bool parsed(NameLong_T const &_Name) const noexcept
    {
        auto const *p = m_pData__->options.find(_Name);
        return p && p->bExplicit__;
    }

    bool has_option(std::string const &_Name) const noexcept
    {
        auto key = _Name;
        if (util::starts_with(key, "--")) { key = key.substr(2); }

        if (auto const *p = m_pData__->options.find(key)) { (void)p; return true; }
        if (key.size() == 1 && m_pData__->options.find(key[0])) { return true; }
        return false;
    }

    template <typename T>
    T get_option(std::string const &_Name) const
    {
        using namespace util;
        auto key = _Name;
        if (util::starts_with(key, "--")) { key = key.substr(2); }

        auto const *p = m_pData__->options.find(key);
        if (!p && key.size() == 1) { p = m_pData__->options.find(key[0]); }
        if (!p) {
            throw ErrorParse(ParseError_E::UNKNOWN_OPTION, "option not found: " + key);
        }

        if (p->type == "bool") {
            if constexpr (std::is_same_v<T, bool>) { return util::parse_bool(p->value); }
            throw ErrorParse(ParseError_E::UNKNOWN_OPTION, "option type mismatch: " + key);
        }
        if (p->type != util::type_string<T>()) {
            throw ErrorParse(
                ParseError_E::UNKNOWN_OPTION,
                "option type mismatch: set '" + p->type + "' but try get with '"
                    + util::type_string<T>() + "'");
        }

        return util::parse_value<T>(p->value);
    }

    bool    has_option_bool(std::string const &_Name) const { return get_option<bool>(_Name); }
    bool    get_option_bool(std::string const &_Name) const { return get_option<bool>(_Name); }
    int     get_option_int(std::string const &_Name) const { return get_option<int>(_Name); }
    int64_t get_option_int64(std::string const &_Name) const { return get_option<int64_t>(_Name); }
    double  get_option_double(std::string const &_Name) const { return get_option<double>(_Name); }
    std::string get_option_string(std::string const &_Name) const
    {
        return get_option<std::string>(_Name);
    }

    std::string usage_string() const { return m_pData__->usage_string(); }

    void appendHelp(std::ostringstream &_Out) const
    {
        if (m_pData__->short_circuit_options.doEmpty() && m_pData__->options.doEmpty()) return;
        _Out << "\nOptions:\n";
        auto max = m_pData__->calc_desc_max_length();
        m_pData__->appendHelp(_Out, max);
    }

private:
    /* 赋值并联动：bExplicit__ 标记、fnOnSet__ 钩子、action 系统调用 */
    void __assign(option_value *_Opt, std::string const &_Value, bool _IsBool)
    {
        if (_IsBool) {
            _Opt->value = _Value.empty() ? "1" : (util::parse_bool(_Value) ? "1" : "0");
        }
        else {
            _Opt->value = _Value;
        }
        _Opt->bExplicit__ = true;
        if (_Opt->fnOnSet__) { _Opt->fnOnSet__(_Opt->value); }

        // 绑定了 action 的选项：命中即通过 std::system 执行子进程命令。
        // __assign 仅在暴露词法 token 的显式赋值路径被调用，且按命令行出现顺序
        // （长/短/聚合短选项逐一命中），因此 action 天然"按先后顺序"执行；
        // 环境变量兜底 / YAML 外部赋值不会走到这里，不会触发 action。
        if (!_Opt->strActionTemplate__.empty()) {
            auto cmd = util::replace(_Opt->strActionTemplate__, "{value}", _Opt->value);
            if (!cmd.empty()) {
                int ret = std::system(cmd.c_str());
                if (ret != 0) {
                    std::cerr << "(action) command exit " << ret << ": " << cmd << "\n";
                }
            }
        }
    }

    bool __fireOnLong(
        std::string const &_Name, Tokens_T::iterator &_It, Tokens_T &_Tokens)
    {
        auto const *pSc = m_pData__->short_circuit_options.doGet(_Name);
        if (!pSc) return false;

        auto const *pAct = static_cast<option_action const *>(pSc);
        m_bFired__ = true;
        _It        = _Tokens.erase(_It);
        pAct->callback();
        return true;
    }

    bool __fireOnShort(std::string const &_Tok, Tokens_T::iterator &_It, Tokens_T &_Tokens)
    {
        if (_Tok.size() != 2) return false;

        auto const *pSc = m_pData__->short_circuit_options.doGet(_Tok[1]);
        if (!pSc) return false;

        auto const *pAct = static_cast<option_action const *>(pSc);
        m_bFired__ = true;
        _It        = _Tokens.erase(_It);
        pAct->callback();
        return true;
    }

    bool __fireOnShortChar(char _Key, Tokens_T::iterator &_It, Tokens_T &_Tokens)
    {
        auto const *pSc = m_pData__->short_circuit_options.doGet(_Key);
        if (!pSc) return false;

        auto const *pAct = static_cast<option_action const *>(pSc);
        m_bFired__ = true;
        _It        = _Tokens.erase(_It);
        pAct->callback();
        return true;
    }

private:
    OptionData    *m_pData__ = nullptr;
    bool           m_bFired__  = false;
    bool           m_bOptEnd__ = false;
};

CMD_ARGS_NAMESPACE_END