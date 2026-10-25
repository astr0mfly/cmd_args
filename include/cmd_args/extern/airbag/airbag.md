# airbag — vendored Windows 崩溃兜底头文件（minidump / SEH / terminate）

> **摘要**：本目录原样收纳 airbag 库的 5 个 Windows 专用头文件：把纯虚调用、SEH 结构化异常、CRT 无效参数、`std::terminate` 统一接管，并在系统故障时用 `MiniDumpWriteDump` 落盘 minidump。它不属于 cmd_args 的业务代码，只为 `exception_handler.h` 的 `exception_init` 提供底层能力，使示例程序崩溃时可诊断。
> **联系**：上级目录文档见 [extern.md](../extern.md)，库级索引见 [cmd_args.md](../../cmd_args.md)，许可证与来源见 [README_zh.md](../../../../README_zh.md) 的"## 来源与参考"。唯一使用方见 [error/exception_handler.h](../../error/exception_handler.h)；示例中的静态实例见 [example/once/main.cpp](../../../../example/once/main.cpp)。同级的另一组 vendored 代码见 [magic_enum/magic_enum.md](../magic_enum/magic_enum.md)。

## truth

5 个文件全部位于 `include/cmd_args/extern/airbag/`，均为 `#pragma once` + MIT 全文头 + `namespace airbag`：

| 文件 | 行数（`wc -l`，2026-10-25） | BOM | 平台条件 | 关键类型 |
| --- | --- | --- | --- | --- |
| `minidump.hpp` | 186 | 无 | `_WIN32`，否则 `#error Unsupported system`；内容再受 `#if !defined(__MINGW32__) \|\| !defined(__MINGW64__)` 包裹 | `airbag::minidump` |
| `process_error.hpp` | 138 | 有 | 同上 | `airbag::process_error` |
| `stop_request.hpp` | 117 | 有 | `_WIN32` 且非 MinGW，否则空命名空间 | `airbag::stop_request` |
| `system_failure.hpp` | 180 | 无 | `_WIN32`，否则 `#error Unsupported system` | `airbag::system_failure` |
| `thread_error.hpp` | 148 | 有 | `_WIN32`，否则 `#error`；内容受 `#if !defined(__MINGW32__) && !defined(__MINGW64__)` 保护 | `airbag::thread_error` |

关键公共签名（行号为 2026-10-25 核对）：

- `system_failure.hpp:59` `struct system_failure`：`using code_type = DWORD`、`using info_type = _EXCEPTION_POINTERS`、`static constexpr auto module_name_capacity = 63`；默认构造、`explicit system_failure(code_type)`、`explicit system_failure(_EXCEPTION_POINTERS *)`；`code()`、`info()`、`module_name()`、`title()`。`title()` 把已知 NTSTATUS 映射为英文描述，未知返回字面量 `"Unknwon"`（上游拼写，原样保留）；`module_name()` 取异常地址所属模块的文件名（截断到 63 字节）。
- `process_error.hpp:59` `class process_error`：`enum kind { undefined, pure_call }`、`static char const *title(kind)`；`using pure_call_handler = std::function<void()>`、`using system_failure_handler = std::function<void(system_failure const&)>`；`on_pure_call(pure_call_handler) noexcept`、`static pre_system_failure(system_failure_handler) noexcept`。析构时恢复 `_CRT_ERROR` 报告模式、还原 purecall 处理器并 `RemoveVectoredExceptionHandler`。`system_failure_dispatcher` 对 `0xE06D7363`（MSVC C++ 异常）返回 `EXCEPTION_CONTINUE_SEARCH`，其余交给注册的回调后再继续搜索。
- `thread_error.hpp:61` `class thread_error : public std::runtime_error`：`using terminate_handler = std::function<void(char const *)>`；默认构造在 CRT 上安装 invalid parameter / SEH translator / terminate 三个钩子；`(std::string const&, system_failure const&)` 与 `(char const*, system_failure const&)` 构造携带失败信息；`failure()`、`on_terminate(terminate_handler)`。
- `minidump.hpp:72` `struct minidump`：`using path_type = std::filesystem::path`；`static std::error_code last_error()`；默认构造把 dump 目录设为"可执行文件父目录 / crash"，`explicit minidump(path_type const &dir)` 可指定目录；`directory(path_type const&)`、`directory()`、`bool generate(system_failure const&)`。`generate` 目录不存在时先 `create_directories`，文件名格式 `<exe>-<年>_<月>_<日>-<时>_<分>_<秒>.dmp`，dump 类型为 `MiniDumpWithPrivateReadWriteMemory | MiniDumpWithDataSegs | MiniDumpWithHandleData | MiniDumpWithFullMemoryInfo | MiniDumpWithThreadInfo`；失败返回 `false`，错误经 `last_error()` 读取。
- `stop_request.hpp:52` `class stop_request`：`static void take_care()`（Windows 走 `SetConsoleCtrlHandler`）、`static bool signaled()`、`static void signal()`、`static void processed()`；两个 `static std::atomic_bool` 成员 `signaled_` / `processed_` 在 112-113 行 inline 定义。

