# cmd_args — C++20 头文件式命令行解析库（include 根索引）

> **摘要**：`include/cmd_args/` 是 cmd_args 库的全部源码（header-only，Apache-2.0）。对外唯一入口为 `cmd_args.hpp`，它只 include 4 个 `context/` 头即可暴露 Context / Builder / 解析 / 环境变量一等公民 / YAML 配置 / 选项 action 全部能力。本目录共 9 个一级子目录：`argument`、`option`、`env`、`context`、`expression` 承担解析主线，`base`、`error`、`config`、`extern` 提供横切能力。
> **联系**：上级 [README_zh](../../README_zh.md)（中文主文档）与 [README](../../README.md)（英文镜像）；子目录文档 [argument](argument/argument.md)、[option](option/option.md)、[env](env/env.md)、[context](context/context.md)、[expression](expression/expression.md)、[base](base/base.md)、[config](config/config.md)、[error](error/error.md)、[extern](extern/extern.md)；使用方 [example/example.md](../../example/example.md)、[dfx/unit_test/unit_test.md](../../dfx/unit_test/unit_test.md)；工程提示词 [.dsh/Agent.md](../../.dsh/Agent.md)。

## truth

### 入口与包含关系

`cmd_args.hpp`（30 行，2026-10-25 核对）只有 4 条 include，无任何声明：

```cpp
#include "cmd_args/context/context.h"         // Context 门面（子命令 + 批量管线）
#include "cmd_args/context/context_step.h"    // ContextStep：批量 step 入口
#include "cmd_args/context/context_runtime.h" // ContextRuntime：流式 parse 入口
#include "cmd_args/context/factory.h"         // make_context(EngineKind_E, Name_T)
```

### 目录清单（9 个一级子目录）

| 目录 | 文件数 | 职责（一句话） | 文档 |
| --- | --- | --- | --- |
| `argument/` | 4 .h | 命名/方位参数：Argument + builder/data/parser | [argument.md](argument/argument.md) |
| `option/` | 5 .h | 值选项/标志/action + OptionGraph 关系图 | [option.md](option/option.md) |
| `env/` | 4 .h | 环境变量一等公民（EnvData/EnvBuilder/EnvParser） | [env.md](env/env.md) |
| `context/` | 13 .h | 门面与数据仓库：ContextBase/Context/Step/Runtime/factory/describe | [context.md](context/context.md) |
| `expression/` | 10 .h | 词法 → 识别 → 编译/解释 → 校验 → 执行 | [expression.md](expression/expression.md) |
| `base/` | 4 | def 宏、util 解析工具、RAII defer、validator | [base.md](base/base.md) |
| `config/` | 3 .h | YAML 配置存储/读写与参数桥接 | [config.md](config/config.md) |
| `error/` | 8 .h | 各阶段错误码 + 可选 airbag 崩溃兜底 | [error.md](error/error.md) |
| `extern/` | 2 目录 | vendored：airbag、magic_enum | [extern.md](extern/extern.md) |

### 解析管线（与 README_zh §管线一致）

```
Context ──build<>──► Builder（Argument / Option / Flag）
   │                    │
   │                    ▼
   │                 Lexer（Scanner 状态机：argv → token 流）
   │                    ▼
   └──►parse()──►    Compiler（三 Parser → Expression 分类入库 Model）
                        ▼
                      Model（ArgumentData + OptionData + EnvData；hasAction 门控）
                        ├──► Executor     批量：后置执行 user action
                        └──► Interpreter  流式：逐 Token 解析即执行
```

- 读取：`getArgument<T>` / `getOption<T>` / `isParsed()` / `has_env()` / `env_string()`
- 存储：`schema()` / `snapshot()` / `save_config()` / `load_config()`（YAML）
- 模式：`once()`（单次输入即退）/ `loop()`（REPL，每行等价一次 once 输入）

### 表达式层次（源码为准）

`expression/expression.h`（2026-10-25 核对）：

```cpp
enum class ExpressionKind_E : char
{
    Argument, Option, Env, Conditional, Dependency
};
```

`class Expression` 持有 `kind()`（非虚）、`getName()` / `getHelp()`（虚），派生类型经 `Model::expressions()` 以统一视图枚举。

### 构建与平台

- xmake 工程，C++20，header-only（无库编译产物）；根 [xmake.lua](../../xmake.lua) 定义公共 includedirs 与 target。
- target：`unit_test`（默认关闭）+ 7 个示例 target（`chain`、`config_demo`、`declare`、`deduce`、`group`、`once`、`validation`，全部 `set_default(false)`）。
- 支持 MSVC / GCC / Clang；Windows 使用 airbag 的 `exception_handler.h` 时需 `/EHa`。

### 外部依赖

- 仅 C++ 标准库 + 两个 vendored 头：`extern/magic_enum`（`expression/scanner.h` 用于 `enum_cast<Token::KeyWord_E>`）、`extern/airbag`（仅 `error/exception_handler.h`，可选）。
- YAML 由 `config/yaml.h` 自研子集实现，不依赖第三方 YAML 库。

## 约束

