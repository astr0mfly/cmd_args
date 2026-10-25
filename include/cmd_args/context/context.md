# context — 上下文抽象、引擎装配与子命令路由

> **摘要**：本目录是 cmd_args 的上下文层：`ContextBase` 收拢与引擎无关的能力（声明式构筑 `build` / `declare` / `group` / `addOption` / `addFlag`、4 个 `parse` 重载、环境变量与 YAML 接口、子命令注册表），`ContextStep` 与 `ContextRuntime` 分别注入「Compiler+Executor 批量」和「Interpreter 流式」两套引擎，由 `factory.h` 的 `make_context()` 选择；`Context` 是兼容期完整实现，同时持有两套引擎并负责层级子命令路由。本目录不含词法/语法前端本体（在 `expression/`），只负责装配与驱动。
> **联系**：[上级索引 README_zh 目录结构](../../../README_zh.md#目录结构)｜同级文档 [expression/expression.md](../expression/expression.md)、[env/env.md](../env/env.md)｜依赖头文件 [../expression/compiler.h](../expression/compiler.h)、[../expression/interpreter.h](../expression/interpreter.h)、[../expression/executor.h](../expression/executor.h)、[../env/env_parser.h](../env/env_parser.h)｜使用方 [example/once/main.cpp](../../../example/once/main.cpp)、[dfx/unit_test/test_context_factory.cpp](../../../dfx/unit_test/test_context_factory.cpp)。

## truth

目录内 13 个源码文件，全部为头文件内联实现，无 .cpp：

- `context_fwd.h`：共用别名 `Name_T`（`std::string`）/ `NameShort_T`（`char`，`'\0'` 表示无短名）/ `NameLong_T` / `Desc_T` / `Tokens_T`（`std::list<std::string>`）/ `Envs_T`（`std::vector<std::string>`）；`enum class EngineKind_E : char { E_STEP, E_RUN_TIME }`；前置声明 `class Errors`。
- `context_base.h`（586 行）：`class ContextBase : private Errors`，是整个上下文层的公共基座。
  - 构筑：`build<Select_T>(...)`、`build(std::initializer_list<T>)`、`build()`（返回 `BuildHelper`）、`declare(Args_T&&...)`、`group(Name_T, Args_T&&...)`、两族 `addOption`（模板 + `int`/`double`/`std::string` 非模板推导重载）、`addFlag`。
  - 解析：4 个重载 —— `parse(int, char const*[], char*[])`、`parse(std::error_code&, int, char const*[], char*[]) noexcept`、`parse(Tokens_T const&, Envs_T const&)`、`parse(std::error_code&, Tokens_T const&, Envs_T const&) noexcept`；统一先做 argv/tokens 归一化，再调纯虚 `_doRunPipeline(Tokens_T&, Envs_T const&)`。
  - 数据/配置：`model()`、`argument()`、`option()`、`getArgument<T>`、`getOption<T>`、`hasOption`、`isParsed`、`has_env`/`get_env`/`get_env_or`/`set_env`/`unset_env`/`requireEnv`/`envSpec`/`apply_env`/`expand_env`/`env_string`/`print_env`、`schema`/`schema_string`/`snapshot`/`apply_snapshot`、`to_yaml`/`save_config`/`from_yaml`/`load_config`/`env_to_yaml`/`yaml_to_env`、`reset`、`set_builtin_options`、`set_exit_code`/`exit_code`。
  - 执行模式与子命令：纯虚 `once()`/`loop()`；`addSubcommand`（普通 + 模板）、`getActiveSubcommandPath`、`getActiveSubcommand`、`hasSubcommands`、`subcommands`、`mutableModel`；`using` 导出 `Errors::dump/getLast/getLastMsg/getLastVal/isOk`。
  - 受保护成员：`strPrjName_`、`instData_`（`Model`）、`bParsed_ = false`、`nExitCode_ = 0`、`subcommandRegistry_`、`activeSubcommandPath_`、`activeSubContext_`；`_markParsed()`、`_errors()`、`_instData()`。
- `model.h`：`struct OptionGroup { Name_T name; std::vector<NameLong_T> members; }`；`class Model` 持有 `ArgumentData args`、`OptionData opts`、`EnvData envs`、`OptionGraph optionGraph`、`std::vector<OptionGroup> groups_`、`strProgramName`、`bBuiltinOptions = true`、`bBuiltinsRegistered = false`、`strSaveConfigPath`；`hasAction()` 等价于 `!opts.actionOrder().empty()`；`expressions()` 遍历三类 Data 返回 `std::vector<Expression const *>` 统一视图。
- `builder.h`：`class Builder : public ArgumentBuilder, private OptionBuilder`；`build(Group<T>&&)`、`build(Argument::Name_T, Argument::Desc_T)`、`declare(Args_T&&...)`（折叠表达式逐个 `_declareOne`：`ArgumentNamed` / `ArgumentPosition` / `FlagDef` / `OptionDef<T>`）；`struct BuildHelper { Model *pCon; operator()(...) }` 支持 `build(...)(...)` 链式。
- `declarations.h`：`template <typename T> struct OptionDef { char shortName; NameLong_T longName; Desc_T help; T defaultValue{}; }`；`struct FlagDef { char shortName; NameLong_T longName; Desc_T help; }`。
- `builtin_options.h`（namespace `builtin_detail`）：`registerBuiltinOptions(Model*)` 依 `bBuiltinOptions`/`bBuiltinsRegistered` 幂等注册 —— 短回路 `-?`/`--help`、`-!`/`--env`、`--env-apply`；交互 `--env-set`（KEY=VALUE，无 `'='` 时值为空串）、`--env-unset`、`--load-config`（`config::read_file` + `from_yaml`，失败写 stderr 不抛）、`--save-config`（置 `strSaveConfigPath`）；`resetModelData(Model*)` 只还原参数与选项值，保留 `EnvData`。
- `describe.h`（namespace `describe_detail`，`using` 导出）：`usageOf(Model const&, Name_T const&)`、`helpOf(Model const&)`（usage + 命名/方位参数帮助 + 分组的选项 + 未分组 Options）、`envOf(Model&)`（委托 `EnvParser::envString`）。
- `factory.h`：`inline auto make_context(EngineKind_E _Kind, Name_T _Name) -> std::unique_ptr<ContextBase>`；`E_STEP` → `ContextStep`，`E_RUN_TIME` → `ContextRuntime`，其余抛 `std::invalid_argument("unknown engine kind")`。
- `context.h`：`class Context : public ContextBase`，成员 `Compiler compiler__` + `Executor executor__` + `Interpreter interp__`（均绑定 `&instData_`）。`print_help()` 在 `activeSubContext_` 且 `dynamic_cast<Context const *>` 成功时委托子 Context，否则先打印子命令列表再 `compiler__.print_help()`；`once()`/`loop()` 有活动子 Context 时直接委托。`_doRunPipeline`：无子命令 → `_doRunPipelineWithoutSubcommands`（Compiler 解析 → `finalize` → `hasAction()` 门控 Executor → `_markParsed`）；有子命令 → `_scanSubcommandPath` 后把 `std::next(begin, consumed)` 起的剩余 token 交子 Context `parse` 并 `_markParsed`。`_scanSubcommandPath` 从流首连续匹配「非 `'-'` 开头且已注册」的 token，逐级建子 Context、把路径逐段拼进子 `strProgramName`、复制 `envs` 与 `bBuiltinOptions`。
- `context_step.h`：`class ContextStep : public ContextBase`，成员 `Compiler compiler__` + `Executor executor__`，不含 `Interpreter`。`once()` 未解析抛 `ErrorInterp(InterpError_E::STOPPED)`，否则 `std::exit(nExitCode_)`；`loop()` 逐行 `getline` → `util::trim` → 空行重提示 / `exit|quit|exit()` 退出 → `_errors().clearLast()` → `builtin_detail::resetModelData` → `Lexer::tokensFromLine` → `_doRunPipeline`（空 `Envs_T`），只捕获 `ErrorParse`/`ErrorLexer` 并打印 `(parse error)` + `dump()`。
- `context_runtime.h`：`class ContextRuntime : public ContextBase`，仅成员 `Interpreter interp__`，不含 `Compiler`/`Executor`。`loop()` 先 `registerBuiltinOptions` 再 `interp__.loop(_errors())`；`_doRunPipeline` 先 `EnvParser(&instData_.envs).parse(_Envs)`，把 token 流以单个空格拼成一行交 `interp__.interpret(line, _errors())`，返回错误码则以 `throw ErrorParse(ec)` 原样透传，成功后 `_markParsed()`。
- `subcommand.h`：`using SubcommandFactory = std::function<std::unique_ptr<ContextBase>()>`；`class SubcommandRegistry` 提供 `registerSubcommand`/`find`（返回 `SubcommandFactory const *` 或 nullptr）/`contains`/`names()`/`empty()`/`clear()`，底层 `std::unordered_map<std::string, SubcommandFactory> mapRegistry__`。
- `loader.h`：仅占位头文件，无任何符号；注释说明 YAML 走 `config/config_store.h` 的 `read_file`/`write_file`，本文件预留给未来 dll 动态补丁 / 额外源加载。

数据流：`parse(argc, argv, envp)` → `__preambleArgv`（argv[0] 探测程序名、`Lexer(argc - 1, argv + 1, nullptr).tokens()`、收集 envp）或 `__preambleTokens`（tokens 入口补程序名）→ 派生类 `_doRunPipeline` → Compiler 批量 / Interpreter 流式 → `Model`。成功路径统一 `_markParsed()` 置 `bParsed_`。

错误与异常：`ContextBase::parse` 的抛异常重载捕获 `ErrorParse`/`ErrorLexer`/`ErrorValidation` 后 `setLast(e.code())` 再 rethrow；noexcept 重载把 `std::exception` 归一为 `ParseError_E::UNKNWON_ARGS`；未知引擎种类抛 `std::invalid_argument`；`once()` 未解析抛 `InterpError_E::STOPPED`。

## 约束

- header-only：新能力必须以内联函数/模板落在头文件；不得新增 .cpp 或需要链接的实体（整个库 `#include "cmd_args/cmd_args.hpp"` 即用）。
- C++20 双平台（MSVC/Windows 与 GCC/Clang POSIX）；平台差异必须以 `#ifdef _WIN32` 隔离（本目录无平台分支，回写分支在 `env/env_parser.h`）。
- 命名空间只经 `CMD_ARGS_NAMESPACE_BEGIN`/`CMD_ARGS_NAMESPACE_END`（`base/def.h` 展开为 `namespace cmd_args {`），禁止裸 `namespace cmd_args`。
- 命名：private/protected 数据成员 `__`/`_` 后缀，形参 `_` 前缀 + PascalCase，返回类型非 4 字符一律 `auto ... -> T` 后置（`bool`/`void` 例外）。
- 引擎互斥：`ContextStep` 不持有 `Interpreter`、`ContextRuntime` 不持有 `Compiler`/`Executor`（两处头注释写明的不变量）；新增派生上下文必须选边，三件套并存仅限兼容版 `Context`。
- argv 归一化只发生在 `ContextBase`：派生类只能在 `_doRunPipeline` 内接引擎，不得另起一套 argv 解析。
- `once()` 必须先检查 `bParsed_`，未解析抛 `ErrorInterp(InterpError_E::STOPPED)`；解析成功路径必须调用 `_markParsed()`。
- 目录内互相 include 用短名（如 `"context_base.h"`），跨目录必须写全路径 `cmd_args/<dir>/<file>.h`。
- 子命令工厂签名固定为 `std::unique_ptr<ContextBase>()`；子 Context 不得绕过 `parse` 直写父 `Model`。
- 范围外（不得在本层实现）：`&&`/`||`/`|`/`...`、JSON 配置、dll 动态补丁（`loader.h` 仅占位）；`extern/` 下 vendored 代码不修改。

## 索引

| 文件 | 关键符号 | 职责 |
| --- | --- | --- |
| `context_base.h` | `ContextBase` / `build` / `declare` / `group` / `parse` / `_doRunPipeline` | 引擎无关基座：构筑、解析包装与双通道错误、环境/YAML、子命令表、once/loop 纯虚 |
| `context.h` | `Context` / `print_help` / `_scanSubcommandPath` | 兼容完整实现：Compiler+Executor+Interpreter 与层级子命令路由 |
| `context_step.h` | `ContextStep` / `compiler__` / `executor__` | 批量上下文：Compiler → finalize → hasAction 门控 Executor |
| `context_runtime.h` | `ContextRuntime` / `interp__` | 流式上下文：env 先入库 + token 拼行 + Interpreter |
| `factory.h` | `make_context` | 按 `EngineKind_E` 注入对应派生上下文 |
| `context_fwd.h` | `Name_T` / `Desc_T` / `Tokens_T` / `Envs_T` / `EngineKind_E` | 共用别名与引擎枚举 |
| `model.h` | `Model` / `OptionGroup` / `hasAction` / `expressions` | 数据仓库：三类 Data + OptionGraph + 分组/程序名/配置路径 |
| `builder.h` | `Builder` / `BuildHelper` / `declare` / `_declareOne` | 构筑桥接：转发 ArgumentBuilder / OptionBuilder |
| `declarations.h` | `OptionDef<T>` / `FlagDef` | 声明式批量注册的 POD |
| `builtin_options.h` | `registerBuiltinOptions` / `resetModelData` | 内置选项懒注册 + 参数/选项值还原 |
| `describe.h` | `usageOf` / `helpOf` / `envOf` | usage / help / env 文本渲染 |
| `subcommand.h` | `SubcommandRegistry` / `SubcommandFactory` | 子命令名 → 工厂映射 |
| `loader.h` | —（占位） | 预留 dll/额外源加载，无符号 |

grep 锚点（2026-10-25 核对）：`grep -n "make_context" include/cmd_args/context/factory.h`；`grep -n "class ContextBase" include/cmd_args/context/context_base.h`；`grep -n "_scanSubcommandPath" include/cmd_args/context/context.h`。

## 决策

- 抽出 `ContextBase` + 纯虚 `_doRunPipeline`：把 argv/tokens 归一化与「异常 / 错误码」双通道收敛到一处，派生类只注入引擎；代价是 `parse` 前置步骤不可定制，子命令路由只能在派生类的 `_doRunPipeline` 里做（`context.h:105`）。
- 用 `make_context(EngineKind_E)` 工厂替代单一 `Context`：显式选择引擎、避免三件套同时存在；代价是兼容期仍保留 `Context`（`parse` 走 Compiler+Executor、`loop` 走 Interpreter 两套语义）。
- 2026-08 表达式前端（expression.h / classified_expr.h / lexer.h / compiler.h / interpreter.h / executor.h）自 `context/` 迁至 `expression/`，跨目录 include 一律写全路径；旧路径 `cmd_args/context/expression.h` 已不存在（旧 `AGENT.md` 记述该迁移，源码 include 已核对）。
- `Model` 作为跨多次 `parse` 持久的数据仓库；`resetModelData` 只还原参数/选项值、保留 `EnvData`，使 `loop` 多行共享环境会话（`builtin_options.h`）。
- 子命令用「前缀 token 扫描」而非改造词法器：连续非 `'-'` 开头且命中注册表的 token 即子命令路径，剩余 token 原样交子 Context；代价是子命令名不能以 `'-'` 开头、工厂必须无参且返回 `std::unique_ptr<ContextBase>`。
- 内置选项懒注册（`bBuiltinsRegistered` 只注册一次）：避免每次 parse 重复注册；代价是 `set_builtin_options(false)` 必须在首次 parse 之前调用。

## 教训

- `ContextStep::loop()` 只捕获 `ErrorParse`/`ErrorLexer`（`context_step.h:85`、`89`），而 `Compiler::finalize()` 会抛 `ErrorValidation`（必选缺失/校验失败）→ 一条坏输入就让 REPL 退出。规避：REPL 封装自行补捕获 `ErrorValidation`，或在 loop 内先做校验。
- `resetModelData()` 只还原参数/选项值，不清 `Model::optionGraph`；而 `OptionParser` 解析关系语法时会 `addEdge` 追加（`../option/option_parser.h:631`、`638`）→ `ContextStep::loop()` 多行执行 `-a?b:c` 会累积关系边并重复求值。规避：多轮批量解析前自行 `optionGraph.clear()`。
- `Context::print_help()` 仅在 `dynamic_cast<Context const *>` 成功时委托子 Context（`context.h:60` 附近）；若子命令工厂返回 `ContextStep`/`ContextRuntime`，帮助会回落到父 `compiler__.print_help()`，子命令帮助不可见。规避：子命令工厂与父层使用同一 Context 类型，或自行渲染子帮助。
- `ContextRuntime::_doRunPipeline` 把 `Tokens_T` 用单个空格拼回一行再 `interpret`（`context_runtime.h`）：带引号/含空格的 token 边界在此丢失，重新词法化可能改变取值。规避：流式入口直接用 `interpret(原始行)`，不要经 `parse(Tokens_T)` 往返。
- 兼容版 `Context` 的 action 时机随入口变化：`parse` 由 Executor 在 `finalize` 之后统一执行，`loop` 由 Interpreter 即时执行。规避：新代码用 `make_context` 明确选边，避免同一实例混用两种语义。
- 三个派生类的 `once()` 都先检查 `bParsed_`，未解析抛 `InterpError_E::STOPPED`；省掉检查会以 `std::exit(0)` 静默退出、掩盖用法错误。
