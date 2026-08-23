# cmd_args

一个 C++20 的**头文件式**命令行解析库（`.hpp` 即用，无需编译），Apache-2.0。

主用于命令行的解析库已经有很多（argparse / cmdline / CLI11 / Lyra / Clara 等），
`cmd_args` 广泛参考并借鉴了几个库的思路，**尝试融入对环境变量的展示与交互**：
环境变量在这里是一等公民——可展示、可交互、可回写，而不仅仅是“读一眼”的孤儿功能。

## 管线

```
Context  ──build<>──►  Builder         （构筑 Argument / Option / Flag）
   │                    │
   │                    ▼
   │                 Lexer             （Scanner 状态机：argv → token 流）
   │                    │
   │                    ▼
   └──►parse()──►    Parser            （选项 / 命名参数 / 方位参数分配）
                        │
                        ▼
                      Model            （ArgumentData + OptionData + environment_store）
                        │
                        ▼
  读取 get_argument<T> / get_option<T> / parsed() / has_env() / env_string()
  执行 once()（解析完即退） / loop()（REPL 常驻）
```

## 快速开始

```shell
# 初始配置 + 构建（示例与单测目标默认不参与全量构建，需显式指定）
xmake
xmake build once         # example/once
xmake build unit_test    # dfx/unit_test（后续会并入 xmake test）

# 运行示例
xmake r once -- --hello hi --timeout 99 -v peter 3
```

## 用法示例

```cpp
#include "cmd_args/cmd_args.hpp"
#include "cmd_args/error/exception_handler.h"   // 可选：纯虚调用/崩溃时生成 minidump

using namespace cmd_args;

Context app("my_tool");

// 命名参数：--hello=world / --hello world
app.build<ArgumentNamed>("hello", "Say hello")
   .build<ArgumentNamed>("world", "Say world too");

// 方位参数：剩余 token 按声明顺序赋值（数量必须一致）
app.build<ArgumentPosition>("name", "your name")
   .build<ArgumentPosition>("count", "how many times");

// 选项与标志
app.add_option<std::string>('o', "output", "output file path", "out.txt");
app.add_option<int>("timeout", "timeout in seconds", 30);
app.add_flag('v', "verbose", "enable verbose output");

// 环境变量联动：命令行显式值 > 环境变量 > 内置默认值
app.option("output")->env("MY_TOOL_OUTPUT");
app.argument("world")->env("MY_TOOL_WORLD");

// 解析（错误进 std::error_code，不抛异常）
std::error_code ec;
app.parse(ec, _Argc, _Argv, _Envp);
if (ec) { /* ec.message() 即错误说明 */ }

// 读取结果
std::string hello = app.get_argument<std::string>("hello");
int         tout  = app.get_option<int>("timeout");
bool        verb  = app.get_option<bool>("verbose");
```

## 环境变量一等公民

### 展示（`-!` / `--env` 或 `env_string()`）

```
environments: my_tool
No. : "key" = "value"
0   : "PATH" = "..."
1*  : "MY_TOOL_OUTPUT" = "env-value"     (* 会话中被修改)
2>  : "MY_TOOL_WORLD"  = "hi"            (> 被程序引用过)
3*+ : "MY_TOOL_NEW"    = "x"             (+ 本次会话新建)
```

- `*` 被修改：已存在的变量值在本会话中被 `--env-set`/`set_env` 改过
- `>` 被引用：被用作兜底默认值，或被 `${VAR}` 展开命中
- `+` 新建：进程环境里原本不存在的变量被本次会话创建

### 交互（Context API / 内置选项）

| 能力 | 编程接口 | 命令行 |
| --- | --- | --- |
| 查看 | `env_string()` / `print_env()` | `-!` / `--env` |
| 读取 | `get_env` / `get_env_or` / `has_env` | — |
| 设置/新建 | `set_env(key, value)` | `--env-set KEY=VALUE`（= / 空格均可） |
| 删除 | `unset_env(key)` | `--env-unset KEY` |
| 展开 | `expand_env("${A} %B% $C")` | 解析完成后自动对取值展开 |
| 回写 | `apply_env()` | `--env-apply`（会话变更写回操作系统） |

### 值展开

解析完成后，所有参数/选项的取值统一做环境展开，支持三种写法：

- `${NAME}`（POSIX 风格）
- `%NAME%`（Windows 风格）
- `$NAME`

未命中的变量保持原样（不做静默替换）。

### 优先级

绑定 `env("VAR")` 后，取值优先级为：