- **header-only**：不得新增需要单独编译的 `.cpp`/二进制依赖；新增需编译的代码即破坏「`.hpp` 即用」契约。
- **C++20**，双平台（Windows/MSVC、Linux/GCC/Clang）可编译；不得使用仅单平台可用的扩展。
- 所有公共符号必须落在 `CMD_ARGS_NAMESPACE_BEGIN` / `CMD_ARGS_NAMESPACE_END`（`base/def.h`）内，禁止手写 `namespace cmd_args {`。
- 命名：类型后缀 `_T`（`Name_T`/`Desc_T`）、枚举后缀 `_E`、成员 `__` 后缀、mutable 成员保留 `m_` 前缀、函数 `auto` 后置返回类型（详见 [cmd-args-code-style] 技能）。
- 公共接口不得以裸 `std::string` 承载表达式/Token 语义（见 `dsi-cmd` 技能的类型化 Token/Expression 约定）。
- `extern/*` 为 vendored 代码，**不得直接修改**；升级走整体替换并记录版本。
- 新增公共能力必须能经 `cmd_args.hpp` 到达（必要时补 include），否则视为不可用。
- 示例与单测 target 一律 `set_default(false)`，不参与全量构建。
- 禁止提交 `compile_commands.json` 等本地产物（仓库当前已有未跟踪副本，勿纳入版本管理）。

## 索引

| 文件/目录 | 关键符号 | 职责 |
| --- | --- | --- |
| `cmd_args.hpp` | 4 条 include | 唯一入口；无声明 |
| `argument/` | `Argument` / `ArgumentBuilder` / `ArgumentData` / `ArgumentParser` | 命名与方位参数 |
| `option/` | `Option` / `OptionBuilder` / `OptionData` / `ValueOptMgr` / `ActionOptMgr` / `OptionGraph` / `OptionParser` | 选项、标志、action、关系图 |
| `env/` | `EnvBuilder` / `EnvData` / `EnvParser` | 环境变量一等公民 |
| `context/` | `ContextBase` / `Context` / `ContextStep` / `ContextRuntime` / `Model` / `make_context` / `usageOf` / `helpOf` / `envOf` | 门面、工厂、描述生成 |
| `expression/` | `Expression` / `Token` / `Scanner` / `Lexer` / `ExpressionRecognizer` / `Compiler` / `Interpreter` / `Executor` / `Result` | 词法→识别→执行主线 |
| `base/` | `CMD_ARGS_NAMESPACE_BEGIN` / `ARRAY_SIZE` / `parse_value<T>` / `UniqueDefer` / `Validator` | 横切基础件 |
| `config/` | `ConfigStore` / `Parameter` / YAML 读写 | 第三类外界环境 |
| `error/` | `ErrorBuilder_E` / `LexerError` / `ParserError` / `InterpError_E` / `ValidationError_E` / `ErrorHandler` | 错误码与兜底 |
| `extern/` | `airbag` / `magic_enum` | vendored 依赖 |

## 决策

- **唯一入口只 include 4 个 context 头**：控制编译依赖面，让 `#include "cmd_args/cmd_args.hpp"` 成为稳定契约；代价是新增模块必须显式挂到该文件或 context 链上。
- **三类外界环境统一**（argv / env / YAML）：这是库的差异主线（README_zh 开头即声明），因此 `env` 单独成目录而不并入 option。
- **编译引擎与解释引擎并存**：批量解析走 `Compiler`（性能、可存 Model），交互 REPL 走 `Interpreter`（边解析边执行），二者共享 Token/Expression 类型。
- **文档命名改为「目录名.md」而非 `AGENT.md`**（2026-10-25，用户裁定）：与目录同名更原生、便于按目录查找；历史 5 篇 `AGENT.md` 已迁移为同名文档并删除旧文件，机制见 [.dsh/Agent.md §12](../../.dsh/Agent.md)。
- **不引入第三方 CLI 解析框架**：仅借鉴 argparse/CLI11/Lyra/Clara 的思路，保持零运行时依赖。

## 教训

- **README 与源码漂移**：README_zh 第 36–44 行「Expression 表达式类层次」只列 `Argument/Option/Env` 三种，而 `expression/expression.h:31-39` 实际定义 5 种（含 `Conditional`/`Dependency`）。改枚举时同步 README；阅读时以源码为准。
- **词法已知缺陷**：`Scanner` 会把含 `.` 的数值（如 `127.0.0.1`）拆成多个 token，`example/validate/main.cpp` 头注释有记录；写文档/示例时不要把它当合法单 token。
- **旧文档事实不可直接沿用**：迁移前 `expression/AGENT.md` 记录了并不存在的 `expression_classifier.h`，`context/AGENT.md` 只登记 4 个文件而目录实有 13 个；本次迁移后须以源码重新核对。
- **统计数字易腐坏**：`.dsh/Agent.md` 曾写「111 TEST_CASE / 523 断言」，2026-10-25 实测为 166 / 818；引用数量必须注明核对日期。
- **不要用行号做文档锚点**：头文件重构频繁，行号会立刻失效；用符号名与文件名定位，必要时附核对日期。