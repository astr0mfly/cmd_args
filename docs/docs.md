# docs — 声明式构建与响应式数据的概念澄清、现状盘点与候选决策

> **摘要**：把 cmd_args 的三层构造用一句话串起来——构建期用链式/声明式语法糖描述参数空间，运行期每个参数值都是一个 `Property<T>`，命令行、环境变量、YAML/快照作为数据源写入或兜底，读写都可被钩子拦截。本文给出术语定义、三层模型、数据源矩阵、逐条带 `file:line` 的符合项/差距盘点，以及尚未实施的候选决策（统一 EnvData 存储、`bind()` 数据源绑定）；env 空间分隔符已宏化为 `CMD_ARGS_ENV_SCOPE_SEP`（默认 `'::'`，`'-'` 可覆盖）。
> **联系**：[.dsh/Agent.md](../.dsh/Agent.md)（§2 硬约束、§12 文档机制）；[cmd_args.md](../include/cmd_args/cmd_args.md)（模块与管线）；[property.md](../include/cmd_args/property/property.md)（属性 API）；[argument.md](../include/cmd_args/argument/argument.md)、[option.md](../include/cmd_args/option/option.md)、[env.md](../include/cmd_args/env/env.md)、[config.md](../include/cmd_args/config/config.md)、[unit_test.md](../dfx/unit_test/unit_test.md)。

## truth

### 1. 三个词的工程含义

- **声明式构建（declarative construction）**：用户描述“参数空间是什么样”（名字、类型、默认值、帮助、绑定的环境变量、互斥/依赖），而不是逐步说“先注册 A 再注册 B”。cmd_args 里的落地形态有三类：`build<T>()` 模板选择器、`declare(...)` 折叠声明、`group(name, ...)` 分组。
- **响应式数据（reactive data）**：参数值不是裸字符串，而是一个可观察的存储单元——写入可被准入检查拒绝、读取可被默认值/派生值改写、变更可被回调订阅、修订号可观察。对应 `Property<T>`。
- **数据源（source）与绑定（binding）**：外部输入（argv、envp、YAML/快照）按优先级写入同一个值单元；绑定声明“这个参数与环境变量 `MY_TOOL-OUTPUT` 相关、与配置键 `options.output` 相关”。当前显式绑定只有 `.env("VAR")`，其余由配置/快照按名称匹配。

### 2. 三层概念模型

```
构建期（declare）
  chain / build<T>() / declare() / group() / OptionDef / FlagDef
        │  声明参数空间：Argument / Option / Flag / EnvSpec
        ▼
运行期值模型（Property<T>）
  get() / set(v) / setRaw(v) / defaultValue / onWrite / onRead / onChange / version
        ▲                │
        │ 写              │ 读（默认值 + onRead）
数据源层（sources）
  argv      ─► Lexer/Scanner ─► Parser ─► set()/setValue()      （过写钩子）
  envp      ─► EnvParser ─► __seedEnvDefaults ─► set()/setValue()（过写钩子）
  YAML/快照 ─► setExternal ─► setRaw()                          （旁路写钩子）
  ${VAR}    ─► expand ─► setRaw()                               （派生值旁路）
```

优先级链（未变）：**命令行显式 > 配置文件 YAML/快照 > 环境变量 > 内置默认值**。每段都有明确落点：CLI 在 `option_parser.h:528-536`（`__assign` → `setValue` → 置 `bExplicit__`）与 `argument_parser.h:114` 等处（`p->set(...)`）；env 兜底在 `compiler.h:256-272`；YAML/快照回填在 `config_store.h:153-166`。

### 3. 声明式构建：已有语法糖

