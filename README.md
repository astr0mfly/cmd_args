# cmd_args

一个 C++20 的**头文件式**命令行解析库（`.hpp` 即用，无需编译），Apache-2.0。

主用于命令行的解析库已经有很多（argparse / cmdline / CLI11 / Lyra / Clara 等），
`cmd_args` 广泛参考并借鉴了几个库的思路，**尝试把三类"外界环境"统一起来**：

1. **环境变量** —— 一等公民：可展示、可交互、可回写，而不只是"读一眼"的孤儿功能；
2. **实时命令行输入** —— 解析后可存储到环境变量或脚本；
3. **脚本文件（YAML 配置，JSON 未来）** —— build 完成后把参数/环境状态存为 YAML 供下次调用，调用时反哺回环境变量。

参数格式一经 `build<>` 确定，三类输入源之间即可**自然转换**（解析、存储、反哺）。

## 管线

```
Context  ──build<>──►  Builder         （构筑 Argument / Option / Flag）
   │                    │
   │                    ▼
   │                 Lexer             （Scanner 状态机：argv → token 流）
   │                    │
   │                    ▼
   └──►parse()──►    Compiler          （编译引擎：Lexer → 三 Parser → Expression 分类入库 Model）
                        │
                        ▼
                      Model            （ArgumentData + OptionData + EnvData；hasAction 门控）
                        │
                        ├──► Executor      （批量: hasAction 门控后置执行）
                        └──► Interpreter   （流式: 逐 Token 解析即执行）

  读取 getArgument<T> / getOption<T> / isParsed() / has_env() / env_string()
  存储/加载 schema() / snapshot() / save_config() / load_config()（YAML 配置）
  执行 once()（单次输入即退） / loop()（REPL 常驻，每行等价一次 once 输入）
```

## Expression 表达式类层次

`expression/expression.h` 定义表达式基类 `Expression` 与分类枚举 `ExpressionKind_E`：

- `ExpressionKind_E::{Argument, Option, Env}` —— 分别对应参数 / 选项 / 环境变量条目三类表达式；
- `Argument`、`Option`（`OptionValue` / `OptionAction`）与 `environment` 均继承 `Expression`，
  `kind()` 自动完成分类；`getName()` / `getHelp()` 为虚函数，可返回各自自有存储；
- `Model::expressions()` 返回全部 Expression 派生对象的**统一视图**（参数 + 选项 + 环境条目）；
- `app.model()` 返回只读 `Model const &`，配合 `Model::hasAction()` 可检查是否需执行用户 action。

## 快速开始

```shell
# 初始配置 + 构建（示例与单测目标默认不参与全量构建，需显式指定）
xmake
xmake build once         # example/once
xmake build config_demo  # example/config（YAML 配置往返演示）
xmake build unit_test    # dfx/unit_test（后续会并入 xmake test）

# 运行示例
xmake r once -- --hello hi --timeout 99 -v peter 3
xmake r config_demo -- --hello hi --timeout 99 -v peter 3   # 解析并生成 demo.yaml
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
app.addOption<std::string>('o', "output", "output file path", "out.txt");
app.addOption<int>("timeout", "timeout in seconds", 30);
app.addFlag('v', "verbose", "enable verbose output");

// 选项 action：命中即经 std::system 执行子进程命令（{value} 为本次命中的值）
app.addOption<std::string>("task", "run a system command", "");
app.option("task")->action("echo run: {value}");

// 环境变量联动：命令行显式值 > 环境变量 > 内置默认值
app.option("output")->env("MY_TOOL_OUTPUT");
app.argument("world")->env("MY_TOOL_WORLD");

// 解析（错误进 std::error_code，不抛异常）
std::error_code ec;
app.parse(ec, _Argc, _Argv, _Envp);
if (ec) { /* ec.message() 即错误说明 */ }

// 读取结果
std::string hello = app.getArgument<std::string>("hello");
int         tout  = app.getOption<int>("timeout");
bool        verb  = app.getOption<bool>("verbose");

// 配置脚本（第三类外界环境）：参数 + 环境 ↔ YAML
app.schema_string();              // 参数格式清单
app.snapshot();                   // 当前取值快照
std::string y = app.to_yaml();    // 参数 + 环境 → YAML 文本
app.save_config("my_tool.yaml");  // 存为配置文件，供下一次调用
app.load_config("my_tool.yaml");  // 下次调用：YAML → 环境变量反哺 + 参数赋值

// 必选 + 校验器 + 默认值判段（链式挂在句柄上）
app.option("host")->required().semantic("ipv4");       // 取值非法即 VALIDATION_FAILED
// 注意：argv 路径下含 '.' 的数值（如 127.0.0.1）会被 Scanner 拆分为多 token（既有词法缺陷）；
// ipv4/ipv6 语义校验仍可用（单元测试覆盖），示例中演示用 hostname/整数。
app.option("timeout")->range(1, 3600);                 // 数值区间
app.option("mode")->oneOf({"fast","slow"}).match(R"(^\w+$)");   // 枚举 + 正则
app.argument("name")->required();                      // 必选参数
app.requireEnv("DB_HOST")->semantic("hostname").defaultEnv("localhost");

// 默认值判段：来源标记
app.option("port")->defaultExpr("${CMD_PORT:-8080}");  // 表达式默认（含回退）
app.option("port")->usesDefault();   // 当前值是否来自默认值
app.option("port")->hasDefault();    // 是否声明默认值
app.option("port")->defaultValue();  // 内置默认值
// usesDefault(): 参数=值比较近似、选项=来源标记（defaultExpr 下以选项语义为准）——见 schema uses_default 字段
```

