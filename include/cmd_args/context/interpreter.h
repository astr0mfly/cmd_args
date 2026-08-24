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
#include <string>
#include <system_error>

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
    Interpreter（实时解释引擎）：**继承** ArgumentParser + OptionParser + EnvParser，
    在 OptionParser.__assign 的 virtual hook 中内联触发 OptionAction（fnOnSet__ + action 模板），
    实现逐 Token 的"解析即执行"。

    与纯解析 Compiler 的区别：
    - Compiler（纯解析层）：OptionParser.__assign 仅赋值+标记，action 由 Executor 后置统一执行。
    - Interpreter（实时引擎）：继承 OptionParser 并 override onOptionAssigned，
      在 OptionParser.__assign 内联触发 action，实现逐行/逐 Token 的即时执行。

    使用场景：REPL/loop 模式（逐行解析即执行）。
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

    /* ===== OptionParser 虚拟钩子：解析期内联触发 OptionAction ===== */

    void onOptionAssigned(OptionValue *_Opt, std::string const &) override
    {
        // 自定义赋值回调：内置选项已在 __assign 解析期内联触发过，此处仅处理用户选项
        if (!_Opt->bBuiltin__ && _Opt->fnOnSet__) { _Opt->fnOnSet__(_Opt->strValue); }

        // action 模板 → std::system 子进程命令
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

    /* ===== 逐行解释 ===== */

    /// @brief 解释执行一行：reset → 分词 → 继承 parse（含 hook 内联执行）→ finalize
    std::error_code interpret(std::string const &_Line, Errors &_Errors)
    {
        m_pModel__->m_args__.mgrName.doMutVisit([](Argument &a) { a.reset(); });
        m_pModel__->m_args__.mgrPos.doMutVisit([](Argument &a) { a.reset(); });
        m_pModel__->m_opts__.mgrValues.forEachValue([](OptionValue &o) { o.reset(); });
        _Errors.clearLast();

        try {
            Tokens_T toks = Lexer::tokensFromLine(_Line);

            this->EnvParser::parse(Envs_T{});
            bool fired = this->OptionParser::parse(toks);

            if (!fired) {
                if (this->OptionParser::isOptionsEnded()) {
                    this->ArgumentParser::parseNamedOnly(toks);
                    this->ArgumentParser::parsePositionalOnly(toks);
                }
                else {
                    this->ArgumentParser::parse(toks);
                }
            }

            for (auto const &t : toks) {
                if (util::starts_with(t, "-")) {
                    throw ErrorParse(
                        ParseError_E::UNKNOWN_OPTION,
                        "unrecognized option or argument: " + t);
                }
            }

            __finalize();
            m_bParsed__ = true;
            return make_error_code(ParseError_E::OK);
        }
        catch (ErrorParse const &e) {
            _Errors.setLast(e.code());
            return e.code();
        }
        catch (ErrorLexer const &e) {
            _Errors.setLast(e.code());
            return e.code();
        }
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
