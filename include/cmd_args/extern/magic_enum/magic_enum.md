# magic_enum — vendored 枚举反射单头库（v0.8.1，MIT）

> **摘要**：本目录原样收纳 Neargye/magic_enum v0.8.1 的 3 个头文件，提供纯编译期的枚举反射：枚举值 ↔ 枚举名、枚举值列表、名称反查与 `std::format` 支持。cmd_args 只用其中一个能力——`scanner` 在扫描词法单元时用 `enum_cast<Token::KeyWord_E>` 判断某个标识符是否等于关键字枚举名，从而把"关键字"从手写字符串表里解放出来。
> **联系**：上级目录文档见 [extern.md](../extern.md)，库级索引见 [cmd_args.md](../../cmd_args.md)，许可证与来源见 [README_zh.md](../../../../README_zh.md) 的"## 来源与参考"。唯一使用方见 [expression/scanner.h](../../expression/scanner.h)；被匹配的枚举定义见 [expression/token.h](../../expression/token.h)。同级的另一组 vendored 代码见 [airbag/airbag.md](../airbag/airbag.md)。

## truth

3 个文件全部位于 `include/cmd_args/extern/magic_enum/`，均为 MIT 全文头 + `#pragma once` 之外的 `NEARGYE_*` include guard，命名空间为 `magic_enum`：

| 文件 | 行数（`wc -l`，2026-10-25） | include guard | 内容 |
| --- | --- | --- | --- |
| `magic_enum.hpp` | 1483 | `NEARGYE_MAGIC_ENUM_HPP` | 反射主实现（枚举值、名称、反查、可选/字符串视图工具） |
| `magic_enum_format.hpp` | 79 | `NEARGYE_MAGIC_ENUM_FORMAT_HPP` | `std::formatter` 特化，依赖 `__cpp_lib_format` |
| `magic_enum_fuse.hpp` | 89 | `NEARGYE_MAGIC_ENUM_FUSE_HPP` | 多枚举融合（`fuse_one_enum`、`fuse_enum`、`typesafe_fuse_enum`） |

- 版本宏在 `magic_enum.hpp:35-37`：`MAGIC_ENUM_VERSION_MAJOR` 0、`MAGIC_ENUM_VERSION_MINOR` 8、`MAGIC_ENUM_VERSION_PATCH` 1，即 v0.8.1。版权头为 MIT，`Copyright (c) 2019 - 2022 Daniil Goncharov`。
- 数值扫描范围默认 `MAGIC_ENUM_RANGE_MIN` = -128（`magic_enum.hpp:93`）、`MAGIC_ENUM_RANGE_MAX` = 128（`magic_enum.hpp:99`），两者可在 include 前用宏覆盖；超出范围的枚举值不会被反射。
- 主要公共 API 与行号（2026-10-25 核对）：`enum_values`（1041）、`enum_index(E)`（1060）、`enum_index<V>`（1091）、`enum_names`（1156）、`enum_entries`（1162）、`enum_cast(underlying_type_t<E>)`（1172）、`enum_cast(string_view, BinaryPredicate = std::equal_to<>)`（1220）、`enum_name(E)`（1111）、`enum_name<V>`（1101）。
- `enum_cast(string_view)` 默认用 `std::equal_to<>` 逐个比较枚举名与输入，大小写敏感且要求完全相等；`is_flags` 的枚举还会按 `|` 拆段匹配。匹配失败返回空的 `optional`。
- `magic_enum_format.hpp` 在文件开头检查 `#if !defined(__cpp_lib_format)` 并直接 `#error "Format is not supported"`（第 35-36 行），随后才 include `magic_enum.hpp`。
- `magic_enum_fuse.hpp:42` `fuse_one_enum`、`magic_enum_fuse.hpp:52` `fuse_enum`、`magic_enum_fuse.hpp:62` `typesafe_fuse_enum`，全部依赖 `magic_enum.hpp`。

仓库内唯一使用点：

- `include/cmd_args/expression/scanner.h:29`：`#include "cmd_args/extern/magic_enum/magic_enum.hpp"`。
- `include/cmd_args/expression/scanner.h:365`：`auto k = magic_enum::enum_cast<Token::KeyWord_E>(_T.getName());`，返回 `optional`；有值则把扫描状态切到 `Phrase_E::IN_KEYWORD`，无值则把该词当普通变量。
- 被匹配的枚举是 `Token::KeyWord_E : char`，仅三个枚举项 `Env_`、`File_`、`Plugin_`（`expression/token.h:41-45`），底层值依次为 0、1、2。因此 `enum_cast` 实际只可能命中这三个字面量（含尾部下划线），不可能命中 `"Env"`、`"File"` 等不带下划线的写法。
- 仓库内没有其他文件 include `magic_enum_format.hpp` 或 `magic_enum_fuse.hpp`，二者不参与编译。

## 约束

