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

#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#include "cmd_args/base/def.h"
#include "cmd_args/base/util.h"

#include "cmd_args/config/config_store.h"
#include "cmd_args/env/env_builder.h"
#include "cmd_args/env/env_parser.h"
#include "cmd_args/error/errors.h"
#include "cmd_args/error/interpreter_error.h"
#include "cmd_args/error/lexer_error.h"
#include "cmd_args/error/parser_error.h"

#include "cmd_args/expression/lexer.h"

#include "cmd_args/context/subcommand.h"

#include "builtin_options.h"
#include "builder.h"
#include "declarations.h"
#include "model.h"

CMD_ARGS_NAMESPACE_BEGIN

/*
    上下文抽象基类：
    - 持有 Model 数据仓库与全部引擎无关能力（构筑/查询/环境变量/YAML 配置/schema）
    - parse() 公共包装负责 argv 归一化（程序名探测 + Lexer 分词 + envp 收集）
      与双通道错误处理（抛异常 / 错误码），引擎管线下沉给纯虚 _doRunPipeline
    - once()/loop() 为执行模式纯虚接口，由 ContextStep / ContextRuntime / 兼容版 Context 各自实现
*/
class ContextBase : private Errors
{
public:
    explicit ContextBase(std::string _Name)
        : strPrjName_(_Name)
    {}
    virtual ~ContextBase() = default;

    /* ---------- 构筑 ---------- */

    template <class Select_T, typename... Args_T>
    ContextBase &build(Args_T &&..._Args)
    {
        Builder(&instData_).build<Select_T>(std::forward<Args_T>(_Args)...);
        return *this;
    }

    template <class G, typename T = typename G::Element_T>
    ContextBase &build(std::initializer_list<T> _Args)
    {
        Builder(&instData_).build(std::move(_Args));
        return *this;
    }

    BuildHelper build() { return { &instData_ }; }

    /* ---------- 声明式批量注册 ---------- */

    /**
     * @brief 批量声明参数/选项/标志（声明式入口）
     * @tparam Args_T 参数包：ArgumentNamed / ArgumentPosition / OptionDef<T> / FlagDef
     * @param _Args 声明项
     * @return Self_T& 支持链式调用
     */
    template <typename... Args_T>
    ContextBase &declare(Args_T &&..._Args)
    {
        Builder(&instData_).declare(std::forward<Args_T>(_Args)...);
        return *this;
    }

    /* ---------- 分组声明 ---------- */

    /**
     * @brief 分组声明选项（带分组标签的批量注册）
     * @tparam Args_T 参数包：OptionDef<T> / FlagDef
     * @param _GroupName 分组名称（会出现在帮助文档中）
     * @param _Args 分组内的选项声明
     * @return Self_T& 支持链式调用
     */
    template <typename... Args_T>
    ContextBase &group(Name_T _GroupName, Args_T &&..._Args)
    {
        // 1. 先提取分组成员名（在 declare 移动之前）
        OptionGroup grp;
        grp.name = std::move(_GroupName);
        _collectGroupMembers(grp, std::forward<Args_T>(_Args)...);
        instData_.groups_.push_back(std::move(grp));
        // 2. 再注册选项到 Model
        Builder(&instData_).declare(std::forward<Args_T>(_Args)...);
        return *this;
    }

private:
    /// @brief 递归提取每个声明项的长名到分组成员列表
    template <typename T, typename... Rest_T>
    void _collectGroupMembers(OptionGroup &_Grp, T &&_Arg, Rest_T &&..._Rest)
    {
        _Grp.members.push_back(_extractLongName(_Arg));
        if constexpr (sizeof...(Rest_T) > 0) {
            _collectGroupMembers(_Grp, std::forward<Rest_T>(_Rest)...);
        }
    }

