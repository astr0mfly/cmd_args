# extern — 第三方 vendored 头文件（airbag 崩溃兜底 + magic_enum 枚举反射）

> **摘要**：本目录收纳 cmd_args 唯一的两组第三方代码，均为原样 vendored 的头文件库：airbag 负责 Windows 上的纯虚调用、SEH、terminate 与 minidump 兜底，magic_enum 负责枚举名与枚举值之间的编译期反射。二者不进入构建 target，只被少量首发点按需 include，把"进程崩溃可诊断"和"关键字枚举可反查"这两件事以零构建耦合的方式带进库。
> **联系**：上级索引见 [cmd_args.md](../cmd_args.md)；子目录文档见 [airbag/airbag.md](airbag/airbag.md) 与 [magic_enum/magic_enum.md](magic_enum/magic_enum.md)。使用方：airbag 仅经 [error/exception_handler.h](../error/exception_handler.h) 的 `exception_init` 接入；magic_enum 仅经 [expression/scanner.h](../expression/scanner.h) 的 `enum_cast<Token::KeyWord_E>` 接入。许可证与来源说明见 [README_zh.md](../../../README_zh.md) 的"## 来源与参考"一节。

## truth

目录下没有直接从属 `extern/` 的源文件；全部代码位于两个子目录，结构如下：

| 路径 | 文件数 | 语言 | 许可证 | 版权头 |
| --- | --- | --- | --- | --- |
| `extern/airbag/` | 5 | C++17 头文件（`#pragma once`，内容再受 `_WIN32` 条件保护） | MIT（每个头文件内嵌全文） | `Copyright 2020 Andrei Ilin <ortfero@gmail.com>` |
| `extern/magic_enum/` | 3 | C++17 单头反射库 | MIT（每个头文件内嵌全文） | `Copyright (c) 2019 - 2022 Daniil Goncharov` |

- airbag 目录没有独立的 `LICENSE` 文件，许可证全文只存在于 5 个头文件的头部注释中；magic_enum 同理，只靠头文件内的 MIT 全文。
- airbag 的 5 个文件分别为 `minidump.hpp`（186 行）、`process_error.hpp`（138 行）、`stop_request.hpp`（117 行）、`system_failure.hpp`（180 行）、`thread_error.hpp`（148 行）（行数取自 `wc -l`，核对日期 2026-10-25）。`process_error.hpp`、`stop_request.hpp`、`thread_error.hpp` 带 UTF-8 BOM，`minidump.hpp`、`system_failure.hpp` 不带。
- magic_enum 的 3 个文件分别为 `magic_enum.hpp`（1483 行）、`magic_enum_format.hpp`（79 行）、`magic_enum_fuse.hpp`（89 行）。版本宏在 `magic_enum.hpp:35-37`：`MAGIC_ENUM_VERSION_MAJOR` 0、`MAGIC_ENUM_VERSION_MINOR` 8、`MAGIC_ENUM_VERSION_PATCH` 1，即 v0.8.1。
- 全部代码为 header-only，xmake.lua 中不存在 `extern` 相关 target，也不参与 `set_warnings("all", "error")` 之外的编译配置；使用方 include 时才实例化。
- airbag 的平台条件分两种：`minidump.hpp`、`process_error.hpp`、`system_failure.hpp`、`thread_error.hpp` 在 `#if defined(_WIN32)` 的 `#else` 分支里直接 `#error Unsupported system`，非 Windows 编译会失败而不是退化为空实现；只有 `stop_request.hpp` 在外层 `#if defined(_WIN32) && (!defined(__MINGW32__) && !defined(__MINGW64__))` 不成立时退化为空 `namespace airbag`。magic_enum 无平台条件。

仓库内的实际引用只有两处：

| 引用点 | 引用方式 | 作用 |
| --- | --- | --- |
| `include/cmd_args/error/exception_handler.h:35-38` | 无条件 `#include "cmd_args/extern/airbag/process_error.hpp"`、`thread_error.hpp`、`system_failure.hpp`、`minidump.hpp` | `exception_init` 的成员类型与注册回调 |
| `include/cmd_args/expression/scanner.h:29` | `#include "cmd_args/extern/magic_enum/magic_enum.hpp"` | `scanner.h:365` 用 `magic_enum::enum_cast<Token::KeyWord_E>(_T.getName())` 判断关键字 |