## 响应式数据：Property 属性（读写拦截）

`Argument` 与 `OptionValue` 的值都存放在 `Property<std::string>` 里（`include/cmd_args/property/property.h`），
参数/选项只是属性的持有者。属性提供三个拦截点和一个修订号：

- **写拦截** `onWrite`：赋值准入。返回空串接受，返回非空消息即拒绝——值、版本、变更回调都不动；
- **读拦截** `onRead`：取值变换（占位串、脱敏、默认值），只影响返回值，不改存储；
- **变更回调** `onChange`：值实际变化后触发 `(旧值, 新值)`；
- **修订号** `version()`：只在值真的变化时自增，可作缓存失效依据。

```cpp
// 写入即校验：空值拒绝；多余空白原地规范化
app.argument("name")->property()
   .onWrite([](std::string &v) {
       v = cmd_args::util::trim(v);
       return v.empty() ? "name 不能为空" : "";
   })
   .onRead([](std::string const &v) { return v.empty() ? "(anonymous)" : v; });

std::error_code ec;
app.parse(ec, argc, argv, envp);   // 写钩子拒绝 → ec == ValidationError_E::VALIDATION_FAILED
std::string name = app.getArgument<std::string>("name");   // 经过读钩子

app.option("timeout")->property().set("60");   // 返回 bool；false 表示被拒绝
app.option("timeout")->setValue("90");         // 拒绝时抛 ErrorValidation(VALIDATION_FAILED)
```

数据源与拦截的关系：CLI 解析与 env 兜底 seed 都走写钩子；配置（YAML / 快照 / `setExternal`）
与 `${VAR}` 展开走旁路 `setRaw`，不会被准入检查二次拦截。既有 `.check()` / `.range()` 等校验器
仍只在解析末尾统一执行；需要“写入即校验”时，在 `onWrite` 里调用 `Validator` 并返回其消息即可。

可运行示例：`xmake build reactive` 后执行 `xmake r reactive -- --name alice --port 9090`
（源码 [example/reactive/main.cpp](example/reactive/main.cpp)）。

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

### 空间键与分隔符

环境键支持空间级联：`A::B::C=V` 会拆成空间路径 `A::B` 与叶子 `C`，写入 `EnvData` 的键是 `fullPath()`（默认 `A::B::C`）。分隔符由宏 `CMD_ARGS_ENV_SCOPE_SEP` 唯一决定：默认 `"::"`，在包含任何头文件之前 `#define CMD_ARGS_ENV_SCOPE_SEP "-"`（或编译期 `/D`、`-D`）即切到 `-`，`cmd_args::ENV_SCOPE_SEP` / `ENV_SCOPE_SEP_LEN` 暴露实际值。