- **不得修改这 3 个文件**：版权头、include guard 名、模板实现、缩进与写法保持上游原样；升级只能整体替换为对应版本的 3 个文件。
- **默认只 include `magic_enum.hpp`**：`magic_enum_format.hpp` 在标准库未实现 `__cpp_lib_format` 时是硬 `#error`，与调用点无关；确需格式化时先确认工具链支持 `std::format`。
- **匹配按枚举名精确比较**：枚举名包含尾部下划线，输入 `"Env"` 无法命中；新增关键字时改 `Token::KeyWord_E` 的枚举项，而不是在 `scanner` 里加字符串分支。
- **不要依赖反射的数值范围外行为**：默认只反射 [-128, 128] 内的枚举值；如枚举项超出范围，需要在 include `magic_enum.hpp` 前定义 `MAGIC_ENUM_RANGE_MIN/MAX`。
- **不得把 vendored 代码纳入本仓库的命名/注释规范**：`extern/` 保持上游风格（包含 `#pragma once`、`NEARGYE_` guard、`[[nodiscard]]` 等），不适用 `CMD_ARGS_NAMESPACE_BEGIN` 等约定。
- **不要在公共头里无节制扩散 magic_enum**：它是 1483 行模板头，include 点越多编译成本越高；目前只允许 `expression/scanner.h` 直接依赖，其他模块需要枚举名时经 `scanner` 的公共行为间接使用。

## 索引

| 文件 | 关键符号 | 职责 |
| --- | --- | --- |
| `magic_enum.hpp` | `magic_enum::enum_cast` / `enum_name` / `enum_names` / `enum_values` / `enum_entries` / `enum_index` | 枚举 ↔ 名称/值的编译期反射，是唯一被使用的文件 |
| `magic_enum_format.hpp` | `std::formatter` 特化 / `magic_enum::customize::enum_format_enabled` | 让枚举可直接用 `std::format` 输出（当前未 include） |
| `magic_enum_fuse.hpp` | `magic_enum::detail::fuse_one_enum` / `fuse_enum` / `typesafe_fuse_enum` | 把多个枚举值融合成单一哈希（当前未 include） |

grep 锚点（核对日期 2026-10-25）：

- `grep -rn "magic_enum/" include/ | grep -v extern/magic_enum` 只命中 `expression/scanner.h:29`。
- `grep -rn "enum_cast" include/ | grep -v extern/magic_enum` 只命中 `expression/scanner.h:365`。

## 决策

- **vendored 单头库而非手写"关键字 → 枚举"表**：`scanner` 需要按名字反查 `Token::KeyWord_E`；手写映射要在增删枚举项时同步改两处，容易漏。magic_enum 让映射随枚举定义自动生成，`enum_cast` 一行完成反查。代价是 1483 行模板头与相应编译期开销。
- **vendored 而非包管理**：与 airbag 同样的理由——cmd_args 是 header-only、零构建依赖的库，把 3 个头拷进仓库即可保证任何环境行为一致；代价是升级要手工替换、版本只能靠宏和版权头识别。
- **只接入主头，format/fuse 随包携带但不用**：保持上游发行版完整，未来需要 `std::format` 或融合枚举时不必再补文件；当前它们不参与编译，不产生运行时代价。
- **不把 `scanner` 的关键字匹配改成 `enum_cast(underlying char)`**：虽然 `KeyWord_E` 的底层类型是 `char`，但按名字字符串匹配更贴近词法分析语义；按值匹配会把任意同名数值当关键字。被否方案：在 `scanner` 内建 `if (name == "Env_") ...` 链——每加关键字都要改代码，且与枚举定义脱节。

## 教训

- **`enum_cast` 大小写敏感、且要求枚举名完全一致**：`Token::KeyWord_E` 的枚举名是 `Env_` / `File_` / `Plugin_`（带尾下划线），输入 `"Env"`、`"env"`、`"ENV_"` 都返回 `nullopt`，随后被 `scanner` 当作普通变量。规避：新增/调整关键字必须同步枚举名与词法用例。依据：`magic_enum.hpp:1220-1230`、`scanner.h:365`、`token.h:41-45`。
- **重命名枚举项不会编译报错，只会静默改变关键字识别**：名字来自枚举定义，拼写改动不触发任何编译诊断，直到运行时才发现某个关键字失效。规避：改 `KeyWord_E` 时同步跑 `dfx/unit_test/test_lexer.cpp` 里包含 Scanner 的用例。依据：`magic_enum.hpp:1111` 起的 `enum_name` 依赖 `__PRETTY_FUNCTION__` 解析枚举名。
- **不要为了"顺手支持格式化" include `magic_enum_format.hpp`**：在未实现 `std::format` 的标准库上它是编译期 `#error "Format is not supported"`，错误位置在第三方头里，容易被误判为项目配置问题。规避：需要时先检查 `__cpp_lib_format`，或只在明确支持 `std::format` 的配置里 include。依据：`magic_enum_format.hpp:35-36`。
- **默认反射范围有限**：`MAGIC_ENUM_RANGE_MIN/MAX` 默认 -128/128，若把关键字枚举改成带大数值的枚举，超出范围的项不会出现在 `enum_name`/`enum_values` 结果里，`enum_cast` 也无法命中。规避：在 include 前按需定义范围宏。依据：`magic_enum.hpp:90-99`、`magic_enum.hpp:127-139`。
- **`enum_cast` 对 flags 枚举按 `|` 拆段**：若未来把 `KeyWord_E` 改成 flags 风格，`"Env_|File_"` 这类输入会被接受，与当前"整词匹配关键字"的语义不同，需重新核对 `scanner` 行为。依据：`magic_enum.hpp:1229-1248`。