    /// @brief 从声明项提取长名（仅用于 group() 内部，支持 OptionDef<T> 和 FlagDef）
    template <typename T>
    NameLong_T const &_extractLongName(T const &_Arg) const
    {
        return _Arg.longName;
    }

public:
    template <typename T>
    ContextBase &addOption(char _Short, NameLong_T _Long, Desc_T _Help, T const &_Default = T{})
    {
        Builder(&instData_).addOption<T>(_Short, _Long, _Help, _Default);
        return *this;
    }

    template <typename T>
    ContextBase &addOption(NameLong_T _Long, Desc_T _Help, T const &_Default = T{})
    {
        Builder(&instData_).addOption<T>(_Long, _Help, _Default);
        return *this;
    }

    /// @brief 构筑值选项（含短名，int 类型推导版本）
    ContextBase &addOption(char _Short, NameLong_T _Long, Desc_T _Help, int _Default)
    { Builder(&instData_).addOption<int>(_Short, _Long, _Help, _Default); return *this; }

    /// @brief 构筑值选项（含短名，double 类型推导版本）
    ContextBase &addOption(char _Short, NameLong_T _Long, Desc_T _Help, double _Default)
    { Builder(&instData_).addOption<double>(_Short, _Long, _Help, _Default); return *this; }

    /// @brief 构筑值选项（含短名，std::string 类型推导版本）
    ContextBase &addOption(char _Short, NameLong_T _Long, Desc_T _Help, std::string const &_Default)
    { Builder(&instData_).addOption<std::string>(_Short, _Long, _Help, _Default); return *this; }

    /// @brief 构筑值选项（仅长名，int 类型推导版本）
    ContextBase &addOption(NameLong_T _Long, Desc_T _Help, int _Default)
    { Builder(&instData_).addOption<int>(_Long, _Help, _Default); return *this; }

    /// @brief 构筑值选项（仅长名，double 类型推导版本）
    ContextBase &addOption(NameLong_T _Long, Desc_T _Help, double _Default)
    { Builder(&instData_).addOption<double>(_Long, _Help, _Default); return *this; }

    /// @brief 构筑值选项（仅长名，std::string 类型推导版本）
    ContextBase &addOption(NameLong_T _Long, Desc_T _Help, std::string const &_Default)
    { Builder(&instData_).addOption<std::string>(_Long, _Help, _Default); return *this; }

    ContextBase &addFlag(char _Short, NameLong_T _Long, Desc_T _Help)
    {
        Builder(&instData_).addFlag(_Short, _Long, _Help);
        return *this;
    }

    ContextBase &addFlag(NameLong_T _Long, Desc_T _Help)
    {
        Builder(&instData_).addFlag(_Long, _Help);
        return *this;
    }

    /* ---------- 数据访问 ---------- */

    /// @brief 获取底层 Model 引用（只读，用于 expressions() 等统一视图）
    auto model() const noexcept -> Model const & { return instData_; }

    Argument *argument(Name_T const &_Name)
    {
        auto *p = instData_.args.mgrName.mutGet(_Name);
        if (!p) { p = instData_.args.mgrPos.mutGet(_Name); }
        return p;
    }
    Argument const *argument(Name_T const &_Name) const
    {
        auto *p = instData_.args.mgrName.doGet(_Name);
        if (!p) { p = instData_.args.mgrPos.doGet(_Name); }
        return p;
    }

    OptionValue *option(NameLong_T const &_Name)
    {
        return instData_.opts.mgrValues.mutFind(_Name);
    }
    OptionValue const *option(NameLong_T const &_Name) const
    {
        return instData_.opts.mgrValues.find(_Name);
    }

    template <typename T>
    T getArgument(Name_T const &_Name) const
    {
        auto const *p = instData_.args.mgrName.doGet(_Name);
        if (!p) { p = instData_.args.mgrPos.doGet(_Name); }
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
        auto const *p = instData_.opts.mgrValues.find(key);
        if (!p && key.size() == 1) { p = instData_.opts.mgrValues.find(key[0]); }
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
        if (instData_.opts.mgrValues.find(key)) { return true; }
        if (key.size() == 1 && instData_.opts.mgrValues.find(key[0])) { return true; }
        return false;
    }