注意：两种写法都不能被 shell 直接导出——POSIX 变量名只允许 `[A-Za-z_][A-Za-z0-9_]*`，`export A::B=1` 与 `export A-B=1` 都失败，只能用 `env 'A::B=1' ./my_tool` 注入；Windows 用 `set "A::B=1"`（必须加引号）。默认保留 `"::"` 是为了键名不随平台漂移（YAML/CI/测试里写的是 `A::B::C`）。
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
命令行显式值  >  配置文件(YAML)  >  环境变量  >  内置默认值
```

示例：`--timeout 99` 显式给出取 99；未给出但加载的 YAML 配置里有 `timeout` 取配置值；
都没有但进程有 `MY_TOOL_TIMEOUT` 取环境值；全无则取 `addOption<int>(..., 30)` 的 30。

### 内置选项

首次 `parse()` 时懒注册：短回路选项（`-?`/`--help`、`-!`/`--env`、`--env-apply`）命中即短路
（打印后停止本次解析）；交互选项（`--env-set` 等）解析期即时生效、不短路：

- `-?` / `--help`     帮助
- `-!` / `--env`      环境变量快照
- `--env-set K=V`     设置/新建会话环境变量
- `--env-unset K`     删除会话环境变量
- `--env-apply`       把会话中的修改/新建回写到操作系统环境（Windows
                      用 `_putenv_s`，POSIX 用 `setenv`）
- `--load-config P`  按出现顺序导入 YAML 配置（后续 CLI 可覆盖）
- `--save-config P`  解析完成后把参数 + 环境状态写出为 YAML 配置

`set_builtin_options(false)` 可关闭内置选项，**必须在首次 parse 之前调用**。

## 配置脚本（YAML）：第三类外界环境

参数格式一经 `build<>` 完成，三类输入源即可互相转换：

```
        ┌───────────────────────────────────────────────┐
        │         parameter_format（build 后固定）        │
        └───────────────┬───────────────────────────────┘
                        │
 ① 环境变量  ────────────┼──► env 兜底
 ② 实时命令行 ── parse ──┼──► 参数赋值
 ③ YAML 配置 ── 新增 ────┼──► 外部赋值（bExternal__）
                        ▼
                parameter_snapshot（某次调用的结果）
                        │
        ┌───────────────┼───────────────┐
        ▼               ▼               ▼
    回写环境变量     存为 YAML       下次调用加载
   (set_env/apply)  (save_config)   (load_config 反哺)
```

### YAML 存储格式（扁平点号分区键）

```
# cmd_args config for: my_tool
# format: yaml v1
arguments.hello: "hi"
positionals.name: "peter"
positionals.count: "3"
options.timeout: "99"
flags.verbose: "1"
environment.MY_TOOL_WORLD: "hi"
```

- 分区避免命名参数与选项同名冲突；`environment` 分区只含**本程序相关**的键
  （被引用/修改/新建的 ∪ `.env()` 绑定的），不倾倒全量 OS 环境；
- 导入时 `environment` 分区反哺环境变量（`set_env`），参数分区按配置语义赋值
  （优先级低于 CLI 显式值，且不再被环境变量覆盖）；
- 未知键 / 类型不匹配记入错误栈（`dump()` 可查），**不中断**。

### YAML 子集边界

内置极简 YAML 解析器（header-only，无第三方依赖），仅支持扁平 map：
`key: value`、`#` 注释、双引号字符串（`\"` `\\` `\n` 转义）、plain 值、空值。
**不支持**嵌套 map/list、`---` 文档分隔、锚点、flow 集合、多行块（留给未来 JSON/扩展）。

### 位置参数语义

配置提供位置参数值后，CLI 可省略（数量落在 `[未赋值数, 总数]` 区间即可）；
CLI 提供的 token 按序覆盖配置值（`CLI > 配置`）。

## 选项 action（system_call 子进程执行）

部分选项可以绑定一个 **action**——它本质上就是一次 `system_call`
（`std::system` 的子进程命令）。执行时序分两路：**批量**（`parse()`）下用户 action 由
Executor 在 `Model.hasAction()` 门控下按 `actionOrder` **后置统一执行**；**流式**
（`once()`/`loop()` 逐行解释）逐 Token **即时执行**，天然满足"按先后顺序执行对应命令"。
内置交互选项（--env-set/--load-config 等）两条路径均在**解析期即时生效**。
内置交互选项在**解析期内联生效**：即使本行解析随后失败，其副作用（环境变量写入/配置加载）
也已发生；批量模式下内置选项总是先于用户 action 执行。

Built-in interactive options take effect inline during parsing: even if the rest of that
line fails afterwards, their side effects (environment writes / config loading) have
already happened; in batch mode built-ins always run before user actions.

### 绑定

