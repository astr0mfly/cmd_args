/*
 * Copyright[2026][ValenciaFly]
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

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include "cmd_args/base/def.h"
#include "cmd_args/argument/argument_parser.h"
#include "cmd_args/context/lexer.h"
#include "cmd_args/env/env_builder.h"
#include "cmd_args/env/env_parser.h"
#include "cmd_args/error/errors.h"
#include "cmd_args/error/interpreter_error.h"
#include "cmd_args/error/lexer_error.h"
#include "cmd_args/error/parser_error.h"
#include "cmd_args/option/option_parser.h"

CMD_ARGS_NAMESPACE_BEGIN

/*
    Interpreter（流式实时引擎）：把一行命令行当做字符流，
    经 Scanner::step 逐字符吐出 Token；每得到一个完整 Token 即分类成
    Expression（Option / Argument / 方位参数），并按类型立即执行——
    选项赋值即刻触发 fnOnSet__ 与 action 模板，命名参数即刻 set，
    方位参数按序缓冲至流空后统一提交（数量校验与批量一致）。直到 Stream 为空。

    与批量 Compiler 的区别：
    - Compiler：先纯解析全部 Token 入库 Model，Executor 按 actionOrder 后置统一执行。
    - Interpreter：解析即执行，无需 Executor / actionOrder。
*/
class Interpreter : private ArgumentParser, private OptionParser, private EnvParser
{
public:
    Interpreter(Model *_Model)
        : ArgumentParser(&_Model->m_args__)
        , OptionParser(&_Model->m_opts__)
        , EnvParser(&_Model->m_envs__)
        , m_pModel__(_Model)
    {}

    /* ===== 逐行解释 ===== */

