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

#include <iostream>
#include <string>
#include <type_traits>

#include "cmd_args/base/def.h"
#include "cmd_args/base/util.h"

#include "cmd_args/error/errors.h"
#include "cmd_args/error/interpreter_error.h"
#include "cmd_args/error/lexer_error.h"
#include "cmd_args/error/parser_error.h"

#include "builder.h"
#include "interpreter.h"
#include "lexer.h"
#include "model.h"
#include "parser.h"

CMD_ARGS_NAMESPACE_BEGIN

/*
    核心服务对象：
    - build<>() 构筑参数/选项
    - parse()   词法(Lexer) -> 语法(Parser) -> 写入 Model，错误进 Errors 堆栈
    - env 交互  set_env/unset_env/apply_env/env_string
    - once()/loop() 执行模式
*/
class Context : public Interpreter, private Errors
{
public:
    Context(std::string _Name)
        : m_strPrjName__(_Name)
    {}

    /* ---------- 构筑 ---------- */

    template <class Select_T, typename... Args_T>
    Context &build(Args_T &&..._Args)
    {
        Builder(&m_instData__).build<Select_T>(std::forward<Args_T>(_Args)...);

        return *this;
    }

    template <class G, typename T = typename G::Element_T>
    Context &build(std::initializer_list<T> _Args)
    {
        Builder(&m_instData__).build(std::move(_Args));

        return *this;
    }

    BuildHelper build() { return { &m_instData__ }; }

    template <typename T>
    Context &add_option(char _Short, NameLong_T _Long, Desc_T _Help, T const &_Default = T{})
    {
        Builder(&m_instData__).add_option<T>(_Short, _Long, _Help, _Default);
        return *this;
    }

    template <typename T>
    Context &add_option(NameLong_T _Long, Desc_T _Help, T const &_Default = T{})
    {
        Builder(&m_instData__).add_option<T>(_Long, _Help, _Default);
        return *this;
    }

    Context &add_flag(char _Short, NameLong_T _Long, Desc_T _Help)
    {
        Builder(&m_instData__).add_flag(_Short, _Long, _Help);
        return *this;
    }

    Context &add_flag(NameLong_T _Long, Desc_T _Help)
    {
        Builder(&m_instData__).add_flag(_Long, _Help);
        return *this;
    }

    /* ---------- 数据访问 ---------- */

    /* 命中的 Argument 句柄（可再 .env("VAR") / .type<T>()） */
    Argument *argument(Name_T const &_Name)
    {
        return m_instData__.m_args__.mgrName.mutGet(_Name);
    }
    Argument const *argument(Name_T const &_Name) const
    {
        return m_instData__.m_args__.mgrName.doGet(_Name);
    }

    /* 命中的选项句柄（可再 .env("VAR") / .on_set(...)） */
    option_value *option(NameLong_T const &_Name)
    {
        return m_instData__.m_opts__.options.mut_find(_Name);
    }
    option_value const *option(NameLong_T const &_Name) const
    {
        return m_instData__.m_opts__.options.find(_Name);
    }

