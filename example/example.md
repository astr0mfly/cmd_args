# example — 八种 API 风格的命令行示例集

> **摘要**：`example/` 是 cmd_args 的示例根目录，用 8 个独立可执行程序覆盖链式、声明式、类型推导、分组、配置持久化、once 执行与环境变量、校验、属性读写拦截等主要用法。所有 target 都不参与默认构建，需显式 `xmake r <target>` 运行；它们是展示公共 API 用法与边界的第一手材料，也是模块文档“使用方”的链接目标。
> **联系**：上级索引 [include/cmd_args/cmd_args.md](../include/cmd_args/cmd_args.md)｜入口汇总 [cmd_args.hpp](../include/cmd_args/cmd_args.hpp)｜子目录 [chain](chain/chain.md)、[config](config/config.md)、[declare](declare/declare.md)、[deduce](deduce/deduce.md)、[group](group/group.md)、[once](once/once.md)、[reactive](reactive/reactive.md)、[validate](validate/validate.md)｜使用方 [unit_test.md](../dfx/unit_test/unit_test.md)｜构建说明 [README_zh.md](../README_zh.md#编译与测试)｜target 声明 [example/xmake.lua](xmake.lua)、[根 xmake.lua](../xmake.lua)

## truth

- 目录结构：`example/xmake.lua` 通过 `includes(...)` 引入 8 个示例子目录；每个子目录一份 `main.cpp` + 一份 `xmake.lua`，连同聚合文件共 17 个文件（2026-10-25 核对）。
- target 一览：`chain`、`config_demo`、`declare`、`deduce`、`group`、`once`、`reactive`、`validation`。目录名与 target 名不一致有两处：`example/config` → `config_demo`，`example/validate` → `validation`；`example/reactive` 与 target 同名。
- 全部 target 的 `xmake.lua` 均含 `set_kind("binary")`、`set_symbols("debug")`、`set_default(false)`、`add_files("*.cpp")`、`add_includedirs("$(projectdir)/include", {public = true})`。`set_default(false)` 使它们不进入默认构建，必须显式指定 target。
- 运行格式：`xmake r <target> -- <程序参数...>`；`--` 之后的 token 原样传给示例程序，例如 `xmake r chain -- --name Alice --output result.txt -v`。也可先 `xmake build <target>` 再运行构建产物。
- 共同骨架：`Context app("<name>_demo")` → 构筑参数/选项 → `std::error_code ec; app.parse(ec, _Argc, const_cast<char const **>(_Argv), _Envp)` → `if (ec)` 打印错误并 `return static_cast<int>(ec.value())` → 用 `getArgument<T>`/`getOption<T>` 读取并打印。
- 错误前缀不统一：`chain`/`config`/`declare`/`deduce`/`group`/`once` 用 `"(parse error) "`；`validate` 用 `"(validation error) "` 并附加 `app.dump()`；`reactive` 用 `"(write rejected at parse) "`，先打印 `property().lastError()` 再打印 `app.dump()`。
- 退出方式：`chain`/`declare`/`deduce`/`group`/`validate` 由 `main` 返回 0；`once`/`config` 在末尾调用 `app.once()`，其实现是 `std::exit(nExitCode_)`（`include/cmd_args/context/context.h`），因此其后的 `return 0` 不会执行。
- 声明 API 的三种风格在同一份库上对照：链式 `build<...>().addOption<T>()`（chain）、`declare(ArgumentNamed/ArgumentPosition/OptionDef<T>/FlagDef)` 折叠分发（declare、group）、`addOption` 非模板推导（deduce）。`once`/`config` 侧重运行时能力，`validate` 侧重校验链，`reactive` 侧重 Property 写/读拦截。
- 构建开关：根 `xmake.lua` 设置 `set_languages("c++20")`、`set_warnings("all", "error")` 并 `includes("example")`；Windows 下追加 `/utf-8` 规避 C4819。`chain`/`declare`/`deduce`/`group`/`once` 的 `xmake.lua` 在 Windows 追加 `/EHa`；`config`/`validate`/`reactive` 没有该分支。

## 约束

- 示例只使用公共 API（`cmd_args/cmd_args.hpp`，以及 `once` 显式单独 include 的 `exception_handler.h`），不得依赖内部实现细节；库内部重构后示例应仍可编译。
- 所有 target 必须保留 `set_default(false)`；示例不是库的一部分，不能拖慢默认构建或进入发布产物。
- include 路径统一经 `add_includedirs("$(projectdir)/include")`，不允许相对源码目录的 `../..` hack。
- 仅需要在 Windows 启用异常展开的示例，才在对应 `xmake.lua` 的 Windows 分支加 `add_cxflags("/EHa", {force = true})`；不得全局强加。
- 示例源码为 UTF-8 无 BOM；Windows 下依赖根 `xmake.lua` 的 `/utf-8` 才能正确编译中文字符串（如 group 的分组名）。
- 不引入第三方依赖；`config` 示例会在当前工作目录写 `demo.yaml`，运行位置需可写。

## 索引

| 文件 | 关键符号 | 职责 |
| --- | --- | --- |
| `xmake.lua` | `includes(...)` 8 条 | 聚合引入 8 个示例子目录 |
| `chain/main.cpp` | `build<ArgumentNamed>` / `addOption<int>` / `parse(ec, ...)` | 链式 SPI 最小示例 |
| `chain/xmake.lua` | `target("chain")` / `set_default(false)` | chain 构建描述（Windows 加 `/EHa`） |
| `config/main.cpp` | `schema_string()` / `to_yaml()` / `save_config()` / `load_config()` | 三类输入源与 YAML 往返 |
| `config/xmake.lua` | `target("config_demo")` | config 构建描述（无 `/EHa`） |
| `declare/main.cpp` | `declare(ArgumentNamed, ...)` / `OptionDef<T>` / `FlagDef` | 声明式折叠分发 |
| `declare/xmake.lua` | `target("declare")` | declare 构建描述 |
| `deduce/main.cpp` | `addOption(name, desc, int)` / `std::string("out.txt")` | 非模板类型推导 |
| `deduce/xmake.lua` | `target("deduce")` | deduce 构建描述 |
| `group/main.cpp` | `group(name, OptionDef...)` / `FlagDef` | help 分组展示 |
| `group/xmake.lua` | `target("group")` | group 构建描述 |
| `once/main.cpp` | `option(...)->action(...)` / `set_env` / `apply_env()` / `once()` | once 执行模式与环境/action |
| `once/xmake.lua` | `target("once")` | once 构建描述（`/EHa` 有注释说明） |
| `reactive/main.cpp` | `onWrite` / `onRead` / `onChange` / `set` / `setValue` / `setRaw` / `reset` / `lastError` / `version` | Property 读写拦截与变更通知 |
| `reactive/xmake.lua` | `target("reactive")` / `set_default(false)` | reactive 构建描述（无 `/EHa`） |
| `validate/main.cpp` | `required()` / `range()` / `semantic()` / `defaultExpr()` / `requireEnv()` | 校验链与退出码 |
| `validate/xmake.lua` | `target("validation")` | validate 构建描述（无 `/EHa`） |

## 决策

- 按 API 风格切分子目录而非按业务功能：每个示例只讲一个 API 面，读者能单独运行、单独对照，避免一个大而全的 demo；代价是同一套参数模型在多个目录重复声明。
- target 名与目录名不完全一致（`config_demo`、`validation`）：沿用历史命名，文档以 target 名为运行依据，并在总览中显式映射，避免读者 `xmake r config` 失败。
- 全部 `set_default(false)`：示例不参与默认构建，避免把示例二进制带入发布物或拖慢 CI；代价是必须显式 `xmake r <target>`。
- 新增 `reactive` 而不是扩展现有示例：属性读写拦截是 `property.h` 的独立 API 面，混进 `chain`/`validate` 会同时掩盖构筑语法与统一校验的演示重点；代价是示例与 target 各加一。
- 被否方案：把示例合并进一个 `main` 用子命令分流。会掩盖每个风格的最小 API 面，且子命令机制本身不是这些示例要验证的对象。

## 教训

- `xmake r config` 会失败：目录名不是 target 名，必须用 `xmake r config_demo`；`validate` 同理要用 `validation`。
- `-?`/`--help` 只终止本次 parse，不终止进程：短回路回调打印帮助后，`main` 仍继续打印默认值并返回 0（`once`/`config` 则继续到 `once()` 才退出）。不要假设 `-?` 会阻止后续输出。
- 示例注释可能与源码行为漂移：`config/main.cpp` 把 `timeout` 说成“来自配置 arguments 分区”，实际 `to_yaml` 把它写在 `options.timeout`（见 [config](config/config.md) 教训）。
- 不要用行号做文档锚点：源码一动行号就失效；本目录文档只锚定文件与符号。
- `const char*` 字面量直接传给 `addOption` 会命中模板并推导出字符数组类型，构筑期抛 `UNSUPPORT_TYPE`；示例统一显式 `std::string` 或 `addOption<T>`（见 [deduce](deduce/deduce.md)）。
- Scanner 在 argv 路径会把含 `.` 的数值拆成多 token，`validate` 示例因此避开 `127.0.0.1`（见 [validate](validate/validate.md)）。
- `__expandEnvValues` 曾用 `getValue()`（过读钩子）取值再 `setRaw` 回写，把读钩子的默认值物化进存储（`level` 的 `info` 导致 `version()` 变 1）；现已改用 `property().raw()` 作展开源（compiler.h:275-290、interpreter.h:500-514），详见 [reactive](reactive/reactive.md) 教训。
- `getArgument<std::string>`/`getOption<std::string>` 经 `util::parse_value` 的 `istringstream >>` 只取首个空白分隔 token（util.h:107-115），多词值被截断；多词值用 `property().get()` / `raw()`。

