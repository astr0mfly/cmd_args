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
#include <sstream>
#include <string>
#include <type_traits>

#include "cmd_args/base/def.h"
#include "cmd_args/base/util.h"

#include "cmd_args/config/config_store.h"
#include "cmd_args/env/env_builder.h"
#include "cmd_args/env/env_parser.h"
#include "cmd_args/error/errors.h"
#include "cmd_args/error/interpreter_error.h"
#include "cmd_args/error/lexer_error.h"
#include "cmd_args/error/parser_error.h"

#include "builder.h"
#include "compiler.h"
#include "executor.h"
#include "interpreter.h"
#include "lexer.h"
#include "model.h"

CMD_ARGS_NAMESPACE_BEGIN

/*
    核心服务对象：
    - build<>() 构筑参数/选项
    - parse()   词法(Lexer) → 纯解析(Compiler) → action 执行(Executor) → 写入 Model
    - env 交互  set_env/unset_env/apply_env/env_string
    - once()/loop() 执行模式（委托 Interpreter）

    三层架构：
    - Compiler（纯解析层）：继承 ArgumentParser+OptionParser+EnvParser，纯赋值，不触发 OptionAction
    - Executor（执行器）：解析完成后按解析顺序执行 OptionAction
    - Interpreter（实时引擎）：继承三 parser，继承 Parser 的 virtual hook 实现逐 Token 内联执行
*/
class Context : private Errors
{
public:
    Context(std::string _Name)
        : m_strPrjName__(_Name)
        , m_instData__()
        , m_compiler__(&m_instData__)
        , m_executor__(&m_instData__)
        , m_interp__(&m_instData__)
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
    Context &addOption(char _Short, NameLong_T _Long, Desc_T _Help, T const &_Default = T{})
    {
        Builder(&m_instData__).addOption<T>(_Short, _Long, _Help, _Default);
        return *this;
    }

    template <typename T>
    Context &addOption(NameLong_T _Long, Desc_T _Help, T const &_Default = T{})
    {
        Builder(&m_instData__).addOption<T>(_Long, _Help, _Default);
        return *this;
    }

    Context &addFlag(char _Short, NameLong_T _Long, Desc_T _Help)
    {
        Builder(&m_instData__).addFlag(_Short, _Long, _Help);
        return *this;
    }

    Context &addFlag(NameLong_T _Long, Desc_T _Help)
    {
        Builder(&m_instData__).addFlag(_Long, _Help);
        return *this;
    }

    /* ---------- 数据访问 ---------- */

    Argument *argument(Name_T const &_Name)
    {
        return m_instData__.m_args__.mgrName.mutGet(_Name);
    }
    Argument const *argument(Name_T const &_Name) const
    {
        return m_instData__.m_args__.mgrName.doGet(_Name);
    }

    OptionValue *option(NameLong_T const &_Name)
    {
        return m_instData__.m_opts__.mgrValues.mutFind(_Name);
    }
    OptionValue const *option(NameLong_T const &_Name) const
    {
        return m_instData__.m_opts__.mgrValues.find(_Name);
    }

    template <typename T>
    T getArgument(Name_T const &_Name) const
    {
        auto const *p = m_instData__.m_args__.mgrName.doGet(_Name);
        if (!p) { p = m_instData__.m_args__.mgrPos.doGet(_Name); }
        if (!p) {
            throw ErrorParse(ParseError_E::UNKNWON_ARGS, "argument not found: " + _Name);
        }
        auto const &type = p->getType();
        if (!type.empty() && type != util::type_string<T>()) {
            throw ErrorParse(ParseError_E::UNKNWON_ARGS, "argument type mismatch...");
        }
        return util::parse_value<T>(p->getValue());
    }

    template <typename T>
    T getOption(NameLong_T const &_Name) const
    {
        auto key = _Name;
        if (util::starts_with(key, "--")) { key = key.substr(2); }
        auto const *p = m_instData__.m_opts__.mgrValues.find(key);
        if (!p && key.size() == 1) { p = m_instData__.m_opts__.mgrValues.find(key[0]); }
        if (!p) { throw ErrorParse(ParseError_E::UNKNOWN_OPTION, "option not found: " + key); }
        if (p->strType == "bool") {
            if constexpr (std::is_same_v<T, bool>) { return util::parse_bool(p->strValue); }
            throw ErrorParse(ParseError_E::UNKNOWN_OPTION, "option type mismatch: " + key);
        }
        if (p->strType != util::type_string<T>()) {
            throw ErrorParse(ParseError_E::UNKNOWN_OPTION, "option type mismatch...");
        }
        return util::parse_value<T>(p->strValue);
    }

