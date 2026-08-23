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
#include "cmd_args/option/option_builder.h"
#include "cmd_args/option/option_parser.h"

#include "model.h"
#include "parse_fwd.h"

CMD_ARGS_NAMESPACE_BEGIN

/*
    语法解析器：把归一化后的 token 流分配给三类对象——
    1. 短回路选项（-?/--help 等，命中即回调并停止）
    2. 值选项（--long=value / -x value 等）
    3. 命名参数（--name=value）与方位参数（剩余 token 按序赋值）

    解析完成后统一做两件环境联动：
    - 环境变量兜底默认值（命令行 > 环境变量 > 内置默认）
    - ${VAR}/%VAR%/$VAR 值展开
*/
class Parser : private argument_parser, private option_parser
{
public:
    Parser(Model *_Data)
        : argument_parser(&_Data->m_args__)
        , option_parser(&_Data->m_opts__)
        , m_pModel__(_Data)
    {}

    Parser &set_program_name(Name_T name)
    {
        program_name = std::move(name);
        return *this;
    }

    Parser &set_description(Desc_T _Desc)
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
        __parse(real, {});
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
        __parse(real, vecEnv);
    }

    // token 流形式的解析（不含 argv[0]，供 Context/loop 复用）
    void parse(Tokens_T const &_Tokens, std::vector<std::string> const &_Env)
    {
        Tokens_T real = _Tokens;
        __parse(real, _Env);
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

    bool parsed(NameShort_T _Name) const { return option_parser::parsed(_Name); }
    bool parsed(NameLong_T const &_Name) const
    {
        return option_parser::parsed(_Name) || argument_parser::parsed(_Name);
    }

    bool fired() const noexcept { return m_bFired__; }
    Name_T const &get_program_name() const noexcept { return program_name; }

    template <typename T>
    T get_option(std::string const &_Name) const
    {
        return option_parser::get_option<T>(_Name);
    }
    template <typename T>
    T get_argument(std::string const &_Name) const
    {
        return argument_parser::get_argument<T>(_Name);
    }

    void print_usage() const { std::cout << __descUsage(); }
    void print_help() const { std::cout << __descHelp(); }
    void print_env() const { std::cout << __descEnv(); }

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

    void __parse(Tokens_T &_Tokens, std::vector<std::string> const &_Env)
    {
        m_pModel__->m_strProgramName__ = program_name;
        m_pModel__->m_envs__.parse(_Env);

        __ensureBuiltins();

        // 1. 短回路选项：命中即回调并停止
        m_bFired__ = option_parser::parse(_Tokens);
        if (m_bFired__) return;

        // 2. `--` 之后全部视为方位参数；之前的命名参数先消化
        if (option_parser::options_ended()) {
            argument_parser::parseNamedOnly(_Tokens);
            argument_parser::parsePositionalOnly(_Tokens);
            m_bFired__ = false;
            __finalize();
            return;
        }

        // 3. 命名参数 + 方位参数
        argument_parser::parse(_Tokens);

        // 4. 剩余 token：以 - 开头的一律视为未知选项
        for (auto const &t : _Tokens) {
            if (!util::starts_with(t, "-")) continue;
            throw ErrorParse(ParseError_E::UNKNOWN_OPTION, "unrecognized option or argument: " + t);
        }

        __finalize();
    }

    void __finalize()
    {
        __seedEnvDefaults();
        __expandEnvValues();

        // --save-config PATH：解析完成后把当前参数 + 环境状态写出为 YAML 配置
        if (!m_strSaveConfig__.empty()) {
            auto text = config::to_yaml(*m_pModel__);
            if (!config::write_file(m_strSaveConfig__, text)) {
                std::cerr << "(save-config) cannot write file: " << m_strSaveConfig__ << "\n";
            }
        }
    }

    /*
        内置选项（懒注册一次）：
        - ?/--help      展示帮助并短路
        - !/--env       展示环境变量并短路
        - --env-set     设置环境变量（KEY=VALUE），可新建
        - --env-unset   删除环境变量
        - --env-apply   把会话中的变更回写到操作系统环境
        - --load-config 按出现顺序导入 YAML 配置（可被后续 CLI 覆盖）
        - --save-config 解析完成后写出 YAML 配置
    */
    void __ensureBuiltins()
    {
        if (!m_pModel__->m_bBuiltinOptions__ || m_pModel__->m_bBuiltinsRegistered__) return;
        m_pModel__->m_bBuiltinsRegistered__ = true;

        Model *m = m_pModel__;
        OptionBuilder ob(&m_pModel__->m_opts__);
        ob.add_short_circuit('?', "help", "show this help message", [m] {
            std::cout << __descHelpOf(*m);
        });
        ob.add_short_circuit('!', "env", "show environments of this program", [m] {
            std::cout << __descEnvOf(*m);
        });
        ob.add_short_circuit('\0', "env-apply", "write changed environments back to the OS", [m] {
            auto applied = m->m_envs__.apply();
            for (auto const &a : applied) { std::cout << "applied " << a << "\n"; }
            std::cout << __descEnvOf(*m);
        });
        ob.add_option<std::string>("env-set", "set an environment variable, KEY=VALUE", "")
            ->bBuiltin__ = true;
        m_pModel__->m_opts__.options.mut_find("env-set")->on_set([m](std::string const &_V) {
            auto eq = _V.find('=');
            if (eq == std::string::npos) {
                m->m_envs__.set(_V, "");
            }
            else {
                m->m_envs__.set(_V.substr(0, eq), _V.substr(eq + 1));
            }
        });
        ob.add_option<std::string>("env-unset", "unset an environment variable, KEY", "")
            ->bBuiltin__ = true;
        m_pModel__->m_opts__.options.mut_find("env-unset")->on_set(
            [m](std::string const &_V) { m->m_envs__.unset(_V); });

        /* YAML 配置：第三类外界环境。出现在 CLI 中的顺序即导入顺序，
           后续 CLI token 仍可覆盖配置值（CLI > 配置 > 环境变量 > 默认值） */
        ob.add_option<std::string>(
              "load-config", "load a yaml config file before later args, PATH", "")
            ->bBuiltin__ = true;
        m_pModel__->m_opts__.options.mut_find("load-config")->on_set(
            [m](std::string const &_Path) {
                auto text = config::read_file(_Path);
                if (!text) {
                    std::cerr << "(load-config) cannot read file: " << _Path << "\n";
                    return;
                }
                config::from_yaml(*m, *text);
            });
        ob.add_option<std::string>(
              "save-config", "write the resolved parameter/env state to a yaml file, PATH", "")
            ->bBuiltin__ = true;
        m_pModel__->m_opts__.options.mut_find("save-config")->on_set(
            [this](std::string const &_Path) { m_strSaveConfig__ = _Path; });
    }

    /* 命令行 > 配置文件(YAML) > 环境变量 > 内置默认值 */
    void __seedEnvDefaults()
    {
        auto &env = m_pModel__->m_envs__;

        m_pModel__->m_args__.mgrName.doMutVisit([&](Argument &a) {
            if (a.getEnvName().empty() || a.hasValue() || a.isExternal()) return;
            auto const *p = env.find(a.getEnvName());
            if (p) {
                p->bReferenced__ = true;
                a.set(p->strValue);
            }
        });
        m_pModel__->m_args__.mgrPos.doMutVisit([&](Argument &a) {
            if (a.getEnvName().empty() || a.hasValue() || a.isExternal()) return;
            auto const *p = env.find(a.getEnvName());
            if (p) {
                p->bReferenced__ = true;
                a.set(p->strValue);
            }
        });
        m_pModel__->m_opts__.options.for_each_value([&](option_value &o) {
            if (o.strEnvName__.empty() || o.bExplicit__ || o.bExternal__) return;
            auto const *p = env.find(o.strEnvName__);
            if (p) {
                p->bReferenced__ = true;
                o.value = p->strValue;
            }
        });
    }

    void __expandEnvValues()
    {
        auto &env = m_pModel__->m_envs__;

        m_pModel__->m_args__.mgrName.doMutVisit([&](Argument &a) {
            if (a.hasValue()) { a.set(env.expand(a.getValue())); }
        });
        m_pModel__->m_args__.mgrPos.doMutVisit([&](Argument &a) {
            if (a.hasValue()) { a.set(env.expand(a.getValue())); }
        });
        m_pModel__->m_opts__.options.for_each_value([&](option_value &o) {
            if (!o.value.empty()) { o.value = env.expand(o.value); }
        });
    }

    static std::string __descUsageOf(Model const &_M, Name_T const &_Program)
    {
        std::ostringstream oss;
        oss << "usage: " << (_Program.empty() ? _M.m_strProgramName__ : _Program)
            << " [arguments]";
        oss << _M.m_args__.mgrName.doGetUsage();
        oss << _M.m_args__.mgrPos.doGetUsage();
        oss << _M.m_opts__.usage_string();
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
            size_t maxLen = _M.m_opts__.calc_desc_max_length();
            _M.m_opts__.appendHelp(oss, maxLen);
        }
        oss << "\n";
        return oss.str();
    }

    static std::string __descEnvOf(Model const &_M)
    {
        return _M.m_envs__.env_string(_M.m_strProgramName__);
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
            size_t maxLen = m_pModel__->m_opts__.calc_desc_max_length();
            m_pModel__->m_opts__.appendHelp(oss, maxLen);
        }
        oss << "\n";
        oss << "environments: use -! / --env to list, --env-set KEY=VALUE to set, "
               "--env-apply to write back\n";
        return oss.str();
    }

    std::string __descEnv() const { return __descEnvOf(*m_pModel__); }

private:
    Model *m_pModel__ = nullptr;
    Name_T program_name;
    Desc_T description;
    bool   m_bFired__ = false;
    std::string m_strSaveConfig__;  // --save-config 传入的路径，解析完成后写出
};

CMD_ARGS_NAMESPACE_END