由此可见：airbag 的 `stop_request.hpp` 未被任何文件 include，magic_enum 的 `magic_enum_format.hpp` 与 `magic_enum_fuse.hpp` 也未进入编译；三者属于"随库携带但暂未接线"的代码。

## 约束

- **不得修改任何一个 vendored 文件**：包括版权头、include guard 名、BOM、缩进风格与既有拼写。升级只能整文件替换，且必须保持上游版本可追溯。
- **不得为消除 BOM 而重写文件**：3 个 airbag 头带 BOM 是上游原样，去 BOM 会产生与升级无关的 diff；若工具链报 BOM 相关告警，应在使用侧处理而不是改 vendored 文件。
- **airbag 仅限 Windows**：四个头文件在非 `_WIN32` 平台直接 `#error Unsupported system`，因此任何在非 Windows 上 include 它们的翻译单元都会编译失败（`exception_handler.h` 是无条件 include）；`stop_request` 还额外排除 MinGW，且只有它在非 Windows 时退化为空实现。
- **MinGW 不是受支持组合**：`minidump.hpp:64`、`process_error.hpp:55`、`system_failure.hpp:53` 用的是 `#if !defined(__MINGW32__) || !defined(__MINGW64__)`（应为 `&&`），MinGW 下该条件恒真，MSVC 专有 API 会被编进来；只有 `thread_error.hpp:57` 用了正确的 `&&`。
- **magic_enum 只 include `magic_enum.hpp`**：`magic_enum_format.hpp` 在 `__cpp_lib_format` 未定义时直接 `#error "Format is not supported"`，当前工具链不要 include 它。
- **不得为 vendored 代码新增依赖或改动包含顺序**：airbag 依赖 Windows SDK 头（`Windows.h`、`DbgHelp.h` 等），magic_enum 依赖标准库头；保持上游 include 顺序，避免宏污染。
- **本目录不写业务逻辑**：任何对第三方行为的适配（如关闭 minidump、替换 terminate handler）都必须放在 `include/cmd_args/error/` 侧，不得改 vendored 文件。
- **文档与索引以子目录为单位**：`extern/` 自身不承载源码，细节分别登记在 `airbag/airbag.md` 与 `magic_enum/magic_enum.md`。

## 索引

| 文件 | 关键符号 | 职责 |
| --- | --- | --- |
| `airbag/minidump.hpp` | `airbag::minidump`（`generate`、`directory`、`last_error`） | 用 `MiniDumpWriteDump` 在崩溃点落盘 dump 文件 |
| `airbag/process_error.hpp` | `airbag::process_error`（`on_pure_call`、`pre_system_failure`） | 纯虚调用与 SEH 异常的统一接管 |
| `airbag/stop_request.hpp` | `airbag::stop_request`（`take_care`、`signaled`、`signal`） | Ctrl+C / 控制台关闭信号转成可查询标志 |
| `airbag/system_failure.hpp` | `airbag::system_failure`（`code`、`info`、`module_name`） | 把 `DWORD` 或 `_EXCEPTION_POINTERS*` 规整为统一失败描述 |
| `airbag/thread_error.hpp` | `airbag::thread_error`（`on_terminate`、`failure`） | 安装 terminate / invalid parameter / SE 转换处理器 |
| `magic_enum/magic_enum.hpp` | `magic_enum::enum_cast`、`enum_name`、`enum_values` 等 | 枚举 ↔ 名称/值的编译期反射主实现 |
| `magic_enum/magic_enum_format.hpp` | `std::formatter` 特化 | 让枚举支持 `std::format`（当前未使用） |
| `magic_enum/magic_enum_fuse.hpp` | `magic_enum::detail::fuse_one_enum` 等 | 多个枚举合并反射（当前未使用） |

grep 锚点（核对日期 2026-10-25）：

- `grep -rn "extern/airbag" include/` 只命中 `error/exception_handler.h`。
- `grep -rn "extern/magic_enum" include/` 只命中 `expression/scanner.h`。