    bool isParsed(NameShort_T _Name) const
    {
        return instData_.opts.mgrValues.find(_Name)
               && instData_.opts.mgrValues.find(_Name)->bExplicit__;
    }
    bool isParsed(NameLong_T const &_Name) const
    {
        if (auto const *p = instData_.opts.mgrValues.find(_Name)) { return p->bExplicit__; }
        if (auto const *p = instData_.args.mgrName.doGet(_Name)) { return p->hasValue(); }
        return false;
    }

    /* ---------- 环境变量交互 ---------- */

    bool has_env(Name_T const &_Key) const noexcept { return instData_.envs.doFind(_Key); }
    std::string get_env(Name_T const &_Key) const { return EnvParser(__envsMut()).get(_Key); }
    std::string get_env_or(Name_T const &_Key, std::string const &_Default = {}) const
    {
        return EnvParser(__envsMut()).getOr(_Key, _Default);
    }

    ContextBase &set_env(Name_T _Key, std::string _Value)
    {
        EnvBuilder(&instData_.envs).setEnv(std::move(_Key), std::move(_Value));
        return *this;
    }

    ContextBase &unset_env(Name_T const &_Key)
    {
        EnvBuilder(&instData_.envs).unsetEnv(_Key);
        return *this;
    }

    /** @brief 注册必需环境变量（声明式条目；缺失报 MISSING_ENV）
     *  @return EnvSpec* 命中句柄（链式 .semantic()/.range()/.defaultEnv() 等） */
    auto requireEnv(Name_T const &_Key, Desc_T _Help = {}) -> EnvSpec *
    {
        return EnvBuilder(&instData_.envs).requireEnv(_Key, std::move(_Help));
    }

    /// @brief 查询声明式环境条目句柄（未注册返回 nullptr）
    auto envSpec(Name_T const &_Key) noexcept -> EnvSpec *
    {
        return EnvBuilder(&instData_.envs).envSpec(_Key);
    }

    size_t apply_env()
    {
        return EnvParser(&instData_.envs).apply().size();
    }

    std::string expand_env(std::string const &_Text) const
    {
        return EnvParser(__envsMut()).expand(_Text);
    }

    std::string env_string() const { return EnvParser(__envsMut()).envString(strPrjName_); }
    void print_env() const { std::cout << env_string(); }

    ContextBase &set_builtin_options(bool _On)
    {
        instData_.bBuiltinOptions = _On;
        return *this;
    }

    ContextBase &set_exit_code(int _Code) noexcept
    {
        nExitCode_ = _Code;
        return *this;
    }
    int exit_code() const noexcept { return nExitCode_; }

    /* ---------- 参数格式 / 快照 ---------- */

    parameter_schema schema() const { return config::schema_of(instData_); }

    std::string schema_string() const
    {
        std::ostringstream oss;
        auto               sch = schema();
        oss << "schema: " << (sch.program.empty() ? strPrjName_ : sch.program) << "\n";
        for (auto const &e : sch.entries) {
            oss << "  [" << e.kind << "] " << e.name;
            if (!e.type.empty()) { oss << " <" << e.type << ">"; }
            if (!e.value.empty()) { oss << " = \"" << e.value << "\""; }
            if (!e.env.empty()) { oss << "  env: " << e.env; }
            oss << "\n";
        }
        return oss.str();
    }

    parameter_snapshot snapshot() const { return config::snapshot_of(instData_); }
    void apply_snapshot(parameter_snapshot const &_Snap) { config::apply_snapshot(instData_, _Snap); }

    /* ---------- 配置脚本 ---------- */

    std::string to_yaml() const { return config::to_yaml(instData_); }

    bool save_config(std::string const &_Path) const
    {
        return config::write_file(_Path, config::to_yaml(instData_));
    }

