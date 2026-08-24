#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "cmd_args/argument/argument_parser.h"
#include "cmd_args/base/util.h"
#include "cmd_args/config/config_store.h"
#include "cmd_args/context/lexer.h"
#include "cmd_args/env/env_builder.h"
#include "cmd_args/env/env_parser.h"
#include "cmd_args/option/option_builder.h"
#include "cmd_args/option/option_parser.h"

#include "model.h"
#include "parse_fwd.h"

CMD_ARGS_NAMESPACE_BEGIN

/*
    语法解析器（纯解析层）：把归一化后的 token 流分配给三类对象——
    1. 短回路选项（-?/--help 等，命中即回调并停止）
    2. 值选项（--long=value / -x value 等）
    3. 命名参数（--name=value）与方位参数（剩余 token 按序赋值）

    OptionParser.__assign：赋值+标记；内置选项回调内联触发，用户 action 记录顺序由 Executor 延迟执行。
    Interpreter：流式字符泵逐 Token 即时执行（不再继承钩子机制）。

    后置步骤（公开方法）：
    - ensureBuiltins()  内置选项注册（--help/--env/--env-set 等，懒注册一次）
    - finalize()        seed 环境兜底 + ${VAR} 展开 + save-config 写出
*/
class Compiler
{
public:
    Compiler(Model *_Data)
        : m_argParser__(&_Data->m_args__)
        , m_optParser__(&_Data->m_opts__)
        , m_envBuilder__(&_Data->m_envs__)
        , m_envParser__(&_Data->m_envs__)
        , m_pModel__(_Data)
    {}

    Compiler &set_program_name(Name_T name)
    {
        program_name = std::move(name);
        return *this;
    }

    Compiler &set_description(Desc_T _Desc)
    {
        description = std::move(_Desc);
        return *this;
    }

    // parse arguments（argv 完整，含 argv[0]；argv[0] 原样保留为程序名）
    void parse(int argc, char const *argv[])
    {
        if (argc <= 0 || !argv || !argv[0]) {
            program_name.clear();
        }
        else {
            program_name = argv[0];
        }

        Tokens_T real;
        if (argc > 1) { real = Lexer(argc - 1, argv + 1, nullptr).tokens(); }
        __parseTokens(real, {});
    }

    void parse(int argc, char const *argv[], char *envp[])
    {
        if (argc <= 0 || !argv || !argv[0]) {
            program_name.clear();
        }
        else {
            program_name = argv[0];
        }

        std::vector<std::string> vecEnv;
        for (int i = 0; envp && envp[i]; ++i) { vecEnv.emplace_back(envp[i]); }

        Tokens_T real;
        if (argc > 1) { real = Lexer(argc - 1, argv + 1, nullptr).tokens(); }
        __parseTokens(real, vecEnv);
    }

    // token 流形式的纯解析（不含 finalize，不含 action 执行）
    void parse(Tokens_T const &_Tokens, Envs_T const &_Env)
    {
        Tokens_T real = _Tokens;
        __parseTokens(real, _Env);
    }

    [[nodiscard]] std::error_code tryParse(int argc, char const *argv[]) noexcept
    {
        return __tryParse([&] { parse(argc, argv); });
    }

    [[nodiscard]] std::error_code tryParse(int argc, char const *argv[], char *envp[]) noexcept
    {
        return __tryParse([&] { parse(argc, argv, envp); });
    }

    [[nodiscard]] std::error_code tryParse(Tokens_T const &_Tokens, Envs_T const &_Env) noexcept
    {
        return __tryParse([&] { parse(_Tokens, _Env); });
    }

    bool isParsed(NameShort_T _Name) const { return m_optParser__.isParsed(_Name); }
    bool isParsed(NameLong_T const &_Name) const
    {
        return m_optParser__.isParsed(_Name) || m_argParser__.isParsed(_Name);
    }

    bool isFired() const noexcept { return m_bFired__; }
    Name_T const &get_program_name() const noexcept { return program_name; }

    template <typename T>
    T getOption(std::string const &_Name) const
    {
        return m_optParser__.getOption<T>(_Name);
    }
    template <typename T>
    T getArgument(std::string const &_Name) const
    {
        return m_argParser__.getArgument<T>(_Name);
    }

    void print_usage() const { std::cout << __descUsage(); }
    void print_help() const { std::cout << __descHelp(); }
    void print_env() const { std::cout << __descEnv(); }