- **链式**：`ContextBase::build<T>()`（`include/cmd_args/context/context_base.h:68`）返回 `ContextBase&`，`addOption/addFlag` 等均返回 `*this`；另有 initializer_list 版 `:75` 与 `BuildHelper` 版 `:81`。
- **数据驱动声明**：`Builder::declare(Args_T&&...)`（`include/cmd_args/context/builder.h:76`）折叠展开，支持 `ArgumentNamed` / `ArgumentPosition` / `OptionDef<T>` / `FlagDef`；不支持类型触发 `static_assert`（`builder.h:100`）。`ContextBase::declare` 在 `context_base.h:92`。
- **声明结构体**：`OptionDef`（`include/cmd_args/context/declarations.h:39`）与 `FlagDef`（`:53`）是纯数据声明，可提前放数组/表里批量展开。
- **分组**：`ContextBase::group(Name, Args...)`（`context_base.h:108`）先从折叠参数里提取成员名，再建组。
- **统一视图**：`Expression::kind()`（`include/cmd_args/expression/expression.h:61`）非虚分类，`Model::expressions()`（`include/cmd_args/context/model.h:62`）把 `ArgumentData` / `OptionData` / `EnvData` 枚举成同一 `Expression` 视图。

```cpp
// 链式：逐步声明
app.build<ArgumentNamed>("input", "输入文件")
   .addOption<int>('t', "timeout", "超时(秒)", 30)
   .addFlag('v', "verbose", "详细输出");

// 数据驱动：一份表 → 一次声明
OptionDef<int> defs[] = { {'t', "timeout", "超时(秒)", 30}, {'p', "port", "端口", 8080} };
app.declare(defs[0], defs[1]).group("net", "timeout", "port");
```

**符合度评估**：声明式与链式两种形态都已具备；缺口是“整份声明对象的装载与校验”（如从一份表批量生成并做重名/依赖检查，目前重名靠构筑期抛 `DUPLICATE_NAME`），以及**声明式数据源绑定**（见第 6 节 D3）。

### 4. 响应式数据：Property 的拦截语义

- **写**：`Property::set`（`include/cmd_args/property/property.h:70`）依次执行 `onWrite` 链，任一钩子返回非空串即拒绝——记 `lastError()`、值不变、`version()` 不涨；通过后清错误、置 `bSet__`、必要时触发 `onChange`。`setRaw`（`:85`）跳过写钩子，供配置/快照/派生值使用。
- **读**：`get`（`:92`）先取当前值（未显式设置时取默认值），再依次过 `onRead` 链，返回按值结果。
- **变更与版本**：`onChange`（`:188`）收到旧值与新值；`version`（`:108`）仅在存储值真的变化时自增。
- **默认值**：`defaultValue`（`:123`）/`defaultValue(v)`（`:126`）/`defaultFn`（`:134`）/`reset`（`:152`）；`Argument::defaultExpr` 与 `OptionValue::defaultExpr` 委托给它，`reset()` 重算默认值。
- **参数层接线**：`Argument::set`（`include/cmd_args/argument/argument.h:194/:204`）被拒绝时抛 `ErrorValidation(ValidationError_E::VALIDATION_FAILED, "argument <name>: <msg>")`；`OptionValue::setValue`（`include/cmd_args/option/option.h:238`）同理抛 `"option --<longName>: <msg>"`；`getValue()` 均按值返回（`argument.h:308`、`option.h:232`），`property()` 暴露底层属性（`argument.h:311-314`、`option.h:248-251`）。
- **旁路规则**：配置/快照经 `setExternal`（`argument.h:217/:229`、`config_store.h:158-166`）写 `setRaw`；`${VAR}` 展开在 `compiler.h:280/:288` 写 `setRaw`。旁路值仍会进入统一校验 `validate_detail::validate`（`include/cmd_args/expression/validation.h:156`）。
- **已知边界**：没有跨数据源订阅——env 变更不会自动回灌，seed 只在 finalize 阶段执行一次（`compiler.h:256`）；`version()` 目前只是计数，没有内置缓存层消费它；写/读钩子在解析期同步执行，须快速且不得重入。

### 5. 数据源矩阵

| 来源 | 入口 | 写入方式 | 过写钩子 | 优先级 | 证据 |
| --- | --- | --- | --- | --- | --- |
| 命令行选项 | `OptionParser::__assign` | `setValue` | 是 | 1 | `option_parser.h:528-536` |
| 命令行参数 | `ArgumentParser` | `p->set` | 是 | 1 | `argument_parser.h:114`/`:160`/`:207`/`:380` |
| 环境变量兜底（已绑定） | `Compiler::__seedEnvDefaults` | `set` / `setValue` | 是 | 3 | `compiler.h:256-272` |
| `${VAR}` 展开 | `__expandEnvValues` | `setRaw` | 否（派生） | 派生 | `compiler.h:280`/`:288`；`interpreter.h:504-512` |
| YAML / 快照 | `apply_snapshot` | `setExternal` → `setRaw` | 否 | 2 | `config_store.h:153-166` |
| 进程内显式赋值 | 用户代码 | `set` / `setValue` | 是 | 用户调用 | `argument.h:194`；`option.h:238` |
| loop 每轮还原 | `reset()` | 重算默认值、清标记 | — | — | `argument.h:247`；`option.h:179` |

