# argument — 命名参数与方位参数的数据、构筑与解析层

> **摘要**：argument 目录承担命令行「参数」一侧：`Argument` 值模型及 `ArgumentNamed` / `ArgumentPosition` 两个子类、命名/方位管理器、`ArgumentBuilder` 注册校验，以及 `ArgumentParser` 的 token 解析与类型化取值。它让 `--name value` 与裸 token（方位参数）拥有确定语义，并把「配置已赋值后 CLI 可省略、也可覆盖」落成数量区间校验。
> **联系**：上级索引 [cmd_args.md](../cmd_args.md)；同级选项面 [option.md](../option/option.md)；依赖 [expression.h](../expression/expression.h)、[context_fwd.h](../context/context_fwd.h)、[validator.h](../base/validator.h)、[util.h](../base/util.h)、[parser_error.h](../error/parser_error.h)、[builder_error.h](../error/builder_error.h)；编排方 [compiler.h](../expression/compiler.h)、[context_step.h](../context/context_step.h)、[interpreter.h](../expression/interpreter.h)；使用方 [once/main.cpp](../../../example/once/main.cpp)、[deduce/main.cpp](../../../example/deduce/main.cpp)；属性载体 [property.h](../property/property.h)；单测 [test_parser.cpp](../../../dfx/unit_test/test_parser.cpp)、[test_property.cpp](../../../dfx/unit_test/test_property.cpp)、[test_builder.cpp](../../../dfx/unit_test/test_builder.cpp)；README 见 [位置参数语义](../../../README_zh.md#位置参数语义) 与 [目录结构](../../../README_zh.md#目录结构)。

## truth

**类层次**：`Argument` 同时继承 `Expression` 与 `ValidatorMixin<Argument>`（argument.h:45）；三个构造函数都向 `Expression` 传 `ExpressionKind_E::Argument`（argument.h:58-79），因此 `kind()` 恒为 `Argument`，名字与帮助由基类存储，`Argument` 自身不复写 `getName()` / `getHelp()`。`ArgumentNamed` 只补构造与 `using` 声明；`ArgumentPosition` 另带 `Seq_T iSeq`、`No(Seq_T)`（设置序号并返回自身）、`getSeq()`、`operator<`（argument.h:418-449）。

**状态与不变量**：
- 值、默认值与读写钩子收在一个私有属性 `Property<Value_T> propValue__` 里（argument.h:384）；类型、环境变量名与状态标记仍是独立成员：`strType__`、`strEnvName__`、`bDefaultExpr__`、`bExternal__`、`bEnvHit__`（后两者 mutable）。属性契约见 [property.md](../property/property.md)。
- `set` 经属性写钩子赋值（`__setChecked`，argument.h:373-379）：钩子拒绝时抛 `ErrorValidation(ValidationError_E::VALIDATION_FAILED, "argument <name>: <msg>")`，值保持不变；`setExternal` 走 `propValue__.setRaw` 旁路并置 `bExternal__ = true`；`env` 只绑定变量名，读取环境变量由 seed 阶段完成；`reset` 委托 `propValue__.reset()`（惰性默认值重算 + 还原 + 清属性错误与 `bSet__`），随后清 `bExternal__` / `bEnvHit__`；`defaultExpr` = `clearDefaultFn` + `defaultValue(_Expr)` + `reset()` 并置 `bDefaultExpr__`。
- 谓词：`hasValue()` / `operator bool` 看 `propValue__.raw()` 是否非空；`isSatisfied() = bExternal__ || bEnvHit__ || hasValue()`；`usesDefault()` 以值比较判定（`!bExternal__ && !bEnvHit__ && propValue__.raw() == propValue__.defaultValue()`）；`hasDefault()` 只看默认串非空；`getType` / `getEnvName` 只读，`getValue()` 按值返回并经过读钩子（argument.h:308），`property()` 暴露属性载体（argument.h:311-314）。
- 数据源接线（compiler.h:256-290）：env seed 走 `set`，因此会被写钩子拦截；`${VAR}` 展开走 `property().setRaw`，属派生值旁路。

**管理器契约**（argument_data.h:32-59）：`ArgumentMgrItf` 声明 8 个纯虚接口 `doGetUsage` / `doFind` / `doEmpty` / `doSize` / `doVisit` / `doGetHelp` / `doGet` / `doPush`，没有虚析构。`NamedArgumentMgr` 用 `std::map<Name_T, Argument::ptr>`，`doPush` 以 `emplace` 写入，同一名字第二次写入被静默丢弃；另提供 `doMutVisit` / `doConVisit` / `mutGet`。`PositionArgumentMgr` 用 `std::map<size_t, Argument::ptr>`，键取自 `dynamic_cast<ArgumentPosition *>(_Arg.get())->getSeq()`（未判空），另提供 `mutGetBySeq`，其 `doGet(name)` 线性扫描值域。两实现的 `doVisit` 都用 try/catch 包裹访问器，捕获后打印到 stderr 并吞掉异常。`ArgumentData` 仅是 `mgrName + mgrPos` 的组合（argument_data.h:282-288），由 `Model` 的 `args` 成员持有。

**构筑**：`ArgumentBuilder::build<T>` 按 `if constexpr` 分派（argument_builder.h:53-70）：`ArgumentNamed` 先 `check`（空名抛 `BuildError_E::NEED_NAME`、空帮助抛 `NEED_HELP`）再 `__checkDuplicate`（已存在抛 `DUPLICATE_NAME`）；`ArgumentPosition` 直接以 `mgrPos.doSize()` 作 `iSeq` 写入，不校验名字/帮助且允许重名；其余类型落到永不失败的 `static_assert(true, ...)`。另有 `build(ArgumentNamed&&/const&)`、`build(ArgumentPosition&&/const&)`（强制重设 `iSeq`）与 `build(Group<ArgumentNamed>&&)`（逐条转调）。

**解析**：`ArgumentParser::parse(Tokens_T&)` = `parseNamedOnly`（仅 `--` 开头且已注册的命名参数；`--k=v` 的 v 为空时吞下一个 token 作值，无下一个抛 `NEED_VALUE`；未知长名留给上层）→ 收集所有非 `-` 开头 token 作方位候选 → 数量区间校验 → 按序 `set` → 删除已消费 token。`parsePositionalOnly` 把 `--` 之后的全部 token（含 `-` 开头）当作方位参数；`assignPositional` 为类型化/流式路径复用同一规则。`parse(std::string)` 处理 `KEY=VALUE` 单行，未知名字抛 `UNKNWON_ARGS`。`getArgument<T>` 先查命名再按名线性查方位，类型串非空且不等于 `util::type_string<T>()` 抛 `UNKNWON_ARGS`，随后 `util::parse_value<T>`；便捷入口为 `getArgumentInt` / `getArgumentInt64` / `getArgumentDouble` / `getArgumentString`。`isParsed(name)` 只查命名管理器且要求非空值，位置参数恒为 false。

**方位参数数量区间**：`parse` / `parsePositionalOnly` / `assignPositional`（以及 interpreter.h:456-479 的第四份）使用同一规则——`total = mgrPos.doSize()`，`need = 未 isExternal() 的位置参数个数`；当 `vecPos.size() < need || vecPos.size() > total` 抛 `ParseError_E::UNKNWON_ARGS`，消息形如 `give N, but need A..B`；当 `size == need` 时跳过 `isExternal()` 的位，否则按 `iSeq` 顺序覆盖外部值。

**错误码**：构筑期 `BuildError_E::NEED_NAME` / `NEED_HELP` / `DUPLICATE_NAME`（完整枚举见 builder_error.h）；解析期 `ParseError_E::UNKNWON_ARGS`（历史拼写，不是 UNKNOWN）与 `NEED_VALUE`。

## 约束

- 头文件式：实现全部位于 .h（README_zh.md:3、318），本目录禁止引入新的编译单元。
- C++20（xmake.lua:11 `set_languages("c++20")`）；改动须同时满足 Windows 与 Linux，不得使用平台专属 API。
- 命名空间必须用 `CMD_ARGS_NAMESPACE_BEGIN` / `CMD_ARGS_NAMESPACE_END`（base/def.h:28-29），detail 命名空间实现后再 `using` 导出（argument.h:43、451-455）。
- 命名参数唯一性由 `ArgumentBuilder` 保证；绕过 Builder 直接 `doPush` 不受保护（见教训）。
- 方位参数必须按 `iSeq` 递增注册（Builder 用 `mgrPos.doSize()` 分配），手写 `ArgumentPosition` 时须显式给序号。
- 方位数量规则只允许 `[未外部赋值数, 总数]`，四个实现必须同步修改，不得单独改动某一入口。
- `ArgumentNamed` 必须有非空 name 与 help（Builder 校验）；`ArgumentPosition` 的名字不参与注册表查找，仅用于帮助与 `getArgument` 的线性查找。
- 值只能经 `Property` 访问：业务写入走 `set`（可能被写钩子拒绝并抛 `ErrorValidation`），只有配置/快照与 `${VAR}` 展开才允许 `setExternal` / `property().setRaw`；禁止重新引入公有值字段或绕过属性的写入口。

## 索引

| 文件 | 关键符号 | 职责 |
| --- | --- | --- |
| `argument.h` | `Argument` / `ArgumentNamed` / `ArgumentPosition` / `set` / `getValue` / `property` / `setExternal` / `reset` / `isSatisfied` / `usesDefault` / `defaultExpr` / `defaultFn` | 参数值模型：类型、`Property<Value_T>` 属性值（默认值 + 读写钩子）、环境变量绑定与外部/命中标记 |
| `argument_data.h` | `ArgumentMgrItf` / `NamedArgumentMgr` / `PositionArgumentMgr` / `ArgumentData` | 管理器接口与实现、参数仓库（命名字典 + 方位序号表） |
| `argument_builder.h` | `ArgumentBuilder` / `build` / `check` / `__checkDuplicate` | 注册入口：空名/空帮助/重名校验，方位序号分配 |
| `argument_parser.h` | `ArgumentParser` / `parse` / `parseNamedOnly` / `parsePositionalOnly` / `getArgument` / `isParsed` | token 解析、数量区间校验、类型化取值 |

grep 锚点（2026-10-25 核对）：`grep -n "class Argument " include/cmd_args/argument/argument.h` 命中 45；`grep -n "size_t total" include/cmd_args/argument/argument_parser.h` 命中 140 / 220 / 386；`grep -n "dynamic_cast<ArgumentPosition" include/cmd_args/argument/argument_data.h` 命中 169。行号仅作稳定锚点，重构后需重新核对。

## 决策

- 复用 `Expression` 承载 name/help/kind：使 `kind()` 分派与帮助渲染统一；代价是 `Argument` 不拥有独立名字存储，旧 AGENT.md 所称「getName 返回自有存储」与源码不符（已更正）。
- 命名参数用 `std::map` + `emplace`：查找 O(log n)、按名输出帮助天然有序；代价是管理器自身不防重，重复注册由 Builder 提前拦截。
- 位置参数用 `iSeq` 索引：方位语义只依赖声明顺序；代价是 `getArgument(name)` 需线性扫描，`isParsed` 干脆不支持位置参数。
- 数量校验用 `[need,total]` 而非「必须全部给齐」：支持配置 seed 后 CLI 可省略已赋值项；代价是规则在四个入口重复实现。
- `--` 后先 `parseNamedOnly` 再 `parsePositionalOnly`（compiler.h:200-204）：保留历史行为，使 `--` 后仍能消化命名参数；代价是 `--` 并不完全「停止命名参数解析」。

## 教训

- 重复注册静默丢参：`NamedArgumentMgr::doPush` 用 `emplace`（argument_data.h:69），绕过 Builder 直接注册时第二次写入无任何提示地丢失；规避方式是统一走 `ArgumentBuilder`。
- 位置管理器的空指针解引用：`PositionArgumentMgr::doPush` 对 `dynamic_cast` 结果不判空即调用 `getSeq()`（argument_data.h:167-171），传入 `ArgumentNamed` 会崩溃；规避方式是只经 Builder 或显式传 `ArgumentPosition`。
- `doVisit` 吞异常：访问器内抛出的校验错误被 try/catch 打印到 stderr 后吞掉（argument_data.h:92-104、200-212）；规避方式是把校验放在 Builder/parse 阶段，不依赖遍历传播错误。
- `usesDefault` 是值比较（argument.h:355-357，比较 `propValue__.raw()` 与 `propValue__.defaultValue()`）：CLI 显式给出与默认值相同的值时也会被判为「用了默认值」；规避方式是需要区分显式性时查 `bExternal__` / `property().isSet()`，而不依赖该谓词。
- 写钩子拒绝会立刻抛 `ErrorValidation`：`Argument::set` 不再是 `noexcept`，env seed 也会抛；`ContextBase::parse` 四个重载会转成 `ec = VALIDATION_FAILED`，但自定义调用链在 `loop` 之外直接 `set` 时必须自行捕获（argument.h:373-379、context_step.h:82-93）。
- `getValue()` 按值返回且经过读钩子（argument.h:308）：返回非空不代表「有值」（读钩子可造出 `(anonymous)`），`hasValue` / `isSatisfied` 仍看 `raw()`；用 `auto const &` 绑定会立刻悬垂，必须按值接。
- 必选参数与默认值冲突：`isSatisfied` 以 `hasValue()` 判定，非空默认值使 required 永不触发；规避方式是 required 参数不要给非空默认值（validator.h 的 required 语义）。
- `isParsed` 只查命名集合（argument_parser.h:284-288）：位置参数永远返回 false；规避方式是位置参数改用 `getArgument` 或 `hasValue` 判断。
- `argument_data.h` 依赖传递包含：该头自身没有 `<map>` / `<sstream>` / `<algorithm>` 等包含，单独包含可能编译失败，需先包含 argument.h 或走 cmd_args.hpp 聚合头。
- `--name=` 不能表达空串：`value.empty()` 分支会继续吞下一个 token，流末则抛 `NEED_VALUE`（argument_parser.h:196-205）；规避方式是不使用该写法传空值。
- 数量校验四份拷贝：argument_parser.h:140 / 220 / 386 与 interpreter.h:456 各一份，只改一处会导致批量、`--`、流式行为漂移；规避方式是改规则时四处同改并补测试。

