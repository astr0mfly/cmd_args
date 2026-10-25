# env — 环境变量一等公民：存储、构筑、展开与回写

> **摘要**：本目录把环境变量做成可展示、可交互、可回写的一等公民：`environment` 记录键值与三个会话标记（被引用/被修改/新建），`EnvData` 以 `std::map` 按键存储 `environment` 与声明式 `EnvSpec`，`EnvBuilder` 负责写（设置/删除/必选声明），`EnvParser` 负责装载 envp、读取即标记引用、`${VAR}`/`%VAR%`/`$VAR` 展开、展示与 `apply()` 回写操作系统。
> **联系**：[上级索引 README_zh 环境变量一等公民](../../../README_zh.md#环境变量一等公民)（含[值展开](../../../README_zh.md#值展开)与[优先级](../../../README_zh.md#优先级)）；同级文档 [context/context.md](../context/context.md)、[expression/expression.md](../expression/expression.md)；依赖头文件 [../expression/expression.h](../expression/expression.h)、[../base/validator.h](../base/validator.h)、[../context/model.h](../context/model.h)；使用方 [example/validate/main.cpp](../../../example/validate/main.cpp)、[dfx/unit_test/test_env.cpp](../../../dfx/unit_test/test_env.cpp)。

## truth

目录内 4 个源码文件，全部为头文件内联实现：

- `env.h`：`struct environment : public Expression` —— 字段 `strKey`、`strValue`、`bReferenced__ = false`、`bModified__ = false`、`bNew__ = false`（三者均非 `mutable`）；构造 `environment(std::string const& _Key)`（仅键，查找用、kind = Env）与 `environment(std::string _Key, std::string _Value, bool _New)`；`getName()` override 返回自有 `strKey`（避免双份存储）；`operator<` 按 `strKey` 比较。同文件 `struct EnvSpec : public ValidatorMixin<EnvSpec>`（`Self_T`、`strKey`、`strHelp`、`strDefault__`、`defaultEnv(std::string)` 的 `&` / `&&` 双重载）；`using envs_t = std::set<environment>`（兼容类型）。
- `env_data.h`（namespace `env_detail`）：`class EnvData`，`Key_T = std::string`、`Value_T = std::string`；`doEmpty/doSize/doFind/doGet/doMutGet`；`doPush(environment const&)` 与 `doPush(environment&&)`（`emplace`，键已存在则保持原条目）；`doPush(Key_T const&, Value_T, bool _New)`（键已存在 → 改值并置 `bModified__`；否则插入 `environment(key, value, _New)`）；`doErase`；声明式条目 `pushSpec`/`mutSpec`/`forEachSpec`；`doVisit`/`doMutVisit`；私有 `std::map<Key_T, environment> mapEnvs__` 与 `std::map<Key_T, EnvSpec> mapSpecs__`。
- `env_builder.h`：`class EnvBuilder` 构造 (`EnvData *`)；`setEnv(Key_T, Value_T)`（存在 → 改值 + `bModified__ = true`；不存在 → `doPush(..., true)` 标新建）；`unsetEnv(Key_T const&) -> bool`（返回原先是否存在）；`requireEnv(Key_T const&, Value_T const& = {}) -> EnvSpec *`（已注册返回旧句柄、不覆盖；否则 `EnvSpec` + `required()` + `pushSpec`）；`envSpec(Key_T const&) -> EnvSpec *`。
- `env_parser.h`：`class EnvParser` 构造 (`EnvData *`)；`recognize(std::string const&) -> std::optional<ClassifiedExpr>`（无 `'='` 返回 nullopt）、`apply(ClassifiedExpr const&)`、`parse(Envs_T const&)`（无 `'='` 的行忽略，`doPush(..., false)`）；查询 `has`/`find`/`get`/`getOr`/`touch`；统计 `size`/`empty`/`referencedCount`/`changedCount`；遍历 `forEach`；展示 `envString(program)`；`expand(text)`；`apply() -> std::vector<std::string>`；Token 规则 `recognizeEnvAssign(TokenStream_T const&, std::size_t) -> std::optional<EnvMatch_T>`（`EnvMatch_T{ ClassifiedExpr expr; std::size_t nConsumed; }`）与 `applyExpr(ClassifiedExpr const&)`（以 `fullPath()` 为键）。

三标记语义：`bReferenced__` 被程序引用（`get`/`getOr`/`touch` 与 `expand` 命中时置位）、`bModified__` 本会话被改、`bNew__` 本会话新建。`envString` 的标记顺序为 `'*'`（modified）→ `'>'`（referenced）→ `'+'`（new），格式为 `environments: <program>` / `No. : "key" = "value"`。

展开：`${NAME}`（支持 `${NAME:-fallback}`）、`%NAME%`、`$NAME` 三种写法；命中则置 `bReferenced__ = true` 并替换为值；未命中保持原样（逐字符回退 `ret += ch`、`++i`），`${NAME:-fallback}` 在缺失时输出 fallback（仅 `${}` 分支实现）。

回写：`apply()` 只处理 `bModified__ || bNew__` 条目；`#ifdef _WIN32` 用 `_putenv_s(key, value)`，否则 `setenv(key, value, 1)`（覆盖既有值）；成功后清 modified/new 并返回 `"KEY=VALUE"` 列表。

优先级链：`命令行显式值 > 配置文件(YAML) > 环境变量 > 内置默认值`。依据：`Compiler::__seedEnvDefaults` 跳过 `a.hasValue() || a.isExternal()` 与 `o.bExplicit__ || o.bExternal__`（`compiler.h`），YAML 导入经 `setExternal` 置 `bExternal__`（`../config/config_store.h:48` 注释），故外部（YAML）值不被 env 覆盖；env 只在未显式赋值时兜底；内置默认值由 `addOption` 的默认参数保持。

声明式条目：`requireEnv` 注册 `EnvSpec` 并置 required；`validate_detail::checkEnvSpecs` 在缺失且 `strDefault__` 非空时用 `EnvBuilder::setEnv` 补全（标新建），仍缺失且 `isRequired()` 报 `MISSING_ENV`，存在则 `spec.validate()`。

loop 跨行保留：`resetModelData` 只还原参数/选项值（`context/builtin_options.h`），`EnvData` 不随解析行重置，因此 REPL 多行共享环境会话（README「执行模式」）。

`changedCount()` 只统计 `bModified__`，新建条目 `bNew__` 不计入。

## 约束

- header-only、C++20 双平台；回写必须 `_WIN32`/POSIX 双实现，且 POSIX 必须覆盖既有值（`setenv(..., 1)`）。
- 命名空间只经 `CMD_ARGS_NAMESPACE_BEGIN`/`CMD_ARGS_NAMESPACE_END`；命名规范：私有成员 `__` 后缀、形参 `_` 前缀、返回类型非 4 字符用 `auto ... -> T` 后置。
- 标记只能经 `EnvBuilder`/`EnvData`/`EnvParser` 的受控路径修改；`apply()` 成功后必须清 modified/new，避免重复回写。
- 只读查询用 `find()`/`has()`；`get()`/`getOr()`/`touch()` 会置 `bReferenced__`（读取即标记引用），不得用于纯只读探测。
- `EnvData::doPush(environment const&)` 语义是「键已存在则保持原条目」；需要覆盖必须走 `doPush(key, value, new)` 或 `EnvBuilder::setEnv`。
- 键唯一且按 `std::map` 字典序；`environment::operator<` 仅服务兼容类型 `envs_t`，不参与 `EnvData` 的实际存储。
- 展开只在解析后置（`Compiler::finalize` / `Interpreter::__finalize`）统一执行，禁止在 Parser/Builder 内展开。
- fallback 仅支持 `${NAME:-fallback}`；不得扩展 `%NAME%`/`$NAME` 的回退语法（README 只承诺三种写法）。
- YAML 映射与优先级由 `config/` 负责；本目录不得直接读写文件。
- `envs_t` 为兼容别名，新代码使用 `EnvData`；`extern/` 下 vendored 代码不修改。

## 索引

| 文件 | 关键符号 | 职责 |
| --- | --- | --- |
| `env.h` | `environment` / `EnvSpec` / `envs_t` | 环境条目（三标记）与声明式期望条目 |
| `env_data.h` | `EnvData` / `doPush` / `doMutGet` / `pushSpec` | 数据仓库：`map` 存储 environment 与 EnvSpec |
| `env_builder.h` | `EnvBuilder` / `setEnv` / `unsetEnv` / `requireEnv` | 写操作层：设置/删除/必选声明 |
| `env_parser.h` | `EnvParser` / `expand` / `apply` / `envString` / `recognizeEnvAssign` | 解析/服务层：装载、引用标记、展开、展示、回写 |

grep 锚点（2026-10-25 核对）：`grep -n "struct environment" include/cmd_args/env/env.h`；`grep -n "class EnvData" include/cmd_args/env/env_data.h`；`grep -n "auto expand" include/cmd_args/env/env_parser.h`；`grep -n "auto apply" include/cmd_args/env/env_parser.h`。

## 决策

- `environment` 继承 `Expression` 并覆写 `getName()` 返回自有 `strKey`：统一 `Model::expressions()` 视图、避免键双份存储；代价是基类 `strName__` 仍被写入一份，属冗余。
- `EnvData` 用 `std::map<Key, environment>` 而非旧 `std::set<environment>`：按键 O(log n) 查找且值可直接修改；代价是 `environment::operator<` 与 `envs_t` 成为遗留兼容，不再参与实际存储。
- 三标记而非单一 dirty 位：区分「被引用/被修改/被新建」以支撑 `-!`/`--env` 的展示；代价是 `apply()` 只认 modified|new、referenced 不参与回写，`changedCount()` 也只数 modified。
- 声明式 `EnvSpec` 与运行时 `environment` 分表存储：构建期期望（必选/校验/缺省）与运行时快照解耦；代价是两套键空间需人工保持一致。
- 回写只改本进程环境（`_putenv_s`/`setenv`），不写系统级注册表或用户环境；代价是只对当前进程及其子进程生效。
- 展开统一放在 finalize 而非读取时：保证 env seed 先于展开；代价是命令行显式含 `${VAR}` 的值也会被展开（README「值展开」）。
- `get/getOr` 在 const 方法内通过非 const 成员指针 `pData__` 修改标记：保持调用方接口 const 的同时记录引用；代价是「只读」方法带副作用。

## 教训

- `changedCount()` 名不副实：只统计 `bModified__`（`env_parser.h:138-143`），新建条目 `bNew__` 不计入；需要实际回写数量请用 `apply().size()`（`env_parser.h:260-276`）。
- 旧文档（迁移前的 `env/AGENT.md`，该文件已随之删除）称三标记为 `mutable` 以兼容 `std::set` 的 const 迭代器；现行源码三标记**非 mutable**（`env.h:39-66`），写入走 `EnvData::doMutGet`/`doVisit`，不要按旧描述加 mutable。
- `EnvBuilder::setEnv` 的「新建」判定取决于是否先 `EnvParser::parse(envp)`：`parse` 用 `doPush(..., false)` 装入进程环境（`env_parser.h:79-86`），之后 `setEnv` 命中已存在键会标 modified 而非 new；「新建」只在键确实不在会话中时成立（`env_builder.h:42-51`）。
- `expand` 的 `${NAME:-fallback}` 只在 `${}` 分支识别；`$NAME` 后跟 `:-` 会被当作变量名结束后的普通字符，`%NAME%` 不支持 fallback（`env_parser.h:181-251`）。
- 空值环境变量等同缺失：`validate_detail::checkEnvSpecs` 以 `p && !p->strValue.empty()` 判定 present（`expression/validation.h`），`KEY=`（空值）会触发缺省补全或 `MISSING_ENV`。
- `get/getOr/touch` 会置 `bReferenced__`（`env_parser.h:98-121`）：只读探测请用 `find()`/`has()`，否则 `--env`/`-!` 展示会多出 `'>'` 标记。
- `apply()` 回写后清 modified/new 但保留 `bReferenced__`；同一会话再次 `apply()` 返回空列表。