    template <typename T>
    T get_argument(Name_T const &_Name) const
    {
        auto const *p = m_instData__.m_args__.mgrName.doGet(_Name);
        if (!p) { p = m_instData__.m_args__.mgrPos.doGet(_Name); }
        if (!p) {
            throw ErrorParse(ParseError_E::UNKNWON_ARGS, "argument not found: " + _Name);
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

    template <typename T>
    T get_option(NameLong_T const &_Name) const
    {
        auto key = _Name;
        if (util::starts_with(key, "--")) { key = key.substr(2); }

        auto const *p = m_instData__.m_opts__.options.find(key);
        if (!p && key.size() == 1) { p = m_instData__.m_opts__.options.find(key[0]); }
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

    bool has_option(NameLong_T const &_Name) const noexcept
    {
        auto key = _Name;
        if (util::starts_with(key, "--")) { key = key.substr(2); }
        if (m_instData__.m_opts__.options.find(key)) { return true; }
        if (key.size() == 1 && m_instData__.m_opts__.options.find(key[0])) { return true; }
        return false;
    }

    bool parsed(NameShort_T _Name) const
    {
        return m_instData__.m_opts__.options.find(_Name)
               && m_instData__.m_opts__.options.find(_Name)->bExplicit__;
    }
    bool parsed(NameLong_T const &_Name) const
    {
        if (auto const *p = m_instData__.m_opts__.options.find(_Name)) { return p->bExplicit__; }
        if (auto const *p = m_instData__.m_args__.mgrName.doGet(_Name)) { return p->hasValue(); }
        return false;
    }

    /* ---------- 环境变量交互 ---------- */

    bool has_env(Name_T const &_Key) const noexcept { return m_instData__.m_envs__.has(_Key); }
    std::string get_env(Name_T const &_Key) const { return m_instData__.m_envs__.get(_Key); }
    std::string get_env_or(Name_T const &_Key, std::string const &_Default = {}) const
    {
        return m_instData__.m_envs__.get_or(_Key, _Default);
    }

    Context &set_env(Name_T _Key, std::string _Value)
    {
        m_instData__.m_envs__.set(std::move(_Key), std::move(_Value));
        return *this;
    }

    Context &unset_env(Name_T const &_Key)
    {
        m_instData__.m_envs__.unset(_Key);
        return *this;
    }

    /* 把会话中的变更写回操作系统环境变量，返回实际写回的条目数 */
    size_t apply_env()
    {
        return m_instData__.m_envs__.apply().size();
    }

    std::string expand_env(std::string const &_Text) const
    {
        return m_instData__.m_envs__.expand(_Text);
    }

    std::string env_string() const { return m_instData__.m_envs__.env_string(m_strPrjName__); }

    void print_env() const { std::cout << env_string(); }

    Context &set_builtin_options(bool _On)
    {
        m_instData__.m_bBuiltinOptions__ = _On;
        return *this;
    }

    Context &set_exit_code(int _Code) noexcept
    {
        m_nExitCode__ = _Code;
        return *this;
    }
    int exit_code() const noexcept { return m_nExitCode__; }

    /* ---------- 解析 ---------- */

    Context &parse(int _Argc, char const *_Argv[], char *_Envp[])
    {
        try {
            __doParse(_Argc, _Argv, _Envp);
        }
        catch (ErrorParse const &e) {
            setLast(e.code());
            throw;
        }
        catch (ErrorLexer const &e) {
            setLast(e.code());
            throw;
        }

        return *this;
    }

    Context &parse(std::error_code &_Ec, int _Argc, char const *_Argv[], char *_Envp[]) noexcept
    {
        try {
            __doParse(_Argc, _Argv, _Envp);
            _Ec = make_error_code(ParseError_E::OK);
        }
        catch (ErrorParse const &e) {
            _Ec = e.code();
            setLast(e.code());
        }
        catch (ErrorLexer const &e) {
            _Ec = e.code();
            setLast(e.code());
        }
        catch (std::exception const &ex) {
            setMsg(ex.what());
            _Ec = make_error_code(ParseError_E::UNKNWON_ARGS);
        }

        return *this;
    }

    /* token 流解析（loop 模式复用；不做 argv[0] 剥离） */
    Context &parse(Tokens_T const &_Tokens, Envs_T const &_Env)
    {
        try {
            __doParse(_Tokens, _Env);
        }
        catch (ErrorParse const &e) {
            setLast(e.code());
            throw;
        }
        catch (ErrorLexer const &e) {
            setLast(e.code());
            throw;
        }

        return *this;
    }

    Context &parse(std::error_code &_Ec, Tokens_T const &_Tokens, Envs_T const &_Env) noexcept
    {
        try {
            __doParse(_Tokens, _Env);
            _Ec = make_error_code(ParseError_E::OK);
        }
        catch (ErrorParse const &e) {
            _Ec = e.code();
            setLast(e.code());
        }
        catch (ErrorLexer const &e) {
            _Ec = e.code();
            setLast(e.code());
        }
        catch (std::exception const &ex) {
            setMsg(ex.what());
            _Ec = make_error_code(ParseError_E::UNKNWON_ARGS);
        }

        return *this;
    }

    /* ---------- 执行模式 ---------- */

    /*
        once：在解析完毕后以设定退出码退出进程
        （内置 -?/--help、-!/--env 已在 parse 时打印输出）
    */
    Context &once()
    {
        if (!m_bParsed__) { throw ErrorInterp(InterpError_E::STOPPED); }
        std::exit(m_nExitCode__);

        return *this;
    }

    /*
        loop：常驻后台，标准输入逐行执行
        exit / quit / EOF 退出；每行独立解析，错误进 Errors 堆栈
    */
    Context &loop()
    {
        if (!m_bParsed__) {
            // 首次解析装载进程环境，之后逐行复用
            m_bParsed__ = true;
        }

        std::cout << "cmd_args> " << std::flush;
        std::string line;
        while (std::getline(std::cin, line)) {
            auto cmd = util::trim(line);
            if (cmd.empty()) {
                std::cout << "cmd_args> " << std::flush;
                continue;
            }
            if (cmd == "exit" || cmd == "quit" || cmd == "exit()") break;

            std::error_code ec;
            parse(ec, Lexer::tokensFromLine(cmd), {});
            if (ec) {
                std::cout << "(parse error) " << ec.message() << "\n";
                std::cout << dump();
            }
            std::cout << "cmd_args> " << std::flush;
        }
        std::cout << "\n";

        return *this;
    }

    using Errors::dump;
    using Errors::getLast;
    using Errors::getLastMsg;
    using Errors::getLastVal;
    using Errors::isOk;

private:
    void __doParse(int _Argc, char const *_Argv[], char *_Envp[])
    {
        // argv[0] 是程序名（可能为含盘符/路径的完整路径），不是命令行语法，
        // 原样保留为程序名；只对 argv[1..] 做词法扫描
        if (_Argc > 0 && _Argv[0]) {
            m_instData__.m_strProgramName__ = _Argv[0];
        }
        else {
            m_instData__.m_strProgramName__ = m_strPrjName__;
        }

        Tokens_T toks;
        if (_Argc > 1) {
            Lexer lex(_Argc - 1, _Argv + 1, nullptr);
            toks = lex.tokens();
        }

        Parser parser(&m_instData__);
        parser.set_program_name(m_instData__.m_strProgramName__);
        parser.set_description(m_strPrjName__);

        Envs_T envs;
        if (_Envp) {
            for (int i = 0; _Envp[i]; ++i) { envs.emplace_back(_Envp[i]); }
        }
        parser.parse(toks, envs);

        m_bParsed__ = true;
        m_bFired__  = parser.fired();
    }

    void __doParse(Tokens_T const &_Tokens, Envs_T const &_Env)
    {
        Parser parser(&m_instData__);
        parser.set_program_name(m_instData__.m_strProgramName__.empty()
                                    ? m_strPrjName__
                                    : m_instData__.m_strProgramName__);
        parser.set_description(m_strPrjName__);
        parser.parse(_Tokens, _Env);

        m_bParsed__ = true;
        m_bFired__  = parser.fired();
    }

private:
    std::string m_strPrjName__;
    Model       m_instData__;
    int         m_nExitCode__ = 0;
    bool        m_bParsed__   = false;
    bool        m_bFired__    = false;
};

CMD_ARGS_NAMESPACE_END