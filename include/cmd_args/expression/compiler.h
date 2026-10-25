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
#include "cmd_args/expression/expression_recognizer.h"
#include "cmd_args/expression/lexer.h"
#include "cmd_args/env/env_builder.h"
#include "cmd_args/env/env_parser.h"
#include "cmd_args/option/option_builder.h"
#include "cmd_args/option/option_parser.h"

#include "cmd_args/context/builtin_options.h"
#include "cmd_args/context/describe.h"
#include "cmd_args/context/model.h"
#include "cmd_args/context/context_fwd.h"
#include "cmd_args/error/validation_error.h"
#include "cmd_args/expression/validation.h"

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
        : argParser__(&_Data->args)
        , optParser__(&_Data->opts)
        , envBuilder__(&_Data->envs)
        , envParser__(&_Data->envs)
        , pModel__(_Data)
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
    void parse(int argc, char const *argv[], char *envp[] = nullptr)
    {
        if (argc <= 0 || !argv || !argv[0]) {
            program_name.clear();
        }
        else {
            program_name = argv[0];
        }

        TokenStream_T real;
        if (argc > 1) { real = Lexer(argc - 1, argv + 1).scan(); }

        std::vector<std::string> vecEnv;
        if (envp) { for (int i = 0; envp[i]; ++i) { vecEnv.emplace_back(envp[i]); } }

        __parseTokens(real, vecEnv);
    }

    // token 流形式的纯解析（兼容旧接口：字符串流 → 旧管线）
    void parse(Tokens_T const &_Tokens, Envs_T const &_Env)
    {
        Tokens_T real = _Tokens;
        __parseTokensLegacy(real, _Env);
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

    bool isParsed(NameShort_T _Name) const { return optParser__.isParsed(_Name); }
    bool isParsed(NameLong_T const &_Name) const
    {
        return optParser__.isParsed(_Name) || argParser__.isParsed(_Name);
    }

    bool isFired() const noexcept { return bFired__; }
    Name_T const &get_program_name() const noexcept { return program_name; }

    template <typename T>
    T getOption(std::string const &_Name) const
    {
        return optParser__.getOption<T>(_Name);
    }
    template <typename T>
    T getArgument(std::string const &_Name) const
    {
        return argParser__.getArgument<T>(_Name);
    }

    void print_usage() const { std::cout << __descUsage(); }
    void print_help() const { std::cout << __descHelp(); }
    void print_env() const { std::cout << __descEnv(); }

    /// 还原 ArgumentData/OptionData 到内置默认值（Env 会话态保留）
    void resetData()
    {
        builtin_detail::resetModelData(pModel__);
    }

    /// 内置选项注册（--help/--env/--env-set 等，懒注册一次）
    void ensureBuiltins()
    {
        builtin_detail::registerBuiltinOptions(pModel__);
    }

    /// 解析后置：seed 环境兜底 + ${VAR} 展开 + 必选校验 + save-config 写出
    void finalize()
    {
        __seedEnvDefaults();
        __expandEnvValues();

        auto vr = validate_detail::validate(*pModel__);
        if (!vr.ok()) {
            throw ErrorValidation(static_cast<ValidationError_E>(vr.ec.value()), std::move(vr.msg));
        }

        if (!pModel__->strSaveConfigPath.empty()) {
            auto text = config::to_yaml(*pModel__);
            if (!config::write_file(pModel__->strSaveConfigPath, text)) {
                std::cerr << "(save-config) cannot write file: " << pModel__->strSaveConfigPath << "\n";
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
        catch (ErrorValidation const &e) {
            return e.code();
        }
        catch (std::exception const &) {
            return make_error_code(ParseError_E::UNKNWON_ARGS);
        }
    }

    /// 纯解析流程（旧管线：字符串 token 流，供 parse(Tokens_T,Envs_T) 兼容）
    void __parseTokensLegacy(Tokens_T &_Tokens, std::vector<std::string> const &_Env)
    {
        try {
            pModel__->strProgramName = program_name;
            envParser__.parse(_Env);
            ensureBuiltins();

            optParser__.setGraph(&pModel__->optionGraph);

            bFired__ = optParser__.parse(_Tokens);
            if (bFired__) return;

            if (optParser__.isOptionsEnded()) {
                argParser__.parseNamedOnly(_Tokens);
                argParser__.parsePositionalOnly(_Tokens);
                bFired__ = false;
            }
            else {
                argParser__.parse(_Tokens);
                pModel__->optionGraph.evaluate();
            }

            for (auto const &t : _Tokens) {
                if (!util::starts_with(t, "-")) continue;
                throw ErrorParse(ParseError_E::UNKNOWN_OPTION,
                    "unrecognized option or argument: " + t);
            }
        }
        catch (...) {
            pModel__->opts.clearParseOrder();
            pModel__->optionGraph.clear();
            throw;
        }
    }

    /// 纯解析流程：类型化 Token 流 → 归一化字符串 → 旧管线（语义等价，跳过 Recognizer 归约开销）
    void __parseTokens(TokenStream_T &_Tokens, std::vector<std::string> const &_Env)
    {
        Tokens_T legacy;
        for (auto const &t : _Tokens) { legacy.push_back(t.toText()); }
        __parseTokensLegacy(legacy, _Env);
    }

    /// @brief 分派单条 Expression 到对应 Parser（右值引用，避免拷贝）
    void __applyOne(ClassifiedExpr &&_E, std::vector<std::string> &_Pos)
    {
        switch (_E.kind) {
        case ExpressionKind_E::Option: {
            if (_E.bIsAction && !_E.name.empty()) {
                if (optParser__.fireAction(_E.name)) { bFired__ = true; }
                return;
            }
            optParser__.applyExpr(_E);
            return;
        }
        case ExpressionKind_E::Conditional:
        case ExpressionKind_E::Dependency:
            optParser__.applyRelation(_E);
            return;
        case ExpressionKind_E::Argument:
            argParser__.applyExpr(_E, _Pos);
            return;
        case ExpressionKind_E::Env:
            envParser__.applyExpr(_E);
            return;
        }
    }

    void __seedEnvDefaults()
    {
        pModel__->args.mgrName.doMutVisit([&](Argument &a) {
            if (a.getEnvName().empty() || a.hasValue() || a.isExternal()) return;
            if (envParser__.has(a.getEnvName())) { a.set(envParser__.get(a.getEnvName())); a.markEnvHit(); }
        });
        pModel__->args.mgrPos.doMutVisit([&](Argument &a) {
            if (a.getEnvName().empty() || a.hasValue() || a.isExternal()) return;
            if (envParser__.has(a.getEnvName())) { a.set(envParser__.get(a.getEnvName())); a.markEnvHit(); }
        });
        pModel__->opts.mgrValues.forEachValue([&](OptionValue &o) {
            if (o.strEnvName__.empty() || o.bExplicit__ || o.bExternal__) return;
            if (envParser__.has(o.strEnvName__)) {
                o.setValue(envParser__.get(o.strEnvName__));
                o.bEnvHit__ = true;
            }
        });
    }

    void __expandEnvValues()
    {
        pModel__->args.mgrName.doMutVisit([&](Argument &a) {
            if (!a.hasValue()) return;
            // 展开属于派生值：旁路写钩子，避免二次准入/覆盖错误
            a.property().setRaw(envParser__.expand(a.getValue()));
        });
        pModel__->args.mgrPos.doMutVisit([&](Argument &a) {
            if (!a.hasValue()) return;
            a.property().setRaw(envParser__.expand(a.getValue()));
        });
        pModel__->opts.mgrValues.forEachValue([&](OptionValue &o) {
            auto const value = o.getValue();
            if (!value.empty()) { o.property().setRaw(envParser__.expand(value)); }
        });
    }

    static std::string __descUsageOf(Model const &_M, Name_T const &_Program)
    {
        return describe_detail::usageOf(_M, _Program);
    }

    static std::string __descHelpOf(Model const &_M)
    {
        return describe_detail::helpOf(_M);
    }

    static std::string __descEnvOf(Model &_M)
    {
        return describe_detail::envOf(_M);
    }

    std::string __descUsage() const
    {
        return __descUsageOf(*pModel__, program_name);
    }

    std::string __descHelp() const
    {
        std::ostringstream oss;
        oss << __descUsageOf(*pModel__, program_name);
        if (!description.empty()) { oss << "\n" << description << "\n"; }
        pModel__->args.mgrName.doGetHelp(oss, 0);
        pModel__->args.mgrPos.doGetHelp(oss, 0);
        if (!pModel__->opts.empty()) {
            oss << "\nOptions:\n";
            size_t maxLen = pModel__->opts.calcDescMaxLength();
            pModel__->opts.appendHelp(oss, maxLen);
        }
        oss << "\n";
        oss << "environments: use -! / --env to list, --env-set KEY=VALUE to set, "
               "--env-apply to write back\n";
        return oss.str();
    }

    std::string __descEnv() const { return __descEnvOf(*pModel__); }

private:
    ArgumentParser argParser__;
    OptionParser   optParser__;
    EnvBuilder     envBuilder__;
    EnvParser      envParser__;

    Model        *pModel__ = nullptr;
    Name_T        program_name;
    Desc_T        description;
    bool          bFired__ = false;
};

CMD_ARGS_NAMESPACE_END