    /// 还原 ArgumentData/OptionData 到内置默认值（Env 会话态保留）
    void resetData()
    {
        m_pModel__->m_args__.mgrName.doMutVisit([](Argument &a) { a.reset(); });
        m_pModel__->m_args__.mgrPos.doMutVisit([](Argument &a) { a.reset(); });
        m_pModel__->m_opts__.mgrValues.forEachValue([](OptionValue &o) { o.reset(); });
    }

    /// 内置选项注册（--help/--env/--env-set 等，懒注册一次）
    void ensureBuiltins()
    {
        if (!m_pModel__->m_bBuiltinOptions__ || m_pModel__->m_bBuiltinsRegistered__) return;
        m_pModel__->m_bBuiltinsRegistered__ = true;

        Model *m = m_pModel__;
        OptionBuilder ob(&m_pModel__->m_opts__);
        ob.addShortCircuit('?', "help", "show this help message", [m] {
            std::cout << __descHelpOf(*m);
        });
        ob.addShortCircuit('!', "env", "show environments of this program", [m] {
            std::cout << __descEnvOf(*m);
        });
        ob.addShortCircuit('\0', "env-apply", "write changed environments back to the OS", [m] {
            auto applied = EnvParser(&m->m_envs__).apply();
            for (auto const &a : applied) { std::cout << "applied " << a << "\n"; }
            std::cout << __descEnvOf(*m);
        });
        ob.addOption<std::string>("env-set", "set an environment variable, KEY=VALUE", "")
            ->bBuiltin__ = true;
        m_pModel__->m_opts__.mgrValues.mutFind("env-set")->onSet([m](std::string const &_V) {
            EnvBuilder b(&m->m_envs__);
            auto eq = _V.find('=');
            if (eq == std::string::npos) {
                b.setEnv(_V, "");
            }
            else {
                b.setEnv(_V.substr(0, eq), _V.substr(eq + 1));
            }
        });
        ob.addOption<std::string>("env-unset", "unset an environment variable, KEY", "")
            ->bBuiltin__ = true;
        m_pModel__->m_opts__.mgrValues.mutFind("env-unset")->onSet(
            [m](std::string const &_V) { EnvBuilder(&m->m_envs__).unsetEnv(_V); });

        ob.addOption<std::string>(
              "load-config", "load a yaml config file before later args, PATH", "")
            ->bBuiltin__ = true;
        m_pModel__->m_opts__.mgrValues.mutFind("load-config")->onSet(
            [m](std::string const &_Path) {
                auto text = config::read_file(_Path);
                if (!text) {
                    std::cerr << "(load-config) cannot read file: " << _Path << "\n";
                    return;
                }
                config::from_yaml(*m, *text);
            });
        ob.addOption<std::string>(
              "save-config", "write the resolved parameter/env state to a yaml file, PATH", "")
            ->bBuiltin__ = true;
        m_pModel__->m_opts__.mgrValues.mutFind("save-config")->onSet(
            [m](std::string const &_Path) { m->m_strSaveConfigPath__ = _Path; });
    }

    /// 解析后置：seed 环境兜底 + ${VAR} 展开 + save-config 写出
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

private:
    std::error_code __tryParse(std::function<void()> const &_Fn) const noexcept
    {
        try {
            _Fn();
            return make_error_code(ParseError_E::OK);
        }
        catch (ErrorParse const &e) {
            return e.code();
        }
        catch (ErrorLexer const &e) {
            return e.code();
        }
        catch (std::exception const &) {
            return make_error_code(ParseError_E::UNKNWON_ARGS);
        }
    }

    /// 纯解析流程（不含 finalize，不含 action 执行）
    void __parseTokens(Tokens_T &_Tokens, std::vector<std::string> const &_Env)
    {
        try {
            m_pModel__->m_strProgramName__ = program_name;
            m_envParser__.parse(_Env);
            ensureBuiltins();

            m_bFired__ = m_optParser__.parse(_Tokens);
            if (m_bFired__) return;

            if (m_optParser__.isOptionsEnded()) {
                m_argParser__.parseNamedOnly(_Tokens);
                m_argParser__.parsePositionalOnly(_Tokens);
                m_bFired__ = false;
                return;
            }

            m_argParser__.parse(_Tokens);

            for (auto const &t : _Tokens) {
                if (!util::starts_with(t, "-")) continue;
                throw ErrorParse(ParseError_E::UNKNOWN_OPTION, "unrecognized option or argument: " + t);
            }
        }
        catch (...) {
            // 解析失败：本次已记录的顺序一并作废——失败解析不残留待执行 action，
            // Model.hasAction() 为 false，后续解析不会触发陈旧 action
            m_pModel__->m_opts__.clearParseOrder();
            throw;
        }
    }