### 6. 符合项 / 差距盘点（逐条可核查）

| # | 要求 | 结论 | 证据 |
| --- | --- | --- | --- |
| 1 | 链式语法糖做声明式调用 | 符合 | `context_base.h:68`/`:75`/`:81`/`:92`/`:108`；`builder.h:76` |
| 2 | 声明式数据结构描述参数 | 符合 | `declarations.h:39`/`:53`；`builder.h:106` |
| 3 | 参数 / 选项 / 环境同构视图 | 符合 | `expression.h:61`；`model.h:62` |
| 4 | 赋值可拦截（准入检查） | 符合（本次新增） | `property.h:70`；`argument.h:194`；`option.h:238` |
| 5 | 取值可拦截（默认值 / 派生读） | 符合（本次新增） | `property.h:92`；`argument.h:308`；`option.h:232` |
| 6 | 变更可观察 + 版本号 | 符合（进程内） | `property.h:188`/`:108` |
| 7 | 环境变量声明式条目 | 符合 | `env.h:76`；`env_builder.h:63` |
| 8 | 参数与环境变量显式绑定 | 部分 | `argument.h:259-292`；`option.h:126`——只支持显式键名，无程序名/参数名推导 |
| 9 | env 兜底且遵守优先级 | 部分 | `compiler.h:256-272`——一次性 seed，非持续绑定 |
| 10 | 外部文件作为数据源 | 部分 | `config_store.h:56`/`:143`/`:183`/`:239`——YAML 扁平子集，无嵌套/锚点 |
| 11 | 快照 / schema 互转 | 符合 | `context_base.h:332`/`:356` |
| 12 | 数据源可直接数据绑定（统一 Source） | 差距 | 全库无 `Source`/`bind` 抽象，`grep bind` 只命中 `std::bind`（`scanner.h:154`） |
| 13 | 以环境变量为基准的“参数级存储” | 差距 | 值在 `Property`（`argument.h:384`、`option.h:254`），`EnvData` 只存环境条目（`model.h:76`） |
| 14 | env 空间分隔符可配置 | 符合（本次新增） | `base/def.h:45-54`（`CMD_ARGS_ENV_SCOPE_SEP`，默认 `"::"`）与 `:56-61`（`ENV_SCOPE_SEP`/`ENV_SCOPE_SEP_LEN`）；`token.h:61`/`:127`、`classified_expr.h:84`、`scanner.h:67`/`:154`/`:452-462` 全部改引用常量；`'-'` 覆盖实测得 `MY_TOOL-OUTPUT` |
| 15 | 统一存储（参数值全落 EnvData，`get*` 从 env 读） | 差距，且与已落地 Property 冲突 | 见「决策 · D2」 |

### 7. 文档一致性核查（2026-10-25 实测）

- 测试口径：`README.md`、`README_zh.md`、`.dsh/Agent.md §9.4`、`dfx/unit_test/unit_test.md` 一致为 **175 用例 / 903 断言**（172 通过，3 个既有 lexer/classifier 基线失败）。
- 子目录文档：`include/cmd_args`、`example`、`dfx/unit_test`、`docs` 共 23 篇 native 文档通过 `check-docs`，无缺失。
- 工作树中 `include/cmd_args/expression/{lexer,scanner}.h` 的 `detail`→`expression_detail` 重命名 WIP 是上述 3 个失败的来源，与本概念模型无关。

## 约束

### 模型约束（代码层）