```cpp
app.addOption<std::string>("task", "run a system command", "");
app.option("task")->action("echo notify: {value}");   // {value} 替换为本次命中的值
app.addFlag('r', "repeat", "repeat!");

// 值含空格 / 路径时请在模板里自带引号（shell 会按空白切分参数）：
app.option("task")->action("echo \"{value}\"");
```

### 语义约定

- `action("...")` 经 `app.option("name")` 句柄绑定（`addOption`/`addFlag` 之后均可）；
- 模板中的 `{value}` **原样**替换为该选项本次命中的值；不含 `{value}` 则是固定命令；
- 仅 CLI 显式命中触发：环境变量兜底 / YAML 配置赋值（`setExternal`）/ 内置默认值**不触发**；
- 同一选项多次命中 → 每次执行；执行顺序 = 命令行出现顺序；
- `std::system` 返回非 0 时向 stderr 打印 `(action) command exit ...` 告警，**不中断**解析；
- action 属于执行配置，不进入 `schema()` / `to_yaml()` 导出（不影响配置往返）；
- 平台行为即 `std::system` 固有语义：Windows 经 `cmd.exe`、POSIX 经 `sh`；
  命令模板中的值请自行转义/加引号（`& | > <` 等在 shell 里有特殊含义）。

## 命令行语法速查

- `--name=value` / `--name value` —— 命名参数
- 剩余非选项 token —— 方位参数，按声明顺序赋值
- `--` —— 之后的 token 一律视为方位参数（含 `-` 开头的）
- `-x value` / `-xvalue` —— 短选项；`-abc` —— 聚合短选项（bool）
- 值 token 保留原始字符：`out.txt`、`a=b`、`C:\path`、`${A}` 均原样传递
- 重复注册参数/选项名 → 构筑期抛 `ErrorArgument`（`DUPLICATE_NAME`）
- `->required()` / `->range(a,b)` / `->oneOf({..})` / `->match(re)` / `->semantic("ipv4"|"ipv6"|"port"|"file"|"path"|"date"|"duration"|"hostname"|"uuid"|"float"|"double")` —— 句柄链式约束

## 错误处理

- 解析可走异常（`parse()` 抛出 `ErrorParse` / `ErrorLexer`）或
  `std::error_code`（`parse(ec, ...)`，不抛异常）
- 三类错误码：`ParseError_E` / `LexError_E` / `InterpError_E`，均已注册进
  `std::error_code`，可用 `ec.message()` 转可读文本
- 第四类错误域 `ValidationError_E`（`MISSING_ARGUMENT` / `MISSING_OPTION` /
  `MISSING_ENV` / `VALIDATION_FAILED`）：`VALIDATION_FAILED` 既用于赋值完成后统一校验，
  也由属性写钩子拒绝即时抛出（`Argument::set` / `OptionValue::setValue`），
  同样支持异常与 `std::error_code` 双通道；批量管线中 `save-config` 随
  `finalize()` 在用户 action 执行之前写出——配置快照为 action 执行前的状态
  （validation 失败时不写出）
- `Errors` 堆栈：`dump()` / `getLast()` 保留最近一次错误详情

## 执行模式

- `once()` —— 只接收**一次**参数，处理完毕后 `std::exit(退出码)` 退出进程
- `loop()` —— REPL 常驻，标准输入**逐行**独立解析，`exit`/`quit`/EOF 退出；
  每次调用（一行）等价于一次 once 的输入——解析前自动 `reset()` 还原参数默认值，
  **绝不把多次输入合并成一条命令**；环境变量会话态跨行保留

## 已实现 / 展望

当前已实现：命名/方位参数、值选项/标志/聚合短选项、`--` 分隔、短回路选项、
环境变量展示/交互/展开/回写、**YAML 配置存储与反哺（save/load-config）**、
**选项 action（system_call 子进程按顺序执行）**、
**Expression 表达式类层次（kind() 自动分类 / Model::expressions() 统一视图 / app.model()）**、
**Property 属性读写拦截（写准入 / 读变换 / 变更回调 / 版本号）**、
参数格式 schema / 快照 snapshot、once/loop 模式、错误分类、异常捕获（airbag）。