    /// @brief 解释执行一行：reset → 字符泵逐 Token 分类 → 立即执行 → 方位参数提交 → finalize
    std::error_code interpret(std::string const &_Line, Errors &_Errors)
    {
        m_pModel__->m_args__.mgrName.doMutVisit([](Argument &a) { a.reset(); });
        m_pModel__->m_args__.mgrPos.doMutVisit([](Argument &a) { a.reset(); });
        m_pModel__->m_opts__.mgrValues.forEachValue([](OptionValue &o) { o.reset(); });
        _Errors.clearLast();

        try {
            cmd_args::detail::Scanner sc({});
            std::vector<std::string> vecPos;        ///< 方位参数缓冲（流空后按序提交）
            std::vector<std::string> vecDashLeft;   ///< 未识别 dash token 缓冲（不参与方位参数校验/赋值）
            std::string strPendingKey;              ///< 等待取值的选项/命名参数名
            /// 挂起方类型：决定取值消费规则与 NEED_VALUE 文案（对齐批量语义）
            enum class PendingKind_E : char { None, LongOpt, ShortOpt, NamedArg };
            PendingKind_E enPending      = PendingKind_E::None;
            bool          bPendingInlineEq = false;  ///< 挂起方是否由 '=' 内联分隔（仅 LongOpt 有意义）
            bool          bOptEnd        = false;    ///< '--' 之后

            auto pump = [&](char _C) {
                auto tokOpt = sc.step(_C);
                if (!tokOpt) return;
                std::string text   = tokOpt->toText();
                bool        bInlineEq = tokOpt->hasInlineEq();

                // ── 挂起取值：当前 token 就是值 ──
                if (!strPendingKey.empty()) {
                    // 消费规则（批量语义）：
                    // - LongOpt：拒绝 dash 开头值，除非挂起方带内联 '='（--opt=-5）
                    // - ShortOpt：无条件接受下一个 token（option_parser.h 短路径无 dash 检查）
                    // - NamedArg：无条件接受下一个 token（ArgumentParser::parseNamedOnly 语义）
                    if (enPending == PendingKind_E::LongOpt && !bPendingInlineEq
                        && util::starts_with(text, "-")) {
                        throw ErrorParse(ParseError_E::NEED_VALUE,
                            "option --" + strPendingKey + " need a value");
                    }
                    if (enPending == PendingKind_E::NamedArg) {
                        if (auto *pArg = m_pModel__->m_args__.mgrName.mutGet(strPendingKey)) {
                            pArg->set(text);
                        }
                    }
                    else if (enPending == PendingKind_E::ShortOpt) {
                        // 短名按 char 查（mutFind(std::string) 只按长名匹配）
                        if (auto *pOpt = m_pModel__->m_opts__.mgrValues.mutFind(strPendingKey.front())) {
                            __executeOptionImmediate(pOpt, text, pOpt->strType == "bool");
                        }
                    }
                    else if (auto *pOpt = m_pModel__->m_opts__.mgrValues.mutFind(strPendingKey)) {
                        __executeOptionImmediate(pOpt, text, pOpt->strType == "bool");
                    }
                    strPendingKey.clear();
                    enPending = PendingKind_E::None;
                    return;
                }

                // ── '--' 之后全部方位参数 ──
                if (bOptEnd) { vecPos.emplace_back(text); return; }

                if (text == "--") { bOptEnd = true; return; }

                // ── 长选项 / 命名参数：--name[=value] ──
                if (util::starts_with(text, "--") && text.size() > 2) {
                    auto        body  = text.substr(2);
                    auto        eq    = body.find('=');
                    std::string name  = body.substr(0, eq == std::string::npos ? body.size() : eq);
                    std::string value = eq == std::string::npos ? "" : body.substr(eq + 1);

                    if (auto const *pSc = m_pModel__->m_opts__.mgrActions.doGet(name)) {
                        static_cast<OptionAction const *>(pSc)->fnCallback();  // 短回路：立即回调
                        return;
                    }
                    if (auto *pOpt = m_pModel__->m_opts__.mgrValues.mutFind(name)) {
                        // 无内联值：等待下一 token 取值。bInlineEq 表达「值在下个 token
                        // 且允许 dash 开头」（--opt=value 被 Scanner 在 '=' 处切开）
                        if (value.empty() && (bInlineEq || pOpt->strType != "bool")) {
                            strPendingKey = name; enPending = PendingKind_E::LongOpt;
                            bPendingInlineEq = bInlineEq;
                            return;
                        }
                        __executeOptionImmediate(pOpt, value, pOpt->strType == "bool");
                        return;
                    }
                    if (auto *pArg = m_pModel__->m_args__.mgrName.mutGet(name)) {
                        if (value.empty()) {
                            strPendingKey = name; enPending = PendingKind_E::NamedArg;
                            bPendingInlineEq = false;
                            return;
                        }
                        pArg->set(std::move(value));  // 命名参数表达式：立即 set
                        return;
                    }
                    vecDashLeft.emplace_back(text);  // 未识别长选项：独立缓冲，流末统一报 UNKNOWN_OPTION
                    return;
                }

                // ── 短选项：-x / 聚合 -abc ──
                if (util::starts_with(text, "-") && text.size() >= 2) {
                    for (size_t i = 1; i < text.size(); ++i) {
                        char key = text[i];
                        if (auto const *pSc = m_pModel__->m_opts__.mgrActions.doGet(key)) {
                            static_cast<OptionAction const *>(pSc)->fnCallback();
                            continue;
                        }
                        auto *pOpt = m_pModel__->m_opts__.mgrValues.mutFind(key);
                        if (!pOpt) {
                            throw ErrorParse(ParseError_E::UNKNOWN_OPTION,
                                "unrecognized short option '" + std::string(1, key) + "' in " + text);
                        }
                        if (pOpt->strType == "bool") {
                            __executeOptionImmediate(pOpt, "", true);
                            continue;
                        }
                        std::string val = text.substr(i + 1);  // 值选项：余下即值（-tvalue / -t=value）
                        if (!val.empty() && val.front() == '=') { val = val.substr(1); }
                        if (val.empty()) {
                            // 值在下一个 token（含聚合串末尾，如 -bt）：批量短路径无条件接受
                            strPendingKey.assign(1, key); enPending = PendingKind_E::ShortOpt;
                            bPendingInlineEq = false;
                            return;
                        }
                        __executeOptionImmediate(pOpt, val, false);
                        return;                              // 聚合串中值选项即终结
                    }
                    return;
                }

                // ── 普通 token → 方位参数 ──
                vecPos.emplace_back(text);
            };

            for (char c : _Line) { pump(c); }
            pump('\0');                                          // 流结束：flush 最后一个 token

            // 流空后仍有挂起取值（--opt / --name 后无值）：与批量路径一致报 NEED_VALUE
            if (!strPendingKey.empty()) {
                if (enPending == PendingKind_E::NamedArg) {
                    throw ErrorParse(ParseError_E::NEED_VALUE,
                        "named argument '--" + strPendingKey + "' need a value");
                }
                if (enPending == PendingKind_E::LongOpt) {
                    throw ErrorParse(ParseError_E::NEED_VALUE,
                        "option --" + strPendingKey + " need a value");
                }
                throw ErrorParse(ParseError_E::NEED_VALUE,
                    "option -" + strPendingKey + " need a value");
            }

            // 未识别 dash token：先于方位参数提交报 UNKNOWN_OPTION（UNKNOWN_OPTION 优先，
            // 且绝不占用方位槽位、不产生赋值副作用——批量语义中 dash token 不进方位缓冲）。
            if (!vecDashLeft.empty()) {
                throw ErrorParse(ParseError_E::UNKNOWN_OPTION,
                    "unrecognized option or argument: " + vecDashLeft.front());
            }

            __assignPositional(vecPos);                          // 数量校验 + 按序提交

            __finalize();
            m_bParsed__ = true;
            return make_error_code(ParseError_E::OK);
        }
        catch (ErrorParse const &e) { _Errors.setLast(e.code()); return e.code(); }
        catch (ErrorLexer const &e) { _Errors.setLast(e.code()); return e.code(); }
        catch (std::exception const &ex) {
            _Errors.setMsg(ex.what());
            return make_error_code(ParseError_E::UNKNWON_ARGS);
        }
    }