    bool hasOption(NameLong_T const &_Name) const noexcept
    {
        auto key = _Name;
        if (util::starts_with(key, "--")) { key = key.substr(2); }
        if (m_instData__.m_opts__.mgrValues.find(key)) { return true; }
        if (key.size() == 1 && m_instData__.m_opts__.mgrValues.find(key[0])) { return true; }
        return false;
    }

    bool isParsed(NameShort_T _Name) const
    {
        return m_instData__.m_opts__.mgrValues.find(_Name)
               && m_instData__.m_opts__.mgrValues.find(_Name)->bExplicit__;
    }
    bool isParsed(NameLong_T const &_Name) const
    {
        if (auto const *p = m_instData__.m_opts__.mgrValues.find(_Name)) { return p->bExplicit__; }
        if (auto const *p = m_instData__.m_args__.mgrName.doGet(_Name)) { return p->hasValue(); }
        return false;
    }

    /* ---------- 环境变量交互 ---------- */

    bool has_env(Name_T const &_Key) const noexcept { return m_instData__.m_envs__.doFind(_Key); }
    std::string get_env(Name_T const &_Key) const { return EnvParser(__envsMut()).get(_Key); }
    std::string get_env_or(Name_T const &_Key, std::string const &_Default = {}) const
    {
        return EnvParser(__envsMut()).getOr(_Key, _Default);
    }

    Context &set_env(Name_T _Key, std::string _Value)
    {
        EnvBuilder(&m_instData__.m_envs__).setEnv(std::move(_Key), std::move(_Value));
        return *this;
    }

    Context &unset_env(Name_T const &_Key)
    {
        EnvBuilder(&m_instData__.m_envs__).unsetEnv(_Key);
        return *this;
    }

    size_t apply_env()
    {
        return EnvParser(&m_instData__.m_envs__).apply().size();
    }

    std::string expand_env(std::string const &_Text) const
    {
        return EnvParser(__envsMut()).expand(_Text);
    }

    std::string env_string() const { return EnvParser(__envsMut()).envString(m_strPrjName__); }
    void print_env() const { std::cout << env_string(); }

    Context &set_builtin_options(bool _On)
    {
        m_instData__.m_bBuiltinOptions__ = _On;
        return *this;
    }

    Context &set_exit_code(int _Code) noexcept
    {
        m_interp__.setExitCode(_Code);
        return *this;
    }
    int exit_code() const noexcept { return m_interp__.exitCode(); }

    /* ---------- 参数格式 / 快照 ---------- */

    parameter_schema schema() const { return config::schema_of(m_instData__); }

    std::string schema_string() const
    {
        std::ostringstream oss;
        auto               sch = schema();
        oss << "schema: " << (sch.program.empty() ? m_strPrjName__ : sch.program) << "\n";
        for (auto const &e : sch.entries) {
            oss << "  [" << e.kind << "] " << e.name;
            if (!e.type.empty()) { oss << " <" << e.type << ">"; }
            if (!e.value.empty()) { oss << " = \"" << e.value << "\""; }
            if (!e.env.empty()) { oss << "  env: " << e.env; }
            oss << "\n";
        }
        return oss.str();
    }

    parameter_snapshot snapshot() const { return config::snapshot_of(m_instData__); }
    void apply_snapshot(parameter_snapshot const &_Snap) { config::apply_snapshot(m_instData__, _Snap); }

    /* ---------- 配置脚本 ---------- */

    std::string to_yaml() const { return config::to_yaml(m_instData__); }

    bool save_config(std::string const &_Path) const
    {
        return config::write_file(_Path, config::to_yaml(m_instData__));
    }

    bool from_yaml(std::string const &_Text)
    {
        return config::from_yaml(m_instData__, _Text, static_cast<Errors *>(this));
    }

    bool load_config(std::string const &_Path)
    {
        auto text = config::read_file(_Path);
        if (!text) { setMsg("config: cannot read file: " + _Path); return false; }
        return from_yaml(*text);
    }

    std::string env_to_yaml() const { return config::env_to_yaml(m_instData__); }

    bool yaml_to_env(std::string const &_Text)
    {
        return config::yaml_to_env(m_instData__, _Text, static_cast<Errors *>(this));
    }