数据流（`exception_handler.h:194` 起的 `exception_init`）：构造 `process_error` / `thread_error` / `minidump` 三个成员 → `process_error.on_pure_call` 打印纯虚调用提示 → `process_error.pre_system_failure` 注册向量化异常处理器，回调打印 `Fatal that %s at %s, generating minidump at '%s'` 并调用 `minidump.generate(f)`，失败再打印 `Unable to generate minidump: %s` → `thread_error.on_terminate` 打印线程终止信息与 backtrace。该注册把回调存进 `process_error` 的静态成员，而 lambda 捕获了 `exception_init` 实例的 `this`，实例生命周期必须覆盖整个进程。

本目录的唯一 include 点是 `error/exception_handler.h:35-38`，且只 include 了 `process_error.hpp`、`thread_error.hpp`、`system_failure.hpp`、`minidump.hpp`；`stop_request.hpp` 在仓库内零引用。

## 约束

- **不得修改这 5 个文件**：版权头、`#pragma once`、缩进（2 空格、`// clang-format off` 区间）、BOM、既有拼写均保持上游原样；升级只能整文件替换。
- **不得删除 3 个文件的 UTF-8 BOM**：`process_error.hpp`、`stop_request.hpp`、`thread_error.hpp` 带 BOM 是上游原样；xmake 在 Windows 平台强制 `/utf-8`（`xmake.lua:19`），能正确处理。
- **不得把 airbag 当跨平台库使用**：4 个头文件在非 `_WIN32` 时直接 `#error Unsupported system`，只有 `stop_request.hpp` 在非 Windows/MinGW 下退化为空实现。任何无条件 include 它们的公共头都会让库在非 Windows 上不可编译。
- **MinGW 不受支持**：`minidump.hpp:64`、`process_error.hpp:55`、`system_failure.hpp:53` 的 `||` 条件在 MinGW 下恒真（正确写法应是 `&&`，`thread_error.hpp:57` 才是对的），会把 MSVC 专有 API 引进编译。
- **只能在 `exception_handler.h` 的平台分支内使用**：`exception_init` 用 `#if defined(_WIN32) && !defined(__MINGW32__) && !defined(__MINGW64__)` 圈住成员与构造函数，非 Windows 下是空 struct。
- **不要在本目录加业务适配**：关闭 minidump、改写 terminate 行为都应改 `error/exception_handler.h`，不得动 vendored 文件。
- **`thread_error` 的默认构造依赖 `/EHa`**：它调用 `_set_se_translator`，而本仓库只有 Windows debug 配置加 `/EHa`（release 是 `/EHsc`），release 下的 SEH 转换兜底不能假设一定生效。

## 索引

| 文件 | 关键符号 | 职责 |
| --- | --- | --- |
| `minidump.hpp` | `airbag::minidump` / `generate` / `directory` / `last_error` | 创建目录并调用 `MiniDumpWriteDump` 写出 `.dmp` 文件 |
| `process_error.hpp` | `airbag::process_error` / `on_pure_call` / `pre_system_failure` / `system_failure_dispatcher` | 接管纯虚调用与 SEH，把系统故障转成 `system_failure` 回调 |
| `stop_request.hpp` | `airbag::stop_request` / `take_care` / `signaled` / `signal` / `processed` / `signaled_` / `processed_` | Ctrl+C / 控制台关闭事件转成可轮询标志 |
| `system_failure.hpp` | `airbag::system_failure` / `code` / `info` / `module_name` / `title` / `module_name_capacity` | 把错误码或 `_EXCEPTION_POINTERS*` 规整成统一失败描述 |
| `thread_error.hpp` | `airbag::thread_error` / `failure` / `on_terminate` / `terminate_handler` | 安装 CRT / SEH / terminate 钩子，并把失败包装成异常对象 |

grep 锚点（核对日期 2026-10-25）：

- `grep -rn "airbag/" include/ | grep -v extern/airbag` 只命中 `error/exception_handler.h:35-38`。
- `grep -rn "stop_request" include/ example/ dfx/` 只命中 `extern/airbag/stop_request.hpp` 自身。