    bool from_yaml(std::string const &_Text)
    {
        return config::from_yaml(instData_, _Text, static_cast<Errors *>(this));
    }

    bool load_config(std::string const &_Path)
    {
        auto text = config::read_file(_Path);
        if (!text) { setMsg("config: cannot read file: " + _Path); return false; }
        return from_yaml(*text);
    }

    std::string env_to_yaml() const { return config::env_to_yaml(instData_); }

    bool yaml_to_env(std::string const &_Text)
    {
        return config::yaml_to_env(instData_, _Text, static_cast<Errors *>(this));
    }

    /* ---------- 执行状态 ---------- */

    ContextBase &reset()
    {
        builtin_detail::resetModelData(&instData_);
        clearLast();
        return *this;
    }

    /* ---------- 解析（公共包装：argv/tokens 归一化 + 双通道错误处理） ---------- */

    ContextBase &parse(int _Argc, char const *_Argv[], char *_Envp[])
    {
        try {
            Tokens_T toks;
            Envs_T   envs;
            __preambleArgv(_Argc, _Argv, _Envp, toks, envs);
            _doRunPipeline(toks, envs);
        }
        catch (ErrorParse const &e) { setLast(e.code()); throw; }
        catch (ErrorLexer const &e) { setLast(e.code()); throw; }
        return *this;
    }

    ContextBase &parse(std::error_code &_Ec, int _Argc, char const *_Argv[], char *_Envp[]) noexcept
    {
        try {
            Tokens_T toks;
            Envs_T   envs;
            __preambleArgv(_Argc, _Argv, _Envp, toks, envs);
            _doRunPipeline(toks, envs);
            _Ec = make_error_code(ParseError_E::OK);
        }
        catch (ErrorParse const &e) { _Ec = e.code(); setLast(e.code()); }
        catch (ErrorLexer const &e) { _Ec = e.code(); setLast(e.code()); }
        catch (std::exception const &ex) { setMsg(ex.what()); _Ec = make_error_code(ParseError_E::UNKNWON_ARGS); }
        return *this;
    }

    ContextBase &parse(Tokens_T const &_Tokens, Envs_T const &_Env)
    {
        try {
            Tokens_T real = _Tokens;
            __preambleTokens();
            _doRunPipeline(real, _Env);
        }
        catch (ErrorParse const &e) { setLast(e.code()); throw; }
        catch (ErrorLexer const &e) { setLast(e.code()); throw; }
        return *this;
    }

    ContextBase &parse(std::error_code &_Ec, Tokens_T const &_Tokens, Envs_T const &_Env) noexcept
    {
        try {
            Tokens_T real = _Tokens;
            __preambleTokens();
            _doRunPipeline(real, _Env);
            _Ec = make_error_code(ParseError_E::OK);
        }
        catch (ErrorParse const &e) { _Ec = e.code(); setLast(e.code()); }
        catch (ErrorLexer const &e) { _Ec = e.code(); setLast(e.code()); }
        catch (std::exception const &ex) { setMsg(ex.what()); _Ec = make_error_code(ParseError_E::UNKNWON_ARGS); }
        return *this;
    }

    /* ---------- 执行模式（纯虚接口，派生类按引擎注入实现） ---------- */

    virtual void once() = 0;
    virtual void loop() = 0;

    /* ---------- 子命令 ---------- */

    /// @brief 添加子命令（链式 API）
    /// @param _Name 子命令名称
    /// @param _Factory 创建子 Context 的工厂函数
    ContextBase &addSubcommand(std::string _Name, SubcommandFactory _Factory)
    {
        _registerSubcommand(std::move(_Name), std::move(_Factory));
        return *this;
    }

