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
    核心容器：
    - build<>() 构筑参数/选项
    - parse()   编译(Compiler) → hasAction 门控(Executor) → finalize
    - env 交互  set_env/unset_env/apply_env/env_string
    - once()/loop() 执行模式（委托 Interpreter 流式引擎）

    通用容器设计：
    - Model 是数据仓库（ArgumentData + OptionData + EnvData + Expression 统一视图）
    - Compiler 是编译引擎（Lexer → Parsers → Expression 分类入库）
    - Executor 是批量执行器（Model.hasAction() 为真时按 actionOrder 顺序触发 OptionAction）
    - Interpreter 是流式解释器（逐字符分词 → 即时分类执行）
    - 执行路径由调用入口（parse vs once/loop）和数据内容（hasAction）共同决定
*/
class Context : private Errors
{
public:
    Context(std::string _Name)
        : strPrjName__(_Name)
        , instData__()
        , compiler__(&instData__)
        , executor__(&instData__)
        , interp__(&instData__)
    {}

    /* ---------- 构筑 ---------- */

    template <class Select_T, typename... Args_T>
    Context &build(Args_T &&..._Args)
    {
        Builder(&instData__).build<Select_T>(std::forward<Args_T>(_Args)...);
        return *this;
    }

    template <class G, typename T = typename G::Element_T>
    Context &build(std::initializer_list<T> _Args)
    {
        Builder(&instData__).build(std::move(_Args));
        return *this;
    }

    BuildHelper build() { return { &instData__ }; }

    template <typename T>
    Context &addOption(char _Short, NameLong_T _Long, Desc_T _Help, T const &_Default = T{})
    {
        Builder(&instData__).addOption<T>(_Short, _Long, _Help, _Default);
        return *this;
    }

    template <typename T>
    Context &addOption(NameLong_T _Long, Desc_T _Help, T const &_Default = T{})
    {
        Builder(&instData__).addOption<T>(_Long, _Help, _Default);
        return *this;
    }

    Context &addFlag(char _Short, NameLong_T _Long, Desc_T _Help)
    {
        Builder(&instData__).addFlag(_Short, _Long, _Help);
        return *this;
    }

    Context &addFlag(NameLong_T _Long, Desc_T _Help)
    {
        Builder(&instData__).addFlag(_Long, _Help);
        return *this;
    }

    /* ---------- 数据访问 ---------- */

    /// @brief 获取底层 Model 引用（只读，用于 expressions() 等统一视图）
    auto model() const noexcept -> Model const & { return instData__; }

    Argument *argument(Name_T const &_Name)
    {
        return instData__.args.mgrName.mutGet(_Name);
    }
    Argument const *argument(Name_T const &_Name) const
    {
        return instData__.args.mgrName.doGet(_Name);
    }

    OptionValue *option(NameLong_T const &_Name)
    {
        return instData__.opts.mgrValues.mutFind(_Name);
    }
    OptionValue const *option(NameLong_T const &_Name) const
    {
        return instData__.opts.mgrValues.find(_Name);
    }

    template <typename T>
    T getArgument(Name_T const &_Name) const
    {
        auto const *p = instData__.args.mgrName.doGet(_Name);
        if (!p) { p = instData__.args.mgrPos.doGet(_Name); }
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
        auto const *p = instData__.opts.mgrValues.find(key);
        if (!p && key.size() == 1) { p = instData__.opts.mgrValues.find(key[0]); }
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
        if (instData__.opts.mgrValues.find(key)) { return true; }
        if (key.size() == 1 && instData__.opts.mgrValues.find(key[0])) { return true; }
        return false;
    }

    bool isParsed(NameShort_T _Name) const
    {
        return instData__.opts.mgrValues.find(_Name)
               && instData__.opts.mgrValues.find(_Name)->bExplicit__;
    }
    bool isParsed(NameLong_T const &_Name) const
    {
        if (auto const *p = instData__.opts.mgrValues.find(_Name)) { return p->bExplicit__; }
        if (auto const *p = instData__.args.mgrName.doGet(_Name)) { return p->hasValue(); }
        return false;
    }

    /* ---------- 环境变量交互 ---------- */

    bool has_env(Name_T const &_Key) const noexcept { return instData__.envs.doFind(_Key); }
    std::string get_env(Name_T const &_Key) const { return EnvParser(__envsMut()).get(_Key); }
    std::string get_env_or(Name_T const &_Key, std::string const &_Default = {}) const
    {
        return EnvParser(__envsMut()).getOr(_Key, _Default);
    }

    Context &set_env(Name_T _Key, std::string _Value)
    {
        EnvBuilder(&instData__.envs).setEnv(std::move(_Key), std::move(_Value));
        return *this;
    }

    Context &unset_env(Name_T const &_Key)
    {
        EnvBuilder(&instData__.envs).unsetEnv(_Key);
        return *this;
    }

    size_t apply_env()
    {
        return EnvParser(&instData__.envs).apply().size();
    }