## 决策

- **vendored 而非包管理引入 airbag**：cmd_args 本体是 header-only、无构建依赖；airbag 的 5 个头可直接随源码分发，读者 clone 即编译。代价是升级必须手工替换，且上游的 `||` guard 缺陷也一并继承。被否方案：用 vcpkg 引入——会给一个纯头文件库增加包管理器依赖。
- **选 airbag 而不是自己写 `MiniDumpWriteDump` 封装**：`minidump` 已处理目录创建、文件名、`MINIDUMP_EXCEPTION_INFORMATION` 组装与句柄关闭，且是 MIT；自写需要重复处理这些 Win32 边界。代价是引入 5 个 MSVC 定向头，以及它们对非 Windows 平台的 `#error` 行为。
- **只接入 4 个头，`stop_request` 不接线**：cmd_args 是库，不拥有进程级信号循环，交给自己轮询 `signaled()` 会让库语义变重；`exception_init` 只需"崩溃可留证据"，不需要优雅停机。被否方案：把 `stop_request` 也塞进 `exception_init`——它不解决崩溃诊断，且需要调用方配合 `processed()`。
- **不把 airbag 包进 `CMD_ARGS_NAMESPACE_BEGIN`**：vendored 代码保持上游命名空间 `airbag`，便于对比与整体替换；cmd_args 侧通过 `exception_handler.h` 的薄封装使用。代价是公共头里会出现 `airbag::` 前缀。
- **保留 `thread_error` 而不用自有异常类型**：它已能把 SEH / terminate / invalid parameter 统一为 `std::runtime_error` 派生对象并携带 `system_failure`；自写要重复实现三个 CRT 钩子。代价是强制依赖 `/EHa` 才能完整工作。

## 教训

- **airbag 的 guard 不是"跨平台安全"的**：`minidump.hpp`、`process_error.hpp`、`system_failure.hpp`、`thread_error.hpp` 在非 Windows 上走 `#else` 的 `#error Unsupported system`，而不是空实现；只有 `stop_request.hpp` 会退化为空命名空间。现象：在 Linux 上 include `exception_handler.h` 直接报 `Unsupported system`。规避：公共头做平台分支，或让使用方自带平台守卫。依据：`minidump.hpp:33-62`、`process_error.hpp:30-53`、`system_failure.hpp:25-51`、`thread_error.hpp:34-55`、`stop_request.hpp:28-50`。
- **MinGW 的排除条件是坏的**：三个文件写成 `#if !defined(__MINGW32__) || !defined(__MINGW64__)`，而 MinGW 只定义其中之一，`||` 恒真，MSVC 专有 API 仍被编入；`thread_error.hpp:57` 用 `&&` 才是正确写法。规避：不要把 MinGW 纳入支持矩阵；若必须支持，先整体替换 airbag 或加外部 guard。依据：`minidump.hpp:64`、`process_error.hpp:55`、`system_failure.hpp:53`、`thread_error.hpp:57`。
- **BOM 不能当噪声清理**：`process_error.hpp`、`stop_request.hpp`、`thread_error.hpp` 带 BOM，删掉会在下次整文件替换时反复冲突；`/utf-8` 已能吸收 BOM。依据：三个文件第 1 字节为 `EF BB BF`。
- **`stop_request` 既未被使用也有一半死代码**：仓库内零 include；且外层 guard 已排除非 Windows/MinGW，内层 `#if _WIN32` 的 `#else`（`signal(SIGINT, ...)`）永远不可达。规避：需要停机信号时不要指望它，自己按平台实现或改用 Win32 控制台回调。依据：`stop_request.hpp:28`、`stop_request.hpp:56-65`、`exception_handler.h:35-38`。
- **`exception_init` 的回调捕获 `this`，生命周期必须覆盖进程**：`pre_system_failure` 是静态注册，回调存进 `process_error` 静态成员，捕获的 `exception_init` 实例一旦析构，崩溃时回调即悬垂。规避：始终用 `static exception_init`（示例 `example/once/main.cpp:34` 的写法）。依据：`exception_handler.h:199-213`、`process_error.hpp:101-108`。
- **minidump 目录默认在 exe 旁而非当前目录**：默认构造把 `dump_dir_` 设为可执行文件父目录下的 `crash`，并在打印 "generating minidump at ..." 之后才在 `generate` 里创建目录；失败信息来自 `last_error()`（`GetLastError` + `std::system_category()`），不是文件系统错误码。依据：`minidump.hpp:77-100`、`minidump.hpp:105-114`、`exception_handler.h:201-215`。
