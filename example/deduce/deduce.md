# deduce — addOption 的非模板类型推导

> **摘要**：`deduce` 对照展示 `addOption` 的三种非模板重载：从默认值参数推导 `int`、`double`、`std::string`，从而省去 `<T>`。示例同时保留显式模板与链式声明以证明可混用，并专门标注一个陷阱——字符串字面量是 `const char*`，不会被推导成 `std::string`。
> **联系**：[example 总览](../example.md)｜对照 [chain](../chain/chain.md)、[declare](../declare/declare.md)、[group](../group/group.md)｜头文件 [option_builder.h](../../include/cmd_args/option/option_builder.h)、[context_base.h](../../include/cmd_args/context/context_base.h)、[util.h](../../include/cmd_args/base/util.h)｜入口 [cmd_args.hpp](../../include/cmd_args/cmd_args.hpp)｜上级索引 [cmd_args.md](../../include/cmd_args/cmd_args.md)｜使用方 [unit_test.md](../../dfx/unit_test/unit_test.md)｜README [用法示例](../../README_zh.md#用法示例)

## truth

- 目录内仅 `main.cpp`（77 行）与 `xmake.lua`；target 名为 `deduce`，`set_default(false)`，Windows 下追加 `/EHa`（2026-10-25 核对）。
- 推导调用（`option_builder.h` 为 `int`/`double`/`std::string` 提供非模板重载，`T` 由默认值实参决定）：
  - `app.addOption("timeout", "timeout in seconds", 30)` → `int`；
  - `app.addOption("ratio", "ratio value", 0.75)` → `double`；
  - `app.addOption("output", "output file", std::string("out.txt"))` → `std::string`；
  - `app.addOption('j', "jobs", "parallel jobs", 4)` → `int`，带短名的形式同样走推导。
- 混用传统写法：`app.addOption<std::string>("format", "output format", "json")`（显式模板，字符串字面量作为默认值）与 `app.build<ArgumentPosition>("file", "input file")`。
- 陷阱机制：若写 `app.addOption("output", "output file", "out.txt")`，重载决议会选模板 `addOption<T>` 并得到 `T = char[N]`（比转成 `std::string` 更精确）；`base/util.h` 的 `type_string<T>()` 对字符数组返回 `"null"`，`option_builder.h` 随即抛 `ErrorArgument(BuildError_E::UNSUPPORT_TYPE)`。这是构筑期错误，不是解析期错误。
- 解析：`std::error_code ec` + `app.parse(ec, ...)`；失败输出 `"(parse error) " << ec.message()` 并 `return static_cast<int>(ec.value())`。
- 成功输出 8 行：`timeout`、`ratio`、`output`、`jobs`、`format`、`file`、`verbose`、`quiet`（后两者 on/off）；最后 `return 0`。
- 运行命令（头注释）：`xmake r deduce -- --timeout 60 --ratio 0.8 --output result.txt`；`xmake r deduce -- -?`；方位参数 `file` 直接作为位置实参传入，如 `xmake r deduce -- result.txt`（`--` 是 xmake 的参数分隔符）。
- 头注释给出的三组“传统 → 推导”对照：`addOption<int>(..., 30)` → `addOption(..., 30)`；`addOption<std::string>(..., "out.txt")` → `addOption(..., std::string("out.txt"))`；`addOption<double>(..., 0.5)` → `addOption(..., 0.5)`。

## 约束

- 字符串默认值必须写成 `std::string("...")`，或显式 `addOption<std::string>(...)`，或 `OptionDef<std::string>`；不得把裸字面量交给非模板重载。
- 非模板推导只覆盖 `int`、`double`、`std::string`；其他受支持类型（如 `int64_t`、`size_t`）仍需继续写 `addOption<T>`。
- 标志选项不参与类型推导：`addFlag` 的布尔语义由 API 固定。
- target 固定 `deduce`、`set_default(false)`；只使用公共 API；源文件 UTF-8 无 BOM。
- Windows 分支保留 `/EHa`，与同目录示例一致；不得因“本示例不抛异常”而删除。

## 索引

| 文件 | 关键符号 | 职责 |
| --- | --- | --- |
| `main.cpp` | `addOption(name, desc, int)`、`addOption(name, desc, double)`、`addOption(name, desc, std::string)`、`addOption('j', ...)`、`addOption<std::string>`、`build<ArgumentPosition>`、`parse(ec, ...)` | 推导调用 + 传统写法混用 + 解析打印 |
| `xmake.lua` | `target("deduce")`、`set_kind("binary")`、`set_symbols("debug")`、`set_default(false)`、Windows `/EHa` | 构建 deduce 可执行文件，默认不构建 |

## 决策

- 把三组写法放在同一文件与同一份头注释里对照，而不是拆成“传统版/推导版”两个目录：读者能直接看到 `int`/`double`/`std::string` 的差异只需改一个实参。
- 字符串仍显式构造 `std::string("out.txt")`，而不是依赖库去猜：这是当前类型系统的边界，示例应展示正确用法而非掩盖。
- 保留 `addOption<std::string>("format", ..., "json")` 与 `build<ArgumentPosition>`：证明推导写法不是替代整个 API，而是可选的简化层。
- 未采用：为 `const char*` 增加非模板重载。那会把字符数组静默转成字符串并放宽类型检查；库当前用 `type_string<T>() == "null"` 明确拒绝未支持类型，示例遵循这一契约。

## 教训

- 最容易踩的坑：`addOption("output", "output file", "out.txt")` 看似自然，实际在构筑期抛 `UNSUPPORT_TYPE`；错误信息不会提示“请用 std::string”，需要知道 `char[N]` 推导这一层。
- 该错误发生在 `addOption` 注册时，不是 `parse` 时；因此 `parse(ec)` 的 `error_code` 捕获不到，必须靠编译/运行前的构筑阶段暴露。
- `addOption<std::string>(..., "json")` 是安全的：显式模板参数固定 `T = std::string`，字面量作为实参正常转换；危险只出现在“省略模板参数 + 裸字面量”。
- `OptionDef<std::string>{...}` 也能避开陷阱（见 [declare](../declare/declare.md)），因为类型在结构体模板参数上已固定。
- 只对 `int`/`double`/`std::string` 有非模板重载；不要以为 `addOption("n", "desc", 3LL)` 之类也能推导。
- 行号会漂移，本文只锚定文件与符号。