    /* ===== 解析后置：seed 环境兜底 + ${VAR} 展开 ===== */

    void finalize()
    {
        __seedEnvDefaults();
        __expandEnvValues();

        if (!m_pModel__->m_strSaveConfigPath__.empty()) {
            auto text = config::to_yaml(*m_pModel__);
            if (!config::write_file(m_pModel__->m_strSaveConfigPath__, text)) {
                std::cerr << "(save-config) cannot write file: " << m_pModel__->m_strSaveConfigPath__ << "\n";
            }
        }
    }

    /* ===== once / loop ===== */

    void setExitCode(int _Code) noexcept { m_nExitCode__ = _Code; }
    int  exitCode() const noexcept { return m_nExitCode__; }
    void markParsed() noexcept { m_bParsed__ = true; }
    bool prepared() const noexcept { return m_bParsed__; }

    std::string describeError(std::error_code const &_Ec, Errors &_Errors) const
    {
        return std::string("(parse error) ") + _Ec.message() + "\n" + _Errors.dump();
    }

    [[noreturn]] void once()
    {
        if (!m_bParsed__) { throw ErrorInterp(InterpError_E::STOPPED); }
        std::exit(m_nExitCode__);
    }

    void loop(Errors &_Errors)
    {
        std::cout << "cmd_args> " << std::flush;
        std::string line;
        while (std::getline(std::cin, line)) {
            auto cmd = util::trim(line);
            if (cmd.empty()) {
                std::cout << "cmd_args> " << std::flush;
                continue;
            }
            if (cmd == "exit" || cmd == "quit" || cmd == "exit()") break;

            auto ec = interpret(cmd, _Errors);
            if (ec) { std::cout << describeError(ec, _Errors); }

            std::cout << "cmd_args> " << std::flush;
        }
        std::cout << "\n";
    }

private:
    /// @brief 选项赋值 + 立即执行（流式专用）：值 → 标记 → fnOnSet__ → action 模板
    /// @note 流式路径不经 OptionParser::__assign，内置选项回调在此触发（等价其内联语义）
    void __executeOptionImmediate(OptionValue *_Opt, std::string const &_Value, bool _IsBool)
    {
        if (_IsBool) {
            _Opt->strValue = _Value.empty() ? "1" : (util::parse_bool(_Value) ? "1" : "0");
        }
        else {
            _Opt->strValue = _Value;
        }
        _Opt->bExplicit__ = true;

        // 自定义回调：内置（--env-set 等）与用户选项一律即时触发（解析即执行）
        if (_Opt->fnOnSet__) { _Opt->fnOnSet__(_Opt->strValue); }

        // action 模板 → 子进程命令；非 0 告警不中断
        if (!_Opt->strActionTemplate__.empty()) {
            auto cmd = util::replace(_Opt->strActionTemplate__, "{value}", _Opt->strValue);
            if (!cmd.empty()) {
                int ret = std::system(cmd.c_str());
                if (ret != 0) {
                    std::cerr << "(action) command exit " << ret << ": " << cmd << "\n";
                }
            }
        }
    }