```
命令行显式值  >  环境变量  >  内置默认值
```

示例：`--timeout 99` 显式给出取 99；未给出但进程有 `MY_TOOL_TIMEOUT` 取环境值；
都没有则取 `add_option<int>(..., 30)` 的 30。

### 内置选项

首次 `parse()` 时懒注册，命中即短路（打印后停止本次解析）：

- `-?` / `--help`     帮助
- `-!` / `--env`      环境变量快照
- `--env-set K=V`     设置/新建会话环境变量
- `--env-unset K`     删除会话环境变量
- `--env-apply`       把会话中的修改/新建回写到操作系统环境（Windows
                      用 `_putenv_s`，POSIX 用 `setenv`）

`set_builtin_options(false)` 可关闭内置选项，**必须在首次 parse 之前调用**。

## 命令行语法速查

- `--name=value` / `--name value` —— 命名参数
- 剩余非选项 token —— 方位参数，按声明顺序赋值
- `--` —— 之后的 token 一律视为方位参数（含 `-` 开头的）
- `-x value` / `-xvalue` —— 短选项；`-abc` —— 聚合短选项（bool）
- 值 token 保留原始字符：`out.txt`、`a=b`、`C:\path`、`${A}` 均原样传递
- 重复注册参数/选项名 → 构筑期抛 `ErrorArgument`（`DUPLICATE_NAME`）

## 错误处理

- 解析可走异常（`parse()` 抛出 `ErrorParse` / `ErrorLexer`）或
  `std::error_code`（`parse(ec, ...)`，不抛异常）
- 三类错误码：`ParseError_E` / `LexError_E` / `InterpError_E`，均已注册进
  `std::error_code`，可用 `ec.message()` 转可读文本
- `Errors` 堆栈：`dump()` / `getLast()` 保留最近一次错误详情

## 执行模式

- `once()` —— 解析成功后 `std::exit(退出码)`，一条命令跑完即退
- `loop()` —— REPL 常驻，标准输入逐行解析，`exit`/`quit`/EOF 退出

## 已实现 / 展望

当前已实现：命名/方位参数、值选项/标志/聚合短选项、`--` 分隔、短回路选项、
环境变量展示/交互/展开/回写、once/loop 模式、错误分类、异常捕获（airbag）。

仍在展望（本期未做，见计划约束）：
- `&&` / `||` 命令串联、`|` 管道、`...` 可重复项
- json 配置加载、lua/luajit action、dll 动态补丁
- bench / meta / mock 工程

## 编译与测试

```shell
xmake                  # 全量配置（示例/单测为 set_default(false)，需显式构建）
xmake build once
xmake build unit_test
xmake r once -- --hello hi --timeout 99 -v peter 3   # 跑示例
```

单元测试基于 vendored Catch2 v2，测试用例覆盖词法（token 归一化、KEY=VALUE 合并、
`-?`/`--`、未闭合字符串）、构筑期重名检查、解析错误码分类、环境变量
（展示格式与标记、三种展开、apply 回写、优先级）等，debug/release 均通过。

> MSVC 注意：源码含 UTF-8 中文注释，xmake 已为 Windows 平台追加
> `/utf-8` 编译选项，避免 GBK 代码页下触发 C4819。

## 目录结构

```
include/cmd_args/          头文件库本体（header-only）
├── base/                  def / util（类型字符串、解析、trim/split 等）
├── error/                 builder / parser / lexer / interpreter 错误分类
├── argument/              Argument / ArgumentNamed / ArgumentPosition + 数据/解析
├── option/                option_value / option_action + 数据/构筑/解析
├── env/                   environment / environment_store（一等公民核心）
├── context/               Context / Builder / Lexer / Parser / Model / Interpreter
└── extern/                vendored：airbag（崩溃兜底）、magic_enum
example/once/              once 模式示例
dfx/unit_test/             Catch2 单元测试
scripts/                   构建运行脚本（build.bat / clean.bat 等）
```

## 来源与参考

[Xmake 参考手册](https://xmake.io/#/getting_started)

[argparse](https://github.com/0382/util/tree/main/cpp/argparse)

[cmdline](https://github.com/tanakh/cmdline)

[CLI11](https://github.com/CLIUtils/CLI11)

[lyra](https://github.com/bfgroup/Lyra)

[Clara](https://github.com/catchorg/Clara)

[Catch 使用教程](https://github.com/catchorg/Catch2/blob/master/docs/tutorial.md)