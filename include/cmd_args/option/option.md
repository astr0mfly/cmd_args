# option — 值选项、标志、短回路与选项关系的注册、解析与执行时序

> **摘要**：option 目录定义选项模型（`OptionValue` 值选项、bool 标志、`OptionAction` 短回路）、注册校验与解析规则，并用 `OptionGraph` 记录条件（`-a?b:c`）与依赖（`-a->b`）边。对外核心约定是三条执行时序：内置交互选项解析期即时、用户 action 在批量路径由 Executor 后置执行、流式 Interpreter 逐 token 即时执行。
> **联系**：上级索引 [cmd_args.md](../cmd_args.md)；同级参数面 [argument.md](../argument/argument.md)；依赖 [expression.h](../expression/expression.h)、[context_fwd.h](../context/context_fwd.h)、[validator.h](../base/validator.h)、[util.h](../base/util.h)、[parser_error.h](../error/parser_error.h)、[builder_error.h](../error/builder_error.h)；执行与编排 [compiler.h](../expression/compiler.h)、[executor.h](../expression/executor.h)、[interpreter.h](../expression/interpreter.h)、[model.h](../context/model.h)、[builtin_options.h](../context/builtin_options.h)、[context_step.h](../context/context_step.h)；使用方 [chain/main.cpp](../../../example/chain/main.cpp)、[once/main.cpp](../../../example/once/main.cpp)、[group/main.cpp](../../../example/group/main.cpp)；单测 [test_option.cpp](../../../dfx/unit_test/test_option.cpp)、[test_option_graph.cpp](../../../dfx/unit_test/test_option_graph.cpp)、[test_builder.cpp](../../../dfx/unit_test/test_builder.cpp)；README 见 [内置选项](../../../README_zh.md#内置选项)、[选项 action 小节](../../../README_zh.md) 与 [优先级](../../../README_zh.md#优先级)。

## truth

**类型模型**：`Option` 继承 `Expression`，构造时传 `ExpressionKind_E::Option`（option.h:50-55）；字段为 `cShortName`（`char`，`'\0'` 表示无短名）、`strLongName`、`strHelp`，并复写 `getName()` / `getHelp()` 返回自有存储（option.h:59-63）。`OptionValue` 进一步继承 `Option` 与 `ValidatorMixin<OptionValue>`（option.h:73），字段与标记如下：

| 成员 | 含义 |
| --- | --- |
| `strType` / `strValue` / `strDefault__` | 类型串（`bool` / `int` / `double` / `std::string`）、当前值、构造默认值 |
| `bExplicit__` | 被 CLI 显式赋值过 |
| `bExternal__` | 值来自外部配置（YAML），env 兜底跳过 |
| `bBuiltin__` | 懒注册的内置交互选项，不参与 schema/配置序列化 |
| `bEnvHit__` | 环境变量兜底命中（seed 置位，reset 清除） |
| `strEnvName__` | 绑定的环境变量名 |
| `fnOnSet__` | 赋值回调钩子 |
| `strActionTemplate__` | system_call 命令模板，`{value}` 替换为本次值 |
| `fnDefault__` / `bDefaultExpr__` | 惰性默认值函数 / 默认值来自表达式 |

链式接口包括 `env` / `onSet` / `action` / `setExternal` / `reset` / `defaultExpr` / `defaultFn`；谓词包括 `isSatisfied`（`bExplicit__ || bExternal__ || bEnvHit__`，或值非空且偏离默认值）、`hasDefault`、`defaultValue`、`usesDefault`（`!bExplicit__ && !bExternal__ && !bEnvHit__`）。`OptionAction` 同样继承 `Option`，携带 `OptionAction::FnCallback_T`（`std::function<void(void)>`）与 `fnCallback`，一旦命中即短路整条解析（option.h:244-268）。

**管理器与仓库**：`ValueOptMgr` 与 `ActionOptMgr` 各自私有继承 `OptMgrItf`，分别持有 `std::vector` 容器；前者提供 `push` / `find` / `mutFind` / `forEachValue` / `calcDescMaxLength` / `appendHelp*`，后者提供 `push` / `forEachAction` / `calcDescMaxLength` / `appendHelp`。`OptionData` 组合 `mgrActions` 与 `mgrValues`，外加预留的 `mapShortNameIndex`（无读写点）、`recordParseOrder`（线性去重后追加长名）、`actionOrder()`、`clearParseOrder()`、`empty()`、`usageString()`、`calcDescMaxLength()`、`appendHelp*`、`forEachValue`（option_data.h:334-425）。

**注册**：`OptionBuilder::addOption<T>` 先以 `util::type_string<T>() == "null"` 拒绝未知类型（抛 `BuildError_E::UNSUPPORT_TYPE`），随后 `checkAddOptionSname` / `checkAddOptionLname` 校验短名/长名（空名 `NEED_NAME`、带 `-` 前缀 `BAD_PREFIX`、跨 `mgrActions` 与 `mgrValues` 重名 `DUPLICATE_NAME`），写入后 `mutFind` 返回句柄。另有 `int` / `double` / `std::string` 推导重载；`addFlag` 以类型 `bool`、默认值 `0` 注册；`addShortCircuit` 注册 `OptionAction` 且无返回值（option_builder.h:55-144）。

**解析**：`OptionParser::parse` 每次先复位 `bFired__` / `bOptEnd__` 并 `clearParseOrder()`；遇 `--` 擦除该 token、置 `bOptEnd__` 并 break；其余 token 经 `recognize` + `apply`：
- 长选项 `--name[=value]`：先查短回路并触发；未注册的长名留给参数解析（`++it`）；非 bool 选项若内联值为空，取下一个 token 作值，下一个以 `-` 开头或不存在则抛 `NEED_VALUE`；bool 选项空值归一为 `1`。
- 短选项 `-x` / `-xVALUE` / `-x=VALUE`：先查短回路；单字符时取余串或下一 token（下一 token 不检查是否以 `-` 开头，与长选项不对称）；多字符聚合 `-abc` 逐字符处理，bool 字符各自赋值，遇到首个非 bool 字符则以剩余串为值，空则吞下一 token，未知字符抛 `UNKNOWN_OPTION`。
- 关系语法 `-a?b:c` / `-a->b`：识别后由 `__parseRelation` 写入 `OptionGraph`，并从 token 流擦除。
`__assign` 是唯一的赋值核心（option_parser.h:528-552）：bool 归一为 `1`/`0`、置 `bExplicit__`；若 `bBuiltin__ && fnOnSet__` 立即调用回调且不记 `actionOrder`；否则只要 `fnOnSet__` 或 `strActionTemplate__` 非空就 `recordParseOrder(长名)`；末尾调用空钩子 `onOptionAssigned`。取值侧 `isParsed(短名/长名)`、`hasOption`、`getOption<T>`（去掉可选 `--` 前缀，单字符回退短名查找；bool 选项只能取 `bool`；类型串不匹配抛 `UNKNOWN_OPTION`）以及便捷 `getOptionBool/Int/Int64/Double/String`。

**OptionGraph**（option_graph.h:40-192）：`EdgeKind_E{Conditional, Dependency}`；`Node{strName, bEnabled}`；`Edge{strFrom, strTo, enKind, strFallback}`（`_` 表示无 else 分支）。`addEdge` 会自动建节点（fallback 非空且非 `_` 时也建）；`setNodeState` / `isNodeEnabled` 读写节点；`evaluate()` 只处理 `Conditional` 边，条件为真激活 `strTo`、为假激活 `strFallback`，返回本次激活名单（不去重）；`topologicalSort()` 以 `map` 字典序初始化入度为零的队列做 BFS，环上节点静默缺失；`edges` / `nodes` / `empty` / `clear` 为查询与清理。

**接线**：`Compiler` 在解析前 `optParser__.setGraph(&pModel__->optionGraph)`（compiler.h:195）；批量路径先 `optParser__.parse`，再在「未出现 `--`」的分支执行 `argParser__.parse` 后调用一次 `optionGraph.evaluate()`（compiler.h:200-208），`--` 分支不求值；解析抛异常时清空 `actionOrder` 与 `optionGraph`（compiler.h:216-219）。`Executor::execute` 只遍历 `actionOrder()`，对每个长名 `mutFind` 后先调 `fnOnSet__`，再替换模板中的 `{value}` 并用 `std::system` 执行，非 0 退出码打印 `(action) command exit` 警告但不中断，最后 `clearParseOrder()`（executor.h:52-73）。`Executor` 不读取 `optionGraph`、不调用 `topologicalSort`；`topologicalSort` 在全仓仅由 `test_option_graph.cpp` 调用。

**三条时序**：
1. 内置交互选项（`bBuiltin__ && fnOnSet__`）：`__assign` 解析期立即回调，不记 `actionOrder`（option_parser.h:538-544）；典型是 `--env-set` / `--load-config` 等，需要先于后续解析（如方位参数数量校验）生效。
2. 用户 action（`fnOnSet__` 或 `action` 模板）：批量路径仅 `recordParseOrder`，由 `Context::_doRunPipeline` 在 `compiler.finalize()`（seed 环境变量、`${VAR}` 展开、必选/校验）之后，以 `Model::hasAction()` 门控执行 `Executor`（context_step.h:100-115）；finalize 抛错则 action 不执行。
3. 流式 `Interpreter`：私有继承三个 Parser，但不调用 `OptionParser::__assign`，而是对每个选项 token 直接 `__executeOptionImmediate` —— 赋值、置 `bExplicit__`、立即触发 `fnOnSet__` 与 action 模板（interpreter.h:428-453），不使用 `actionOrder`，每次命中都执行。

**批量去重**：`recordParseOrder` 按长名去重（option_data.h:343-347），Executor 用选项的**最终值**执行一次。`test_option.cpp:66-86` 以三次 `--step one/two/three` 断言只写出一行且值为 `three`；`test_option.cpp:88` 起断言环境变量兜底与 YAML 外部赋值不触发 action；`test_option.cpp:61-63` 断言 action 不进入 schema / `to_yaml`。

## 约束

- 头文件式：实现全部位于 .h（README_zh.md:3、318）；C++20（xmake.lua:11）；命名空间用 `CMD_ARGS_NAMESPACE_BEGIN` / `CMD_ARGS_NAMESPACE_END`（base/def.h:28-29），detail 实现后 `using` 导出。
- 内置交互选项必须在解析期即时生效，且不得记入 `actionOrder`（否则 Executor 会重复触发，且配置导入会晚于方位数量校验）。
- 用户 action 只允许在 CLI 显式命中时进入 `actionOrder`；环境变量兜底、YAML 外部赋值、内置默认值都不得触发（option.h:148；test_option.cpp:88 起）。
- action 属于执行配置而非取值：不得进入 schema 或 YAML 导出（test_option.cpp:61-63）。
- 批量路径同一选项多次命中只执行一次，且使用最终值；不能按出现次数执行（recordParseOrder 去重 + Executor 以 key 查值）。
- `OptionGraph` 当前只记录关系：Executor 不消费节点状态或拓扑序，任何「依赖/条件会改变执行顺序」的假设都不成立（compiler.h:207、executor.h:52-73）。
- 命名规则：长名非空、不得以 `-` 开头；短名 `'\0'` 表示无短名；短名与长名在两个管理器中都必须唯一。
- `bBuiltin__` 的选项不参与 schema / 配置序列化（config_store.h:92 跳过），注册内置选项时必须设置该标记。

## 索引

| 文件 | 关键符号 | 职责 |
| --- | --- | --- |
| `option.h` | `Option` / `OptionValue` / `OptionAction` / `env` / `onSet` / `action` / `setExternal` / `reset` / `isSatisfied` / `usesDefault` | 选项值模型：短/长名、类型、默认值、标记、回调与 action 模板 |
| `option_data.h` | `OptMgrItf` / `ValueOptMgr` / `ActionOptMgr` / `OptionData` / `recordParseOrder` / `actionOrder` / `clearParseOrder` / `usageString` | 值选项与短回路容器、action 执行顺序、帮助与用法串 |
| `option_builder.h` | `OptionBuilder` / `addOption` / `addFlag` / `addShortCircuit` / `checkAddOptionSname` / `checkAddOptionLname` | 注册入口与重名/前缀/类型校验 |
| `option_graph.h` | `OptionGraph` / `EdgeKind_E` / `Node` / `Edge` / `addEdge` / `setNodeState` / `isNodeEnabled` / `evaluate` / `topologicalSort` | 选项关系有向图：条件/依赖边的记录与求值 |
| `option_parser.h` | `OptionParser` / `parse` / `recognize` / `apply` / `__assign` / `getOption` / `isFired` / `isOptionsEnded` / `setGraph` | 纯解析层：赋值、标记、记录 action 顺序、内置选项即时回调 |

grep 锚点（2026-10-25 核对）：`grep -n "void __assign" include/cmd_args/option/option_parser.h` 命中 528；`grep -n "void recordParseOrder" include/cmd_args/option/option_data.h` 命中 343；`grep -n "auto topologicalSort" include/cmd_args/option/option_graph.h` 命中 104；`grep -n "void execute" include/cmd_args/expression/executor.h` 命中 52。行号仅作稳定锚点，重构后需重新核对。

## 决策

- 值选项与短回路选项分两个管理器：短回路命中即停、值选项可重复赋值，语义不同；代价是名字查重要同时覆盖两者，`empty` / `usageString` 也需合并两容器。
- `OptionParser` 只解析不执行（内置选项除外）：把用户 action 延迟到 finalize 之后，确保校验失败不产生子进程副作用；被否方案是解析期直接执行，代价是半解析状态也可能触发命令。
- `recordParseOrder` 按 key 去重：避免同一选项重复触发子进程，并让 Executor 使用最终值；代价是失去「同一选项不同值分别执行」的能力，也与 README/option.h 注释的「每次执行」不一致。
- `--` 终止选项区且 `evaluate()` 只在非 `--` 分支调用：`--` 后全部 token 交给位置参数；代价是关系求值被跳过。
- 内置交互选项用 `bBuiltin__` + 解析期内联回调，而不是统一走 Executor：`--load-config` 必须在方位参数数量校验前导入配置，Executor 的批次位置太晚；代价是内置选项无法享受统一的 action 去重。
- `OptionGraph` 早期设计为「注册期建边、解析期置状态、Executor 前拓扑排序」，实际 Executor 未接线，拓扑排序仅供测试；保留图结构是为后续条件/依赖执行预留（属已知未完成项，不是当前行为）。

## 教训

- 文档与实现冲突（批量 action）：README_zh.md 的语义约定与 option.h:148-149 注释写「同一选项多次命中则每次执行」，但批量实现按 key 去重且只执行一次（option_data.h:343-347、test_option.cpp:66-86）；规避方式是以测试与实现为准并修正措辞，不要把该注释当作行为契约。
- `--opt=` 不能表示空串：内联值为空会被当作「缺值」，转而吞下一个 token 或抛 `NEED_VALUE`（option_parser.h:144-153）；规避方式是用 YAML/env 或显式赋值接口传空值。
- 短选项取值不检查 `-` 前缀：`-t --verbose` 会把 `--verbose` 当作 `-t` 的值，而长选项 `--t --verbose` 会抛 `NEED_VALUE`（option_parser.h:147 与 216-221 的不对称）；规避方式是在测试中锁定该行为，命令行避免歧义写法。
- 聚合短选项的截断：`-abc` 中若 `a`/`b` 是 bool 会先逐个赋值，遇到非 bool 字符立即以其后余串为值并结束本 token 的解析，后续字符不再处理（option_parser.h:166-197）；规避方式是不在聚合串中混写标志与取值。
- 关系语法当前无运行时效果：`-a?b:c` / `-a->b` 会写入图并在 `evaluate()` 中激活节点，但返回列表被丢弃，Executor 也不读节点状态，因此不改变任何执行（compiler.h:207、option_graph.h:136-161）；规避方式是不要依赖关系语法实现条件执行。
- `topologicalSort` 对环静默丢节点：只在 BFS 中输出入度归零的节点，环上节点无错误地消失（option_graph.h:104-132）；规避方式是注册期自检环，或改用带剩余节点检测的实现。
- `Dependency` 边不参与 `evaluate()`：依赖边只影响拓扑序，而拓扑序当前无人消费，`-a->b` 不会让 a 先于 b 执行（option_graph.h:140-158）。
- `onOptionAssigned` 是空钩子且全仓无 override：`__assign` 每次赋值都会调用，但没有任何效果（option_parser.h:551-555）；流式执行走 Interpreter 自己的 `__executeOptionImmediate`，扩展执行语义不要指望该钩子。
- 内置选项只设 action 模板不会即时执行：即时分支要求 `bBuiltin__ && fnOnSet__`，只有模板时仍会记入 `actionOrder` 交给 Executor（option_parser.h:538-548）；规避方式是同时设置 `bBuiltin__` 与 `onSet` 回调，并在测试中断言时序。
- `getOption<T>` 类型必须精确匹配：bool 选项只能取 `bool`，其它类型串不匹配抛 `UNKNOWN_OPTION`（option_parser.h:314-323）；规避方式是使用 `getOptionInt` 等便捷入口或注册时保持类型一致。
- `OptionData::mapShortNameIndex` 是预留字段（option_data.h:340）：全仓没有读写点，短名查找实际走 `mgrValues.find(char)`；不要依赖该索引做短名到长名的映射。