- **值验证器**（range/oneOf/match/check + semantic 语义校验：ipv4/ipv6/port/file/path/date/duration/hostname/uuid/float/double）
- **必选约束**（`required()` / `required(false)` + `requireEnv`）
- **默认值增强**（`defaultExpr` 含 `${VAR:-fallback}`、`defaultFn` 每次 reset 重算、`hasDefault`/`usesDefault`/`defaultValue`）
- **schema 标注** `[required]` / `[default]` / `[validators]`
- **属性读写拦截** `property().onWrite/onRead/onChange` + `version()`：写准入（拒绝不改值）、读变换、变更回调；CLI 与 env seed 过钩子，配置与 `${VAR}` 展开走 `setRaw` 旁路

仍在展望（本期未做，见计划约束）：
- `counter` 计数选项（`-vvv` → 3，涉及解析层聚合改造）
- `requiredIf` / `mutuallyExclusive` 条件必选
- `removeOption` 卸载、Array Checker（`minSize`/`maxSize`，依赖容器语义）、默认值继承（子命令场景）
- argv 词法层对含 '.' 数值值的拆分缺陷（影响 127.0.0.1 等选项值；词法重构 WIP）
- `&&` / `||` 命令串联、`|` 管道、`...` 可重复项
- json 配置加载（YAML 之外的第二种脚本格式）、lua/luajit action、dll 动态补丁
- bench / meta / mock 工程

## 未来升级计划

### 1. 值验证器（Validators）

为 Option / Argument 引入 **验证器链**，在赋值后、执行前统一校验：

| 验证器 | 说明 | 示例 |
|--------|------|------|
| **Range Checker** | 数值范围检查（含开闭区间） | `option<int>("port")->range(1024, 65535)` |
| **Enum Checker** | 限定枚举值集合 | `option<string>("mode")->oneOf({"fast","slow","auto"})` |
| **Array Checker** | 限定数组/列表长度与元素约束 | `option<vector<string>>("files")->minSize(1).maxSize(10)` |
| **Regex Checker** | 正则匹配（字符串参数） | `option<string>("email")->match(R"(^[\w.]+@[\w.]+$)")`) |
| **Custom Checker** | 用户自定义 `std::function<bool(T const&)>` | `option<int>("n")->check([](int v){ return v%2==0; })` |

验证器在 `Compiler` 归约完成后、`Executor` 执行前统一触发，错误汇入 `Errors` 堆栈。

### 2. 默认值增强（Default Values）

当前 `addOption<T>(name, desc, default)` 已支持编译期默认值。未来扩展：

- **表达式默认值**：`defaultExpr("${PORT:-8080}")` —— 支持环境变量回退语法
- **惰性默认值**：`defaultFn([]{ return compute_default(); })` —— 首次访问时求值
- **默认值可见性**：`schema()` 输出中标注哪些参数有默认值，便于配置文件生成器跳过可选项
- **默认值继承**：从父 Context 或全局配置继承默认值（子命令场景）

### 3. 必选约束（Required Constraints）

| 特性 | 语法 | 说明 |
|------|------|------|
| **必选参数** | `app.build<ArgumentNamed>("name", "...")->required()` | 缺失时报 `MISSING_ARGUMENT` |
| **必选选项** | `app.addOption<int>("port", "...")->required()` | 未命中时报 `MISSING_OPTION` |
| **数值 Flag** | `app.addFlag('v', "verbose")->count()` | `-vvv` → count=3，支持 `--verbose=5` |
| **条件必选** | `requiredIf("output", "format")` | 当 format 存在时 output 也必须存在 |
| **互斥组** | `mutuallyExclusive({"json","yaml"})` | 同组内至多选一个 |

必选检查在 `Compiler` 归一化后、`Executor` 执行前统一校验。

### 4. Option 类型扩充

在现有 `bool` / `int` / `std::string` 基础上扩展专用类型：

| 类型 | 模板参数 | 语义 | 示例 |
|------|----------|------|------|
| **File** | `FilePath` | 文件路径校验（存在性/可读性） | `option<FilePath>("input")->mustExist()` |
| **Counter** | `Counter` | 计数器（`-vvv` → 3） | `option<Counter>("verbose")->count()` |
| **Flag** | `bool` (显式) | 布尔标志（`--flag` / `--no-flag`） | `option<bool>("color")->noArgDefault(true)` |
| **Float** | `float` | 单精度浮点 | `option<float>("scale", "...", 1.0f)` |
| **Double** | `double` | 双精度浮点 | `option<double>("ratio", "...", 0.5)` |
| **Date** | `Date` | 日期类型（`YYYY-MM-DD`） | `option<Date>("deadline")->format("%Y-%m-%d")` |
| **Duration** | `Duration` | 时间段（`30s` / `5m` / `1h`） | `option<Duration>("timeout", "...", "30s")` |
| **Path** | `FilePath` | 路径（不校验存在性） | `option<FilePath>("output")` |