    void __seedEnvDefaults()
    {
        m_pModel__->m_args__.mgrName.doMutVisit([&](Argument &a) {
            if (a.getEnvName().empty() || a.hasValue() || a.isExternal()) return;
            if (m_envParser__.has(a.getEnvName())) { a.set(m_envParser__.get(a.getEnvName())); }
        });
        m_pModel__->m_args__.mgrPos.doMutVisit([&](Argument &a) {
            if (a.getEnvName().empty() || a.hasValue() || a.isExternal()) return;
            if (m_envParser__.has(a.getEnvName())) { a.set(m_envParser__.get(a.getEnvName())); }
        });
        m_pModel__->m_opts__.mgrValues.forEachValue([&](OptionValue &o) {
            if (o.strEnvName__.empty() || o.bExplicit__ || o.bExternal__) return;
            if (m_envParser__.has(o.strEnvName__)) { o.strValue = m_envParser__.get(o.strEnvName__); }
        });
    }

    void __expandEnvValues()
    {
        m_pModel__->m_args__.mgrName.doMutVisit([&](Argument &a) {
            if (a.hasValue()) { a.set(m_envParser__.expand(a.getValue())); }
        });
        m_pModel__->m_args__.mgrPos.doMutVisit([&](Argument &a) {
            if (a.hasValue()) { a.set(m_envParser__.expand(a.getValue())); }
        });
        m_pModel__->m_opts__.mgrValues.forEachValue([&](OptionValue &o) {
            if (!o.strValue.empty()) { o.strValue = m_envParser__.expand(o.strValue); }
        });
    }

    static std::string __descUsageOf(Model const &_M, Name_T const &_Program)
    {
        std::ostringstream oss;
        oss << "usage: " << (_Program.empty() ? _M.m_strProgramName__ : _Program)
            << " [arguments]";
        oss << _M.m_args__.mgrName.doGetUsage();
        oss << _M.m_args__.mgrPos.doGetUsage();
        oss << _M.m_opts__.usageString();
        oss << "\n";
        return oss.str();
    }

    static std::string __descHelpOf(Model const &_M)
    {
        std::ostringstream oss;
        oss << __descUsageOf(_M, _M.m_strProgramName__);
        _M.m_args__.mgrName.doGetHelp(oss, 0);
        _M.m_args__.mgrPos.doGetHelp(oss, 0);
        if (!_M.m_opts__.empty()) {
            oss << "\nOptions:\n";
            size_t maxLen = _M.m_opts__.calcDescMaxLength();
            _M.m_opts__.appendHelp(oss, maxLen);
        }
        oss << "\n";
        return oss.str();
    }

    static std::string __descEnvOf(Model &_M)
    {
        return EnvParser(&_M.m_envs__).envString(_M.m_strProgramName__);
    }

    std::string __descUsage() const
    {
        return __descUsageOf(*m_pModel__, program_name);
    }

    std::string __descHelp() const
    {
        std::ostringstream oss;
        oss << __descUsageOf(*m_pModel__, program_name);
        if (!description.empty()) { oss << "\n" << description << "\n"; }
        m_pModel__->m_args__.mgrName.doGetHelp(oss, 0);
        m_pModel__->m_args__.mgrPos.doGetHelp(oss, 0);
        if (!m_pModel__->m_opts__.empty()) {
            oss << "\nOptions:\n";
            size_t maxLen = m_pModel__->m_opts__.calcDescMaxLength();
            m_pModel__->m_opts__.appendHelp(oss, maxLen);
        }
        oss << "\n";
        oss << "environments: use -! / --env to list, --env-set KEY=VALUE to set, "
               "--env-apply to write back\n";
        return oss.str();
    }

    std::string __descEnv() const { return __descEnvOf(*m_pModel__); }

private:
    ArgumentParser m_argParser__;
    OptionParser   m_optParser__;
    EnvBuilder     m_envBuilder__;
    EnvParser      m_envParser__;

    Model        *m_pModel__ = nullptr;
    Name_T        program_name;
    Desc_T        description;
    bool          m_bFired__ = false;
};

CMD_ARGS_NAMESPACE_END