    /// @brief 添加子命令（模板版本，自动推导工厂类型）
    template <typename ContextType, typename... Args>
    ContextBase &addSubcommand(std::string _Name, Args &&..._Args)
    {
        _registerSubcommand(std::move(_Name), [args = std::make_tuple(std::forward<Args>(_Args)...)]() {
            return std::apply([](auto &&...a) { return std::make_unique<ContextType>(std::forward<decltype(a)>(a)...); }, args);
        });
        return *this;
    }

    /// @brief 获取当前激活的子命令路径（如 ["commit"] 或 ["remote", "add"]）
    auto getActiveSubcommandPath() const noexcept -> std::vector<std::string> const &
    {
        return activeSubcommandPath_;
    }

    /// @brief 获取当前激活的子 Context（可能为空）
    ContextBase *getActiveSubcommand() const noexcept
    {
        return activeSubContext_.get();
    }

    /// @brief 是否有子命令注册
    bool hasSubcommands() const noexcept
    {
        return !subcommandRegistry_.empty();
    }

    /// @brief 获取子命令注册表（只读）
    SubcommandRegistry const &subcommands() const noexcept
    {
        return subcommandRegistry_;
    }

    /// @brief 获取底层 Model 引用（可读写，供子命令路由使用）
    Model &mutableModel() noexcept { return instData_; }

    /// @brief 获取底层 Model 引用（只读，供子命令路由使用）
    Model const &mutableModel() const noexcept { return instData_; }

    using Errors::dump;
    using Errors::getLast;
    using Errors::getLastMsg;
    using Errors::getLastVal;
    using Errors::isOk;

public:
    /// @brief 底层数据仓库访问器（供子命令上下文传递用）
    auto &_instData() noexcept { return instData_; }

protected:
    /// @brief 标记「已完成一次解析」（once() 门控依据）
    void _markParsed() noexcept { bParsed_ = true; }

    /// @brief 派生类访问错误栈的唯一通道（Errors 为本类私有基）
    auto _errors() noexcept -> Errors & { return static_cast<Errors &>(*this); }

    /// @brief 引擎钩子：消费归一化后的 token 流与环境串（纯虚，派生类注入）
    virtual void _doRunPipeline(Tokens_T &_Toks, Envs_T const &_Envs) = 0;

protected:
    std::string strPrjName_;         ///< 工程名
    Model       instData_;           ///< 核心数据仓库
    bool        bParsed_   = false;  ///< 已完成一次解析
    int         nExitCode_ = 0;      ///< once() 退出码

    /// 子命令注册表
    SubcommandRegistry subcommandRegistry_;

    /// 当前激活的子命令路径（如 ["commit"] 或 ["remote", "add"]）
    std::vector<std::string> activeSubcommandPath_;

    /// 当前激活的子 Context（解析后持有）
    std::unique_ptr<ContextBase> activeSubContext_;

    /// @brief 注册子命令（protected，由 addSubcommand 调用）
    /// @param _Name 子命令名称
    /// @param _Factory 子 Context 工厂函数
    void _registerSubcommand(std::string _Name, SubcommandFactory _Factory)
    {
        subcommandRegistry_.registerSubcommand(std::move(_Name), std::move(_Factory));
    }

private:
    EnvData *__envsMut() const
    {
        return &const_cast<Model &>(instData_).envs;
    }

    void __preambleArgv(int _Argc, char const *_Argv[], char *_Envp[], Tokens_T &_Toks, Envs_T &_Envs)
    {
        if (_Argc > 0 && _Argv[0]) {
            instData_.strProgramName = _Argv[0];
        }
        else {
            instData_.strProgramName = strPrjName_;
        }

        if (_Argc > 1) {
            Lexer lex(_Argc - 1, _Argv + 1, nullptr);
            _Toks = lex.tokens();
        }

        if (_Envp) { for (int i = 0; _Envp[i]; ++i) { _Envs.emplace_back(_Envp[i]); } }
    }

    void __preambleTokens()
    {
        if (instData_.strProgramName.empty()) {
            instData_.strProgramName = strPrjName_;
        }
    }
};

CMD_ARGS_NAMESPACE_END