    std::string expand_env(std::string const &_Text) const
    {
        return EnvParser(__envsMut()).expand(_Text);
    }

    std::string env_string() const { return EnvParser(__envsMut()).envString(strPrjName__); }
    void print_env() const { std::cout << env_string(); }

    Context &set_builtin_options(bool _On)
    {
        instData__.bBuiltinOptions = _On;
        return *this;
    }

    Context &set_exit_code(int _Code) noexcept
    {
        interp__.setExitCode(_Code);
        return *this;
    }
    int exit_code() const noexcept { return interp__.exitCode(); }

    /* ---------- 参数格式 / 快照 ---------- */

    parameter_schema schema() const { return config::schema_of(instData__); }

    std::string schema_string() const
    {
        std::ostringstream oss;
        auto               sch = schema();
        oss << "schema: " << (sch.program.empty() ? strPrjName__ : sch.program) << "\n";
        for (auto const &e : sch.entries) {
            oss << "  [" << e.kind << "] " << e.name;
            if (!e.type.empty()) { oss << " <" << e.type << ">"; }
            if (!e.value.empty()) { oss << " = \"" << e.value << "\""; }
            if (!e.env.empty()) { oss << "  env: " << e.env; }
            oss << "\n";
        }
        return oss.str();
    }

    parameter_snapshot snapshot() const { return config::snapshot_of(instData__); }
    void apply_snapshot(parameter_snapshot const &_Snap) { config::apply_snapshot(instData__, _Snap); }

    /* ---------- 配置脚本 ---------- */

    std::string to_yaml() const { return config::to_yaml(instData__); }

    bool save_config(std::string const &_Path) const
    {
        return config::write_file(_Path, config::to_yaml(instData__));
    }

    bool from_yaml(std::string const &_Text)
    {
        return config::from_yaml(instData__, _Text, static_cast<Errors *>(this));
    }

    bool load_config(std::string const &_Path)
    {
        auto text = config::read_file(_Path);
        if (!text) { setMsg("config: cannot read file: " + _Path); return false; }
        return from_yaml(*text);
    }

    std::string env_to_yaml() const { return config::env_to_yaml(instData__); }

    bool yaml_to_env(std::string const &_Text)
    {
        return config::yaml_to_env(instData__, _Text, static_cast<Errors *>(this));
    }

    /* ---------- 执行状态 ---------- */

    Context &reset()
    {
        compiler__.resetData();
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

    [[noreturn]] void once() { interp__.once(); }
    void              loop() { compiler__.ensureBuiltins(); interp__.loop(static_cast<Errors &>(*this)); }

    using Errors::dump;
    using Errors::getLast;
    using Errors::getLastMsg;
    using Errors::getLastVal;
    using Errors::isOk;

private:
    EnvData *__envsMut() const
    {
        return &const_cast<Model &>(instData__).envs;
    }

    /* 批量解析路径：编译(Compiler) → hasAction 门控(Executor) → finalize */
    void __doParse(int _Argc, char const *_Argv[], char *_Envp[])
    {
        if (_Argc > 0 && _Argv[0]) {
            instData__.strProgramName = _Argv[0];
        }
        else {
            instData__.strProgramName = strPrjName__;
        }

        Tokens_T toks;
        if (_Argc > 1) {
            Lexer lex(_Argc - 1, _Argv + 1, nullptr);
            toks = lex.tokens();
        }

        compiler__.set_program_name(instData__.strProgramName);
        compiler__.set_description(strPrjName__);

        Envs_T envs;
        if (_Envp) { for (int i = 0; _Envp[i]; ++i) { envs.emplace_back(_Envp[i]); } }

        compiler__.ensureBuiltins();
        compiler__.parse(toks, envs);      // 编译：Tokens → Expression 分类入库 Model

        if (instData__.hasAction())        // Model.hasAction() 门控
        {
            executor__.execute();          // 按 actionOrder 顺序执行
        }

        compiler__.finalize();             // seed 环境兜底 + 展开 + save-config

        interp__.markParsed();
        bFired__ = compiler__.isFired();
    }

    void __doParse(Tokens_T const &_Tokens, Envs_T const &_Env)
    {
        compiler__.set_program_name(instData__.strProgramName.empty()
                                          ? strPrjName__
                                          : instData__.strProgramName);
        compiler__.set_description(strPrjName__);

        compiler__.ensureBuiltins();
        compiler__.parse(_Tokens, _Env);   // 编译：Tokens → Expression 分类入库 Model

        if (instData__.hasAction())        // Model.hasAction() 门控
        {
            executor__.execute();          // 按 actionOrder 顺序执行
        }

        compiler__.finalize();             // seed 环境兜底 + 展开 + save-config

        interp__.markParsed();
        bFired__ = compiler__.isFired();
    }

private:
    std::string  strPrjName__;
    Model        instData__;
    Compiler     compiler__;
    Executor     executor__;
    Interpreter  interp__;
    bool         bFired__ = false;
};

CMD_ARGS_NAMESPACE_END