    /// @brief 按声明顺序提交方位参数（数量校验与批量 ArgumentParser 一致）
    void __assignPositional(std::vector<std::string> const &_VecPos)
    {
        size_t total = m_pModel__->m_args__.mgrPos.doSize();
        size_t need  = 0;
        m_pModel__->m_args__.mgrPos.doConVisit([&](Argument const &a) {
            if (!a.isExternal()) ++need;
        });
        if (_VecPos.size() < need || _VecPos.size() > total) {
            std::ostringstream oss;
            oss << "give " << _VecPos.size() << ", but need " << need << ".." << total << '\n';
            oss << "uncaptured command line arguments:\n";
            for (auto const &tok : _VecPos) { oss << tok << '\n'; }
            throw ErrorParse(ParseError_E::UNKNWON_ARGS, oss.str());
        }

        bool   exact = (_VecPos.size() == need);
        size_t vi    = 0;
        for (size_t seq = 0; seq < total && vi < _VecPos.size(); ++seq) {
            auto *p = m_pModel__->m_args__.mgrPos.mutGetBySeq(seq);
            if (!p) continue;
            if (exact && p->isExternal()) continue;
            p->set(_VecPos[vi++]);
        }
    }

    void __seedEnvDefaults()
    {
        m_pModel__->m_args__.mgrName.doMutVisit([&](Argument &a) {
            if (a.getEnvName().empty() || a.hasValue() || a.isExternal()) return;
            if (this->EnvParser::has(a.getEnvName())) { a.set(this->EnvParser::get(a.getEnvName())); }
        });
        m_pModel__->m_args__.mgrPos.doMutVisit([&](Argument &a) {
            if (a.getEnvName().empty() || a.hasValue() || a.isExternal()) return;
            if (this->EnvParser::has(a.getEnvName())) { a.set(this->EnvParser::get(a.getEnvName())); }
        });
        m_pModel__->m_opts__.mgrValues.forEachValue([&](OptionValue &o) {
            if (o.strEnvName__.empty() || o.bExplicit__ || o.bExternal__) return;
            if (this->EnvParser::has(o.strEnvName__)) {
                o.strValue = this->EnvParser::get(o.strEnvName__);
            }
        });
    }

    void __expandEnvValues()
    {
        m_pModel__->m_args__.mgrName.doMutVisit([&](Argument &a) {
            if (a.hasValue()) { a.set(this->EnvParser::expand(a.getValue())); }
        });
        m_pModel__->m_args__.mgrPos.doMutVisit([&](Argument &a) {
            if (a.hasValue()) { a.set(this->EnvParser::expand(a.getValue())); }
        });
        m_pModel__->m_opts__.mgrValues.forEachValue([&](OptionValue &o) {
            if (!o.strValue.empty()) { o.strValue = this->EnvParser::expand(o.strValue); }
        });
    }

    void __finalize()
    {
        __seedEnvDefaults();
        __expandEnvValues();

        if (!m_pModel__->m_strSaveConfigPath__.empty()) {
            auto text = config::to_yaml(*m_pModel__);
            if (!config::write_file(m_pModel__->m_strSaveConfigPath__, text)) {
                std::cerr << "(save-config) cannot write file: " << m_pModel__->m_strSaveConfigPath__ << "\n";
            }
        }
    }

private:
    Model *m_pModel__ = nullptr;
    bool   m_bParsed__   = false;
    int    m_nExitCode__ = 0;
};

CMD_ARGS_NAMESPACE_END
