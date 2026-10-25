# declare — 声明式折叠分发一次性注册

> **摘要**：`declare` 用一次 `declare(...)` 调用集中注册 2 个命名参数、1 个方位参数、2 个值选项和 2 个标志，展示 `ArgumentNamed` / `ArgumentPosition` / `OptionDef<T>` / `FlagDef` 混合传入、由折叠表达式分发到对应 Builder 方法的声明式风格。随后又用链式 `addOption<int>(...).addFlag(...)` 追加两个声明，证明两种风格可以共存。
> **联系**：[example 总览](../example.md)｜对照 [chain](../chain/chain.md)、[deduce](../deduce/deduce.md)、[group](../group/group.md)｜头文件 [declarations.h](../../include/cmd_args/context/declarations.h)、[builder.h](../../include/cmd_args/context/builder.h)、[context_base.h](../../include/cmd_args/context/context_base.h)｜入口 [cmd_args.hpp](../../include/cmd_args/cmd_args.hpp)｜上级索引 [cmd_args.md](../../include/cmd_args/cmd_args.md)｜使用方 [unit_test.md](../../dfx/unit_test/unit_test.md)｜README [用法示例](../../README_zh.md#用法示例)

## truth

- 目录内仅 `main.cpp`（72 行）与 `xmake.lua`；target 名为 `declare`，`set_default(false)`，Windows 下追加 `/EHa`（2026-10-25 核对）。
- `Context app("declare_demo")` 后一次 `declare(...)` 传入 7 个声明（顺序即注册顺序）：
  - `ArgumentNamed{"name", "your name"}`、`ArgumentNamed{"greeting", "greeting message"}`；
  - `ArgumentPosition{"output", "output file"}`；
  - `OptionDef<int>{'t', "timeout", "timeout in seconds", 30}`；
  - `OptionDef<std::string>{'o', "format", "output format", "json"}`；
  - `FlagDef{'v', "verbose", "enable verbose output"}`、`FlagDef{'q', "quiet", "suppress output"}`。
- `declare(...)` 接受上述四类声明并混合传入；`context/builder.h` 的折叠分发把每类转给对应 Builder 方法，未覆盖的类型触发 `static_assert`（编译期失败，而不是运行期异常）。
- `OptionDef<T>` 的字段依次为 `shortName`、`longName`、`help`、`defaultValue`；`FlagDef` 只有 `shortName`、`longName`、`help`，出现即 true。
- 追加声明：`app.addOption<int>("retry", "retry count", 3).addFlag('d', "dry-run", "dry run mode")`；其中 `retry` 无短名，`dry-run` 用长名查询。
- 解析：`std::error_code ec` + `app.parse(ec, _Argc, const_cast<char const **>(_Argv), _Envp)`；失败输出 `"(parse error) " << ec.message()` 并 `return static_cast<int>(ec.value())`。
- 成功输出 9 行：`name`、`greeting`、`output`、`timeout`、`format`、`retry`、`verbose`、`quiet`、`dry-run`（后三者打印 on/off）；最后 `return 0`。
- 内置帮助：`xmake r declare -- -?` 打印帮助并让本次 parse 提前返回；`main` 仍继续打印默认值。
- 运行命令：`xmake r declare -- --name Alice --output result.txt -v`；`xmake r declare -- -?`。

## 约束

- `declare()` 只接受 `ArgumentNamed`、`ArgumentPosition`、`OptionDef<T>`、`FlagDef` 四类；传其他类型在编译期被 `static_assert` 拒绝。
- 每个 `OptionDef<T>` 必须显式写模板参数（如 `OptionDef<int>`、`OptionDef<std::string>`），并与默认值类型一致；`T` 不支持的类型会在 `addOption<T>` 层抛 `UNSUPPORT_TYPE`。
- 参数与选项的短名可以省略，但聚合初始化不能跳过字段：无短名时写 `'\0'`，不要只写后三个字段。
- 名字重复（同名参数/选项）在构筑期抛 `ErrorArgument(DUPLICATE_NAME)`；示例依赖唯一的长名与短名。
- target 固定 `declare`、`set_default(false)`，只使用公共 API，源文件 UTF-8 无 BOM。

## 索引

| 文件 | 关键符号 | 职责 |
| --- | --- | --- |
| `main.cpp` | `declare(ArgumentNamed, ArgumentPosition, OptionDef<T>, FlagDef)`、`OptionDef<int>`、`OptionDef<std::string>`、`FlagDef`、`addOption<int>`、`addFlag`、`parse(ec, ...)` | 声明式集中注册 + 链式追加 + 解析打印 |
| `xmake.lua` | `target("declare")`、`set_kind("binary")`、`set_symbols("debug")`、`set_default(false)`、Windows `/EHa` | 构建 declare 可执行文件，默认不构建 |

## 决策

- 用 `declare()` 集中声明 7 项，再追加两个链式声明：既展示“一处读完声明”的可读性，也证明声明式与链式不是互斥的两套 API。
- `OptionDef<T>` 全部显式写类型（`int` / `std::string`）：聚合体里默认值直接写 `"json"` 也不会触发 `addOption` 的模板推导陷阱，因为类型已由 `OptionDef<std::string>` 固定。
- 用 `addOption<int>("retry", ...)` 而不是再开一次 `declare()`：说明追加声明的现实用法；被否方案是把所有项都塞进一次 `declare()`，那样就看不出两种风格混用。
- 被否方案：引入宏 DSL（如 `OPTION(int, timeout, 30)`）。库未提供宏层，示例也不应发明一套只有示例使用的语法。

## 教训

- `declarations.h` 的 Doxygen 写“类型 T 从 defaultValue 推导”，但代码里没有任何省略 `T` 的写法，`OptionDef` 也没有显式推导指引；示例与文档一律显式 `OptionDef<T>`，不要把该注释当作可用语法。
- 聚合初始化顺序是 `shortName, longName, help, defaultValue`；把 `help` 与默认值写反会得到类型错误或语义错位，编译信息不一定直观。
- `declare` 只覆盖参数/选项声明，不覆盖环境变量绑定；环境条目仍要 `requireEnv`/`env` 单独声明（见 [once](../once/once.md)、[validate](../validate/validate.md)）。
- `dry-run` 查询用长名字符串 `getOption<bool>("dry-run")`；带连字符的长名不会被转成下划线。
- `-?` 打印帮助后进程继续运行，帮助不会阻止 `main` 的默认值打印；脚本里若只想看帮助，需自行忽略后续输出。
- 行号易变，本文只用文件与符号锚定。

