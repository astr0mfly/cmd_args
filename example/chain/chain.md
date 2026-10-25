# chain — 链式 SPI 风格的最小示例

> **摘要**：`chain` 用一条链式表达式声明 3 个参数与 4 个选项，演示 cmd_args 最初的 `Builder` SPI 风格：`build<T>()`、`addOption<T>()`、`addFlag()` 均返回可继续串联的引用。程序走 `std::error_code` 解析通道而不抛异常，并把错误码直接作为进程退出码。
> **联系**：[example 总览](../example.md)｜同模型的其他风格 [declare](../declare/declare.md)、[deduce](../deduce/deduce.md)、[group](../group/group.md)｜头文件 [context_base.h](../../include/cmd_args/context/context_base.h)、[builder.h](../../include/cmd_args/context/builder.h)、[option_builder.h](../../include/cmd_args/option/option_builder.h)｜入口 [cmd_args.hpp](../../include/cmd_args/cmd_args.hpp)｜上级索引 [cmd_args.md](../../include/cmd_args/cmd_args.md)｜使用方 [unit_test.md](../../dfx/unit_test/unit_test.md)｜README [快速开始](../../README_zh.md#快速开始)

## truth

- 目录内仅两个文件：`main.cpp`（54 行）与 `xmake.lua`；target 名为 `chain`，`set_kind("binary")`、`set_symbols("debug")`、`set_default(false)`，Windows 下追加 `/EHa`（2026-10-25 核对）。
- 构筑序列（`main.cpp`）：
  - `Context app("chain_demo")`；
  - `build<ArgumentNamed>("name", "your name")`、`build<ArgumentNamed>("greeting", "greeting message")`；
  - `build<ArgumentPosition>("output", "output file")`；
  - `addOption<int>('t', "timeout", "timeout in seconds", 30)`；
  - `addOption<std::string>('o', "format", "output format", "json")`；
  - `addFlag('v', "verbose", ...)`、`addFlag('q', "quiet", ...)`。
- 链上各方法返回可继续串联的引用，因此从 `build<ArgumentNamed>` 到最后一个 `addFlag` 是一条完整语句；参数与选项的注册顺序就是声明顺序。
- 解析与退出：`std::error_code ec; app.parse(ec, _Argc, const_cast<char const **>(_Argv), _Envp);` 失败时向 `std::cerr` 输出 `"(parse error) " << ec.message()`，并 `return static_cast<int>(ec.value())`；成功打印 7 行后 `return 0`。
- 打印字段固定为 `name`、`greeting`、`output`、`timeout`、`format`、`verbose`（on/off）、`quiet`（on/off），值分别来自 `getArgument<std::string>` 与 `getOption<T>`。
- 未在命令行给出的字段保持声明时的默认值：`timeout=30`、`format=json`、两个 flag 为 false、字符串参数/方位参数为空串。
- `xmake r chain -- -?` 命中内置帮助短回路：`helpOf` 打印帮助，`Compiler::parse` 提前返回且不置错误；`main` 继续执行并打印默认值，最终返回 0（短回路不终止进程）。
- 运行命令（取自文件头注释）：`xmake r chain -- --name Alice --output result.txt -v`；`xmake r chain -- -?`。

## 约束

- target 固定为 `chain`，必须保留 `set_default(false)` 与 `add_files("*.cpp")`；示例不得进入默认构建。
- 只使用公共 API：`Context`、`ArgumentNamed`、`ArgumentPosition`、`addOption`、`addFlag`、`parse(ec, ...)`、`getArgument`、`getOption`；不 include 内部实现头。
- 源文件 UTF-8 无 BOM，C++20 编译；Windows 分支保留 `/EHa`（`chain`/`declare`/`deduce`/`group`/`once` 五个 target 一致）；确需调整时先验证 MSVC 构建，不要只改单个目录。
- 解析失败必须返回 `ec.value()`，不得吞掉错误或改成固定返回值；成功路径不得调用 `once()`，保持“正常返回 0”的示例语义。

## 索引

| 文件 | 关键符号 | 职责 |
| --- | --- | --- |
| `main.cpp` | `Context app("chain_demo")`、`build<ArgumentNamed>`、`build<ArgumentPosition>`、`addOption<int>`、`addOption<std::string>`、`addFlag`、`parse(ec, ...)` | 链式声明模型、解析、失败退出、结果打印 |
| `xmake.lua` | `target("chain")`、`set_kind("binary")`、`set_symbols("debug")`、`set_default(false)`、`add_includedirs("$(projectdir)/include", {public = true})`、Windows `/EHa` | 构建 chain 可执行文件，默认不构建 |

## 决策

- 选用链式风格作为“最小示例”：源码注释明确这是 cmd_args 最初的 API 风格，适合参数少、逻辑简单的场景；一条语句即可读完全部声明。
- 走 `parse(ec, ...)` 而非抛异常：演示非异常通道，并让 `return ec.value()` 直接把错误码映射为退出码；异常通道由其他示例与 README 覆盖。
- 字符串选项显式写 `addOption<std::string>`：避免 `const char*` 字面量命中模板推导后抛 `UNSUPPORT_TYPE`（见 [deduce](../deduce/deduce.md)）。
- 被否方案：改用 `declare()` 集中声明。该写法由 [declare](../declare/declare.md) 示例承担，chain 要保留对“链式”这一历史风格的对照价值。

## 教训

- `-?`/`--help` 不阻止后续逻辑：帮助打印后 `parse` 正常返回，`main` 仍会打印默认值并返回 0；调试脚本时不要把它当成“打印帮助并退出”。依据：`context/builtin_options.h` 注册的短回路回调 + `context/context.h` 的管线顺序。
- 库不替你加错误前缀：`ec.message()` 只有底层错误文本，示例自己输出 `"(parse error) "`；其他示例的前缀不统一，阅读输出时需按程序区分。
- `_Argv` 必须经 `const_cast<char const **>` 传给 `parse`，而 `_Envp` 原样传入；擅自改动签名会与 `ContextBase::parse` 的重载不匹配。
- 链式调用里写 `addOption('o', "format", "desc", "json")` 会在构筑期抛 `UNSUPPORT_TYPE`，不会等到解析；必须写 `addOption<std::string>` 或 `std::string("json")`。
- 行号随源码变动失效：本文只以文件与符号作锚点，不引用行号。