- **值只经 Property**：`.dsh/Agent.md §2` 第 11 条——不允许恢复 `OptionValue::strValue` 之类的外部直写；业务写入走 `set`/`setValue`，配置/快照/派生值才允许 `setExternal`/`setRaw`。
- **写钩子**：返回非空消息即拒绝，消息必须可读（会被包成 `ErrorValidation`）；不得重入 `set`，不得做长耗时工作（解析期同步执行）。
- **读钩子**：不得抛异常、不得写回存储（`get()` 无副作用是缓存与排查的前提）。
- **旁路不豁免校验**：`setRaw` 写入的值仍要经过 `validate_detail::validate`（`validation.h:156`）。
- **优先级链不得倒置**：CLI > 文件/快照 > env > 默认；新数据源接入必须声明优先级并加冲突测试。
- **测试只测公共 API**：禁止 `#define private public` 之类手段（`.dsh/Agent.md §9`）。

### 文档约束

- 列出“符合/不符合”必须带 `file:line`，行号注明核对日期；公共 API 变动后复检。
- 候选决策必须显式标注“未实施”，不得与已落地行为混写。
- 本目录只保留这一篇官方文档；历史计划与过期样例不放这里（计划归 `.dsh/plans/`）。

## 索引

| 文件 | 关键符号 / 主题 | 在本文中的作用 |
| --- | --- | --- |
| `include/cmd_args/property/property.h` | `Property<T>` / `set` / `get` / `setRaw` / `onWrite` / `onRead` / `onChange` / `version` | 响应式值模型本体 |
| `include/cmd_args/argument/argument.h` | `propValue__` / `set` / `getValue` / `property()` / `setExternal` | 参数侧接线 |
| `include/cmd_args/option/option.h` | `propValue__` / `getValue` / `setValue` / `property()` | 选项侧接线 |
| `include/cmd_args/expression/compiler.h` | `finalize` / `__seedEnvDefaults` / `__expandEnvValues` | env 兜底与派生展开 |
| `include/cmd_args/expression/validation.h` | `validate_detail::validate` / `checkEnvSpecs` / `checkRequired` / `checkValues` | 统一校验 |
| `include/cmd_args/config/config_store.h` | `schema_of` / `snapshot_of` / `apply_snapshot` / `to_yaml` / `from_yaml` | 文件/快照数据源 |
| `include/cmd_args/context/context_base.h` | `build` / `declare` / `group` / `getArgument` / `getOption` / `schema` / `snapshot` | 声明式构筑与查询入口 |
| `include/cmd_args/context/builder.h` + `declarations.h` | `declare` / `OptionDef` / `FlagDef` | 数据驱动声明 |
| `include/cmd_args/context/model.h` | `Model` / `hasAction` / `expressions` | 三类 Data 与统一视图 |
| `include/cmd_args/base/def.h` + `expression/token.h` + `classified_expr.h` + `scanner.h` + `env/env_parser.h` | `CMD_ARGS_ENV_SCOPE_SEP` / `ENV_SCOPE_SEP` / `fullPath` / `IN_SCOPE_SEP` / `recognizeEnvAssign` | env 空间分隔符宏（默认 `'::'`，已验证 `'-'` 覆盖） |
| `include/cmd_args/context/context_step.h` + `expression/interpreter.h` | 批量 step 管线 / 流式解释执行 | 两套执行引擎下的同一套值语义 |
| `dfx/unit_test/test_property.cpp` | 属性与读写拦截用例（9 用例 / 79 断言） | 行为证据 |

## 决策

### 已定（已实施）

- **Property 是参数值的唯一存储**：读写都经属性，写拒绝即时抛 `ErrorValidation(VALIDATION_FAILED)`；配置/快照/派生值走 `setRaw` 旁路。
- **`.check()`/`.required()` 等校验器不自动挂写钩子**：统一校验仍在解析末尾聚合首错；需要“写入即校验”时在 `onWrite` 里调用 `Validator`（见 `property.md` 配方）。
- **概念文档并入本目录单文件**：`docs/` 只保留 `docs.md`，避免一个目录多篇官方文档造成检查告警与口径分叉。
- **env 空间分隔符宏化、默认 `'::'`**：`base/def.h` 新增 `CMD_ARGS_ENV_SCOPE_SEP`（`#ifndef` 包裹，默认 `"::"`），导出 `ENV_SCOPE_SEP`/`ENV_SCOPE_SEP_LEN`；`Token::toText`（`token.h:127`）、`ClassifiedExpr::fullPath`（`classified_expr.h:84`）、Scanner 的 `IN_SCOPE_SEP`/`__processScopeSep`（`scanner.h:452-462`）全部改引用常量。`'-'` 通过 include 之前 `#define CMD_ARGS_ENV_SCOPE_SEP "-"` 覆盖，实测 `MY_TOOL-OUTPUT` 正确切分。回退 `'::'` 成立：POSIX shell 对 `'-'` 和 `'::'` 都禁止 `export`，按平台切默认只会让键名漂移。