类型注册通过 `OptionBuilder::addOption<T>()` 模板特化 + 类型 traits 自动推导。

### 5. Option 卸载与动态约束

| 特性 | API | 说明 |
|------|-----|------|
| **卸载 Option** | `app.removeOption("name")` | 从 Model 中移除，后续 parse 不再识别 |
| **动态取消必选** | `app.option("port")->required(false)` | 运行时解除必选约束 |
| **动态添加 Option** | `app.addOption<int>("new_opt", "...")` | 解析后追加（需 `reset()` 重新 parse） |
| **条件注册** | `app.build_if([]{ return need_x(); }, ...)` | 根据运行时条件决定是否注册 |
| **Option 分组** | `app.group("advanced", {...})` | 按组批量启用/禁用 |

动态约束在 `loop()` REPL 模式中尤为重要：每次 `reset()` 后可重新配置参数格式。

## 编译与测试

```shell
xmake                  # 全量配置（示例/单测为 set_default(false)，需显式构建）
xmake build once
xmake build config_demo
xmake build validation
xmake build unit_test
xmake r once -- --hello hi --timeout 99 -v peter 3   # 跑 once 示例
# config_demo 直接运行（xmake run 在 PowerShell 下传参受限时用可执行文件路径）：
./build/windows/x64/release/config_demo.exe --hello hi --timeout 99 -v peter 3
```

单元测试基于 vendored Catch2 v2，测试用例覆盖词法（token 归一化、KEY=VALUE 合并、
`-?`/`--`、未闭合字符串）、构筑期重名检查、解析错误码分类、环境变量
（展示格式与标记、三种展开、apply 回写、优先级）、**YAML 往返 / 配置反哺 /
CLI>配置>环境>默认优先级链 / loop 独立调用语义 / 选项 action 执行语义**等，
debug/release 均通过（合计 175 用例 / 913 断言，全部通过）。

> MSVC 注意：源码含 UTF-8 中文注释，xmake 已为 Windows 平台追加
> `/utf-8` 编译选项，避免 GBK 代码页下触发 C4819。

## 目录结构

```
include/cmd_args/          头文件库本体（header-only）
├── base/                  def / util（类型字符串、解析、trim/split 等）
├── error/                 builder / parser / lexer / interpreter 错误分类
├── argument/              Argument / ArgumentNamed / ArgumentPosition + 数据/解析
├── option/                OptionValue / OptionAction + 数据/构筑/解析
├── env/                   environment / EnvData / EnvBuilder / EnvParser（一等公民核心）
├── property/              Property<T>：值 + 默认值 + 写/读/变更钩子 + 修订号（读写拦截）
├── expression/            token / lexer / scanner / expression_recognizer / classified_expr /
│                          compiler / interpreter / executor（编译前端管线）
├── config/                yaml（极简 YAML 子集）/ parameter（schema/快照）/
│                          config_store（三类输入源转换桥接）
├── context/               Context / Builder / Compiler / Executor / Interpreter / Model / Expression
└── extern/                vendored：airbag（崩溃兜底）、magic_enum
example/once/              once 模式示例
example/config/            YAML 配置往返示例（config_demo）
example/validate/          必选/校验器/默认值/环境声明示例（validation）
example/reactive/          响应式拦截示例（reactive：Property 写/读拦截与变更通知）
dfx/unit_test/             Catch2 单元测试
scripts/                   构建运行脚本（build.bat / clean.bat 等）
docs/                      概念澄清：声明式构建 / 响应式数据 / 数据源绑定（含符合项盘点与候选决策）
```

## 来源与参考

[Xmake 参考手册](https://xmake.io/#/getting_started)

[argparse](https://github.com/0382/util/tree/main/cpp/argparse)

[cmdline](https://github.com/tanakh/cmdline)

[CLI11](https://github.com/CLIUtils/CLI11)

[lyra](https://github.com/bfgroup/Lyra)

[Clara](https://github.com/catchorg/Clara)

[Catch 使用教程](https://github.com/catchorg/Catch2/blob/master/docs/tutorial.md)