## 决策

- **选择 vendoring 而非包管理（vcpkg / Conan / FetchContent）**：cmd_args 本体是 header-only、零构建依赖的库，再多两个单头库可以整体拷入仓库，读者 clone 即可编译；代价是升级要手工整文件替换、无法用版本求解器约束。被否方案：把 magic_enum 作为可选依赖由使用方自行提供——会让 `scanner.h` 的编译期行为随环境漂移，且示例 target 需要额外配置。
- **airbag 而不是自写 minidump/SEH 代码**：`minidump.hpp` 等已把 `MiniDumpWriteDump`、`AddVectoredExceptionHandler`、`_set_se_translator` 的样板封装成可组合对象，且是 MIT；自写需要处理大量 Windows API 边界。代价是库被锁在 Windows，且其 `thread_error` 构造函数要求 `/EHa` 才能完整工作。
- **magic_enum 而不是手写"关键字字符串 → 枚举"映射**：`scanner` 需要按名字反查 `Token::KeyWord_E`，手写映射在增删枚举项时容易漏改；magic_enum 让映射随枚举定义自动生成。代价是单个 1483 行模板头、编译期成本，以及默认严格按枚举名匹配（含尾下划线）带来的脆弱性。
- **只拷入被使用的头文件（format/fuse 也拷了但未接线）**：保持上游发行版的完整三文件，便于后续启用 `std::format` 或融合枚举时不需要再补文件；当前它们不参与编译，不产生成本。
- **不改 vendored 代码以适配 cmd_args 的命名/注释风格**：本仓库有 `CMD_ARGS_NAMESPACE_BEGIN` 等自有约定，但 vendored 目录保持上游形态，便于对比与升级。被否方案：把 airbag 用命名空间宏包裹——会把上游代码改得不可 diff。

## 教训

- **`magic_enum::enum_cast` 默认精确且大小写敏感**：它按枚举名（这里是 `Env_` / `File_` / `Plugin_`，自带尾下划线）做 `==` 匹配，写成 `"Env"` 或 `"env"` 都会得到 `nullopt`，`scanner.h` 随后会把它降级为普通变量。依据：`expression/scanner.h:365`、`expression/token.h:41-45`。
- **重命名枚举项会静默改变关键字识别行为**：因为名字来自枚举定义而不是常量表，重构 `KeyWord_E` 时拼写错误不会编译报错，只会让关键字在运行时失效；改枚举必须同步核对 `scanner` 的识别用例。依据：`magic_enum.hpp:1220` 起的 `enum_cast(string_view)` 实现。
- **不要顺手 include `magic_enum_format.hpp`**：在未定义 `__cpp_lib_format` 的工具链上它是编译期 `#error`，错误信息只有 `"Format is not supported"`，与调用点无关，排查成本高。依据：`magic_enum_format.hpp:35-36`。
- **airbag 的 BOM 不是可随手清理的噪声**：BOM 属于 vendored 原文，删掉会在下次整体替换时反复出现冲突；同时 `/utf-8` 编译选项（xmake.lua 中 windows 平台强制）能正确处理 BOM。依据：`process_error.hpp`、`stop_request.hpp`、`thread_error.hpp` 文件头。
- **`stop_request` 形同虚设**：它已随 airbag 拷入但没有任何 include 点，`exception_handler.h` 只接入 process_error / thread_error / system_failure / minidump；另外其内部 `#if defined(_WIN32)` 的 `else signal(SIGINT)` 分支被外层非 Windows guard 挡住，永远不可达。依据：`exception_handler.h:35-38`、`stop_request.hpp:28` 与外层 `#if`。
- **airbag 的 `thread_error` 构造函数调用了 `_set_se_translator`**，MSVC 下该 API 只有 `/EHa` 才可靠工作；本仓库 xmake.lua 只在 Windows debug 配置使用 `/EHa`，release 是 `/EHsc`，因此 release 下 SEH 转换兜底能力不能假设一定生效。依据：`xmake.lua` 的 windows 平台分支与 `airbag/thread_error.hpp:61` 起的构造函数。