### 候选（均**未实施**）

#### D2 统一 EnvData 存储（参数值全部存进 EnvData，`get*` 从 env 读）

- **与已落地硬约束冲突**：`.dsh/Agent.md §2` 第 11 条要求“参数值只经 Property 读写”，Property 是唯一存储（`argument.h:384`、`option.h:254`）。
- 三种折中：
  1. **Property 为主 + EnvData 镜像**：每次写入成功后把值同步进 `envs`（键=推导的 env 名），读取仍走 Property。代价：双份状态与同步失效；收益：`Model::expressions()` / `env_to_yaml` / schema 天然统一。
  2. **EnvData 为主 + Property 视图**：Property 变成 EnvData 的包装，`get` 从 env 读。代价：未绑定参数需要合成键（如 `<program>-<name>`），会把“环境变量”语义扩大到所有参数，与用户可 export 的定位冲突。
  3. **不统一存储，只统一命名/绑定基准**：Property 保持唯一存储，env 键成为参数对外唯一标识（供 YAML `environment` 区、schema、文档）。代价最小，不改 `get*` 数据流。
- 建议：近期取 3；若取 1，需先在 Model 层定义同步点（谁权威、何时失效），并补一致性测试。

#### D3 数据源声明式绑定 API（bind / source）

目标形态（示意，当前不存在）：

```cpp
app.option("output")
   .bind(src::Env("MY_TOOL-OUTPUT"))
   .bind(src::File("cfg.yaml", "options.output"));
```

- 规则：按优先级排序（CLI > File > Env > default）；File/Env 写入走 `setRaw` 并记录来源；冲突时高优先级胜出且可查询 `sourceOf(name)`。
- 现状：只有 `.env()` 单绑定（`argument.h:259-292`、`option.h:126`）与全量 `load_config`/`apply_snapshot`。
- 补齐建议：P1 新增 `SourceKind_E::{Cli,Env,File,Snapshot}` 与优先级表；P2 复用 `CMD_ARGS_ENV_SCOPE_SEP` 统一 Source 键名推导；P3 视 D2 结论加 `syncEnvView`；P4 用 `version()` 做派生值缓存与失效。

## 教训

- **本目录一度只剩过期样例与历史计划**：这些内容删除后目录空缺；重建为单文件概念文档，不回填历史计划（计划属于 `.dsh/plans/`）。
- **概念文档最容易漂移**：测试计数与 `file:line` 必须在写作当天实测；本次核对发现 `unit_test.md` 仍按旧口径（166/818），已同步为 175/903。
- **3 个既有失败不是本次回归**：`test_lexer.cpp:207`/`:241`（`toks[6].getAttr()`、`toks.size()`）与 `test_expression_classifier.cpp:180`（`exprs.size()`）来自未提交的 scanner/lexer `detail`→`expression_detail` WIP，改动文档或属性代码时不要把它们当作自己的回归。
- **分隔符不能按平台改默认**：`'-'` 与 `'::'` 在 shell 里同样不可 `export`，按平台切默认只会让键名漂移；改为单一宏 `CMD_ARGS_ENV_SCOPE_SEP`（默认 `'::'`）后由用户按需覆盖，词法与文档里不再出现第二个字面量。
- **EnvLine 不在生产路径**：`A::B=V` 只在 `scanEnvLine`/`recognizeEnvAssign`（当前仅单测）里级联，真实 envp 由 `EnvParser::parse` 按首个 `'='` 截键；验证分隔符要用 `Lexer::scanEnvLine`，不要指望 `parse` 拆键。
- **D2 与 Property 冲突要先定权威**：在“值存哪儿”没定论前，不要同时推进镜像同步与响应式钩子，否则两套真值源会互相覆盖。