    /* ---------- 执行状态 ---------- */

    Context &reset()
    {
        m_compiler__.resetData();
        clearLast();
        return *this;
    }

    /* ---------- 解析（批量模式：纯解析 + Executor 执行 action + finalize） ---------- */

    Context &parse(int _Argc, char const *_Argv[], char *_Envp[])
    {
        try {
            __doParse(_Argc, _Argv, _Envp);
        }
        catch (ErrorParse const &e) { setLast(e.code()); throw; }
        catch (ErrorLexer const &e) { setLast(e.code()); throw; }
        return *this;
    }

    Context &parse(std::error_code &_Ec, int _Argc, char const *_Argv[], char *_Envp[]) noexcept
    {
        try {
            __doParse(_Argc, _Argv, _Envp);
            _Ec = make_error_code(ParseError_E::OK);
        }
        catch (ErrorParse const &e) { _Ec = e.code(); setLast(e.code()); }
        catch (ErrorLexer const &e) { _Ec = e.code(); setLast(e.code()); }
        catch (std::exception const &ex) { setMsg(ex.what()); _Ec = make_error_code(ParseError_E::UNKNWON_ARGS); }
        return *this;
    }

    Context &parse(Tokens_T const &_Tokens, Envs_T const &_Env)
    {
        try {
            __doParse(_Tokens, _Env);
        }
        catch (ErrorParse const &e) { setLast(e.code()); throw; }
        catch (ErrorLexer const &e) { setLast(e.code()); throw; }
        return *this;
    }

    Context &parse(std::error_code &_Ec, Tokens_T const &_Tokens, Envs_T const &_Env) noexcept
    {
        try {
            __doParse(_Tokens, _Env);
            _Ec = make_error_code(ParseError_E::OK);
        }
        catch (ErrorParse const &e) { _Ec = e.code(); setLast(e.code()); }
        catch (ErrorLexer const &e) { _Ec = e.code(); setLast(e.code()); }
        catch (std::exception const &ex) { setMsg(ex.what()); _Ec = make_error_code(ParseError_E::UNKNWON_ARGS); }
        return *this;
    }

    /* ---------- 执行模式（委托 Interpreter） ---------- */

    [[noreturn]] void once() { m_interp__.once(); }
    void              loop() { m_compiler__.ensureBuiltins(); m_interp__.loop(static_cast<Errors &>(*this)); }

    using Errors::dump;
    using Errors::getLast;
    using Errors::getLastMsg;
    using Errors::getLastVal;
    using Errors::isOk;

private:
    EnvData *__envsMut() const
    {
        return &const_cast<Model &>(m_instData__).m_envs__;
    }

    /* 批量解析路径：纯解析 → action 执行 → finalize */
    void __doParse(int _Argc, char const *_Argv[], char *_Envp[])
    {
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

        m_compiler__.set_program_name(m_instData__.m_strProgramName__);
        m_compiler__.set_description(m_strPrjName__);

        Envs_T envs;
        if (_Envp) { for (int i = 0; _Envp[i]; ++i) { envs.emplace_back(_Envp[i]); } }

        m_compiler__.ensureBuiltins();
        m_compiler__.parse(toks, envs);     // 纯解析（含 OptionParser + ArgumentParser + 短回路）
        m_executor__.execute();             // 按解析顺序执行 OptionAction
        m_compiler__.finalize();            // seed 环境兜底 + 展开 + save-config

        m_interp__.markParsed();
        m_bFired__ = m_compiler__.isFired();
    }

    void __doParse(Tokens_T const &_Tokens, Envs_T const &_Env)
    {
        m_compiler__.set_program_name(m_instData__.m_strProgramName__.empty()
                                          ? m_strPrjName__
                                          : m_instData__.m_strProgramName__);
        m_compiler__.set_description(m_strPrjName__);

        m_compiler__.ensureBuiltins();
        m_compiler__.parse(_Tokens, _Env);
        m_executor__.execute();
        m_compiler__.finalize();

        m_interp__.markParsed();
        m_bFired__ = m_compiler__.isFired();
    }

private:
    std::string  m_strPrjName__;
    Model        m_instData__;
    Compiler     m_compiler__;
    Executor     m_executor__;
    Interpreter  m_interp__;
    bool         m_bFired__ = false;
};

CMD_ARGS_NAMESPACE_END
