# group — 分组声明与 help 分组渲染

> **摘要**：`group` 在声明选项的同时登记分组元数据，共 13 个选项/标志，其中 10 个分入“网络 / 输出 / 安全”三组，其余 3 个未分组项仍显示在 `Options:` 下。示例演示 `group(name, OptionDef<T>/FlagDef...)` 的批量形式与 `'\0'` 无短名选项，适合参数超过 10 个、help 需要按业务域组织的程序。
> **联系**：[example 总览](../example.md)｜对照 [declare](../declare/declare.md)、[chain](../chain/chain.md)｜头文件 [describe.h](../../include/cmd_args/context/describe.h)、[declarations.h](../../include/cmd_args/context/declarations.h)、[context_base.h](../../include/cmd_args/context/context_base.h)｜入口 [cmd_args.hpp](../../include/cmd_args/cmd_args.hpp)｜上级索引 [cmd_args.md](../../include/cmd_args/cmd_args.md)｜使用方 [unit_test.md](../../dfx/unit_test/unit_test.md)｜README [内置选项](../../README_zh.md#内置选项)

## truth

- 目录内仅 `main.cpp`（88 行）与 `xmake.lua`；target 名为 `group`，`set_default(false)`，Windows 下追加 `/EHa`（2026-10-25 核对）。
- 未分组声明（走 `declare`）：`OptionDef<std::string>{'i', "input", "input file to process", ""}`、`OptionDef<int>{'j', "jobs", "parallel jobs", 4}`、`FlagDef{'v', "verbose", ...}`。
- 三个分组（顺序即示例中的注册顺序）：
  - `app.group("网络", OptionDef<std::string>{'H', "host", "remote host", "localhost"}, OptionDef<int>{'p', "port", "listen port", 8080}, OptionDef<std::string>{'b', "bind", "bind address", "0.0.0.0"})`；
  - `app.group("输出", OptionDef<std::string>{'o', "output", "output file", "out.txt"}, OptionDef<std::string>{'f', "format", "output format", "json"}, FlagDef{'q', "quiet", ...}, FlagDef{'c', "color", ...})`；
  - `app.group("安全", OptionDef<std::string>{'\0', "cert", "TLS certificate file", ""}, OptionDef<std::string>{'\0', "key", "TLS private key file", ""}, FlagDef{'\0', "tls", "enable TLS"})`。
- `group(name, defs...)` 与 `declare(...)` 共用同一套折叠分发：`defs` 只接受 `OptionDef<T>` / `FlagDef`（分组是选项的展示属性，参数没有分组）。
- help 渲染（`context/describe.h`）：先按分组输出每组的 `"\n<name>:\n"` 与组内选项，再对未分组选项输出 `"\nOptions:\n"`；没有分组时全部选项都在 `Options:` 下。
- 分组只影响 help 展示，不改变解析：所有选项仍按长名/短名在整个 Context 内全局匹配，不存在“组内命名空间”。
- 解析：同其他示例的 `parse(ec, ...)` + `"(parse error) "` + `return ec.value()` 路径；失败不会进入结果打印。
- 成功输出 13 行，并按组用空行分隔：`input`/`jobs`/`verbose`；`host`/`port`/`bind`；`output`/`format`/`quiet`/`color`；`cert`/`key`/`tls`（`tls` 打印 on/off）。
- 运行命令：`xmake r group -- -i data.txt -H myhost -p 8080 -o log.txt -v`；`xmake r group -- -?` 可观察分组后的帮助版面。
- Windows 下分组名“网络/输出/安全”依赖根 `xmake.lua` 的 `/utf-8` 编译选项，否则 MSVC 可能报 C4819 或写出乱码。

## 约束

- `group()` 的 `defs` 只能是选项/标志声明；向分组传入参数声明会在折叠分发处编译失败。
- 短名可选，但聚合初始化不能跳字段：无短名时写 `'\0'`（示例“安全”组三项均如此），不能只写后三个字段。
- 分组名是展示元数据，不参与解析；不要依赖分组隔离同名短选项——短名/长名在 Context 内仍必须全局唯一，重复注册在构筑期抛 `DUPLICATE_NAME`。
- 未分组项必须继续可用；示例保留 `input`/`jobs`/`verbose` 来验证 `Options:` 兜底路径。
- target 固定 `group`、`set_default(false)`；只使用公共 API；源文件 UTF-8 无 BOM。

## 索引

| 文件 | 关键符号 | 职责 |
| --- | --- | --- |
| `main.cpp` | `group("网络", ...)`、`group("输出", ...)`、`group("安全", ...)`、`OptionDef<std::string>`、`OptionDef<int>`、`FlagDef`、`'\0'` 无短名、`parse(ec, ...)` | 未分组声明 + 三组批量注册 + 解析打印 |
| `xmake.lua` | `target("group")`、`set_kind("binary")`、`set_symbols("debug")`、`set_default(false)`、Windows `/EHa` | 构建 group 可执行文件，默认不构建 |

## 决策

- 按业务域分组（网络/输出/安全）而不是按类型分组（字符串/int/flag）：帮助文档按使用场景组织才对读者有用，类型信息已由 `<type>` 标注。
- 保留 3 个未分组项：验证 `Options:` 兜底分支，也说明分组不是“必须全部分组”的强制约束。
- “安全”组全部使用长名（`'\0'`）：证书/私钥通常由脚本生成，长名可读性更好，同时避免短名撞车；代价是命令行更长。
- 被否方案：用 `--net-host`、`--out-format` 之类前缀替代分组。那会改变选项名和用户输入，而分组只是 help 展示层，不应侵入解析语义。

## 教训

- `'\0'` 不能省略：`OptionDef`/`FlagDef` 是聚合体，字段按位置初始化；想跳过短名必须显式写 `'\0'`。
- 分组不是命名空间：不同组里出现相同短名或长名仍会在构筑期抛 `DUPLICATE_NAME`；设计大参数表时要全局查重。
- `-?` 打印分组帮助后，`main` 仍会继续执行并打印默认值；帮助不会终止进程。
- 中文字符串在 MSVC 下需要 `/utf-8`；缺失时表现为 C4819 警告（本项目开了 `set_warnings("all", "error")`，会直接失败）或运行期乱码。
- 帮助按注册的分组渲染；示例不依赖组间顺序，也不把组顺序当稳定接口。
- 行号会漂移，本文只锚定文件与符号。

