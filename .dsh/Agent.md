# cmd_args 工程 Agent 系统提示词

> 本文件是 `cmd_args` 工程的系统提示词（Agent 工作约定）。
> 任何 AI 助手 / 编码代理在本仓库中工作时，请把以下内容视为默认行为准则与项目事实来源，
> 并与用户**用中文**交流、用中文书写注释与说明。
>
> 事实权威：`README.md`（英文）与 `README_zh.md`（中文）为中英镜像对，
> 二者必须保持同步；行为细节以源码为准，禁止臆造 API。

---

## 1. 项目简介与定位

`cmd_args` 是一个 **C++20 头文件式（header-only）命令行解析库**（`.hpp` 即用、无需编译），Apache-2.0 许可。

- 参考借鉴了 argparse / cmdline / CLI11 / Lyra / Clara 等库的思路。
- **差异化核心**：把三类"外界环境"统一起来，参数格式一经 `build<>` 确定即可**自然转换**：
  ① **环境变量**（一等公民：可展示/交互/回写，而非"读一眼"的孤儿功能）；
  ② **实时命令行输入**（解析后可存储到环境变量或脚本）；
  ③ **脚本文件**（YAML 配置，JSON 未来：`save_config`/`load_config` 往返 + 反哺环境变量）。
- 默认命名空间为 `cmd_args`（经 `CMD_ARGS_NAMESPACE_BEGIN/END` 宏展开，见 `include/cmd_args/base/def.h`）。

### 解析管线（架构必须保持）

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
  存储/加载 schema() / snapshot() / save_config() / load_config()（YAML 配置）
  执行 once()（单次输入即退） / loop()（REPL 常驻，每行等价一次 once 输入）
```

---

## 2. 不可违背的硬约束

| # | 约束 | 说明 |
|---|------|------|
| 1 | 保持 header-only | 所有实现必须留在 `include/cmd_args/**` 的头文件中；禁止引入 `.cpp` 编译单元、静态库链接或运行时依赖 |
| 2 | C++20，双平台可编译 | MSVC（Windows）+ Linux（Clang/GCC）都必须编译通过；任何头文件改动都要双平台验证 |
| 3 | 重名必须报错 | 重复的命名参数/选项名 → 抛 `detail::ErrorArgument(BuildError_E::DUPLICATE_NAME)`（"name already exists"） |
| 4 | 范围外功能不得擅自实现 | `&&` / `\|` 组合、`...` 可变参数、json / lua / dll / bench / meta / mock 等只可出现在"未来路线图"（见 §10），不得实现或伪实现 |
| 5 | `--` 语义 | `--` 之后的所有 token 一律作为方位参数；`--` 之前的命名参数先行消化 |
| 6 | argv[0] 原样保留 | argv[0] 是程序名（可能含盘符/路径），不是命令行语法，原样保留；只对 `argv[1..]` 做词法扫描 |
| 7 | 测试只用公共 API | 禁止 `#define private public` 等访问黑魔法；测试必须经公开接口驱动 |
| 8 | 环境变量兜底语义 | `.env("VAR")` 显式绑定后才参与兜底；优先级 **CLI 显式值 > 配置文件(YAML) > 环境变量 > 内置默认值**；配置导入走 `set_external`（记 `bExternal__`），env 兜底跳过，CLI 后续仍可覆盖 |
| 9 | 兼容别名勿删 | `envirionment_parser` 是 `environment_store` 的历史拼写兼容别名（保留原拼写错误），不得删除 |
| 10 | builtins 懒注册 | 内置选项在每个 Model 首次解析时注册一次；`set_builtin_options(false)` 只在**同一 Context 首次 parse 之前**调用才生效 |

---

## 3. 源码地图

```
include/cmd_args/
├── cmd_args.hpp                    # 唯一入口：只 include context/context.h
├── base/def.h                      # 命名空间宏 CMD_ARGS_NAMESPACE_BEGIN/END、ARRAY_SIZE、ENUM_NAME
├── base/util.h                     # parse_value<T> / parse_bool / type_string / starts_with / trim 等
├── base/defer.hpp                  # RAII defer
├── context/
│   ├── context.h                   # Context：build/add_option/add_flag/parse/once/loop/env 交互门面
│   ├── builder.h                   # Builder：构筑 Argument/Option/Flag 并写入 Model
│   ├── lexer.h                     # Lexer/Scanner 状态机：argv/行文本 → token 流（含归一化）
│   ├── parser.h                    # Parser：语法分配、内置选项注册、env 兜底默认、值展开
│   ├── interpreter.h               # Interpreter 基类：once/loop 解释执行引擎（钩子由 Context 实现）
│   ├── model.h                     # Model：ArgumentData + OptionData + environment_store
│   ├── loader.h / parse_fwd.h      # 类型前置声明（Tokens_T / Envs_T / envs_t 等）
├── argument/
│   ├── argument.h / argument_builder.h / argument_data.h
│   └── argument_parser.h           # parseNamedOnly / parsePositionalOnly；数量不匹配报 "give X, but need Y"
├── option/
│   ├── option.h / option_builder.h / option_data.h / option_parser.h
│   └──            # 长/短选项、短回路选项、--long value 空格吃值、
│                  # 选项 action（system_call 子进程，解析期命中即按顺序执行）
├── env/
│   ├── environment.h               # environment：key/value + bReferenced__/bModified__/bNew__ 标记
│   └── environment_parser.h        # environment_store：解析/展示/交互/展开/回写（差异模块）
├── config/
│   ├── yaml.h                      # 极简 YAML 子集（yaml_doc：解析/发射，扁平 map）
│   ├── parameter.h                 # parameter_entry / parameter_schema / parameter_snapshot
│   └── config_store.h              # 三类输入源转换桥接：schema/snapshot、to_yaml/from_yaml、env↔yaml
├── error/
│   ├── errors.h                    # Context 错误堆栈（dump/getLast/getLastMsg/isOk）
│   ├── builder_error.h             # detail::BuildError_E / ErrorArgument（DUPLICATE_NAME 等）
│   ├── lexer_error.h               # LexError_E / ErrorLexer（UNTERMINATED_STRING 等）
│   ├── parser_error.h              # ParseError_E / ErrorParse（UNKNOWN_OPTION 等）
│   ├── interpreter_error.h         # InterpError_E / ErrorInterp（STOPPED 等）
│   ├── exception_handler.h         # 可选：纯虚调用/崩溃处理
│   └── functional_util.h
└── extern/
    ├── airbag/                     # 内置异常/崩溃辅助（Windows 示例 target 需 /EHa）
    └── magic_enum/                 # 内置枚举反射（vendored）
example/once/main.cpp               # 演示程序：env 一等公民完整故事
example/config/main.cpp             # config_demo：三类输入源转换（YAML 往返 + 反哺）
dfx/unit_test/head.cpp              # 单元测试（Catch2，仅公共 API）
dfx/internal/catch2/ catch.hpp      # vendored 单测框架
scripts/build_*.bat                 # 预留的构建脚本（bench/meta/mock 为路线图占位）
```

---

## 4. 公共 API 事实（以头文件为准，勿臆造）

```cpp
Context app("my_tool");

// ── 构筑 ─────────────────────────────────────────────
app.build<ArgumentNamed>("hello", "Say hello");          // 命名参数 --hello=world
app.build<ArgumentPosition>("name", "your name");        // 方位参数按序赋值
app.add_option<std::string>('o', "output", "help", "out.txt"); // 短名+长名+默认值
app.add_option<int>("timeout", "help", 30);              // 仅长名
app.add_flag('v', "verbose", "help");                    // 布尔标志

// 句柄继续定制（在 parse 之前）
app.argument("world")->env("CMD_ARGS_DEMO_WORLD");       // 绑定环境变量兜底
app.option("timeout")->env("CMD_ARGS_DEMO_TIMEOUT");     // .on_set(cb) 可挂钩回调
app.option("task")->action("make {value}");              // 绑定 system_call 子进程命令：
                                                         // 解析期命中即执行；{value} 替换为本次值

// ── 解析（三形态）────────────────────────────────────
app.parse(argc, argv, envp);                            // 抛 ErrorParse/ErrorLexer
std::error_code ec; app.parse(ec, argc, argv, envp);     // noexcept，错误入 ec
app.parse(ec, tokens, envs);                            // token 流形态（loop 复用）

// ── 数据读取 ─────────────────────────────────────────
std::string s = app.get_argument<std::string>("hello");
int t = app.get_option<int>("timeout");
app.has_option("--timeout");  app.parsed("verbose");

// ── 环境变量交互 ─────────────────────────────────────
app.has_env("KEY"); app.get_env("KEY"); app.get_env_or("KEY", "<missing>");
app.set_env("KEY", "VALUE"); app.unset_env("KEY");
size_t n = app.apply_env();             // 返回实际写回条目数（不是 vector！）
std::string e = app.expand_env("${VAR}!");
std::cout << app.env_string();          // 或 app.print_env();

// ── 参数格式 / 快照 / YAML 配置（第三类外界环境）────────
app.schema_string();                    // 参数格式清单（schema() 结构化版本）
app.snapshot();                         // 当前取值快照（parameter_snapshot）
std::string y = app.to_yaml();          // 参数 + 环境 → YAML 文本
app.save_config("cfg.yaml");            // 存配置，供下一次调用
app.load_config("cfg.yaml");            // 加载：YAML → 环境反哺 + 参数外部赋值
app.env_to_yaml();  app.yaml_to_env(text);
app.reset();                            // 还原默认值/清标记/清错误（loop 每次调用前自动做）

// ── 执行模式 ─────────────────────────────────────────
app.once();                             // 解析后以退出码退出进程
app.loop();                             // REPL：exit/quit/EOF 退出；每行独立解析
```

要点：

- 选项长名可带 `--` 前缀查询，内部会剥离；短名 `char` 也可查询。
- `get_argument<T>` / `get_option<T>` 有严格类型匹配检查（`type_string<T>()`），不匹配即抛错。
- `once()` 未解析时抛 `ErrorInterp(InterpError_E::STOPPED)`。
- `loop()` 每次调用（一行）前自动 `reset()`：还原参数/选项默认值、清显式/外部标记、清错误栈；
  逐行 `Lexer::tokensFromLine` 独立解析，**绝不把多行合并成一条命令**；环境变量会话态跨行保留。
- `reset()` 依赖 `Argument`/`option_value` 的默认值缓存（`m_strDefault__`/`strDefault__`）。
- 错误堆栈：`dump() / getLast() / getLastMsg() / isOk()`（由 `Context` 公开继承自 `Errors`）。
- YAML 配置导入的未知键/类型不匹配记入错误栈（warning），不中断；`from_yaml` 返回 false 仅当解析失败。

---

## 5. 环境变量一等公民（差异化核心，`environment_store`）

### 5.1 状态标记

| 标记 | 含义 | 触发 |
|------|------|------|
| `*` | 被修改（modified） | `set()` 命中已有键、`--env-set` 改已有值 |
| `>` | 被引用（referenced） | `get()/get_or()/touch()`、`.env()` 兜底命中、`expand()` 命中 |
| `+` | 本次新建（new） | `set()` 新建键、`--env-set` 新建（新键只显 `+`，不显 `*`） |

### 5.2 展示格式（`env_string(program)` / `print_env()` / 内置 `-!` `--env`）

```
environments: <program>
No. : "key" = "value"
0   : "A" = "1"
1*  : "B" = "2"     (* 被修改)
2>  : "C" = "3"     (> 被引用)
3*+ : "D" = "4"     (+ 本次新建)
```

### 5.3 展开（`expand(text)`，命中即标记引用，未命中保持原样）

支持三种方言：`${NAME}` / `%NAME%` / `$NAME`。
Parser 在 `__finalize` 阶段会对所有已赋值参数/选项值做展开（`__expandEnvValues`）。

### 5.4 回写（`apply()`）

- 只回写 `bModified__ || bNew__` 的条目；成功回写后清除标记。
- 平台差异：Windows `_putenv_s`；POSIX `setenv(key, value, 1)`。
- 返回实际应用的 `"KEY=VALUE"` 列表；`Context::apply_env()` 返回其条数。

### 5.5 会话内内置 CLI（Parser 懒注册，`-?`/`-!` 属短路选项）

| 内置项 | 行为 |
|--------|------|
| `-?` / `--help` | 打印帮助并短路（不继续解析） |
| `-!` / `--env` | 打印环境展示并短路 |
| `--env-set KEY=VALUE` | 设置环境变量（无 `=` 时值为空串），可新建 |
| `--env-unset KEY` | 删除环境变量 |
| `--env-apply` | 回写操作系统，逐行打印 `applied KEY=VALUE` 后再展示环境 |
| `--load-config PATH` | 按出现顺序导入 YAML 配置（后续 CLI 可覆盖；值选项，非短路） |
| `--save-config PATH` | 解析完成后把参数 + 环境状态写出为 YAML（`__finalize` 末尾执行） |

短路选项命中即回调并停止（`fired()` 为 true），其余 token 不再分配。
内置值选项带 `bBuiltin__` 标记，**不参与** `schema()`/`to_yaml()` 的格式导出。

---

## 6. Lexer / Parser 行为约定（重要规则细节）

- token 归一化：`--foo=bar`（长名+值）、`-x=value`（短名+值）、`-abc`（聚合短标志）、
  `KEY=VALUE` / `a=b`（Continual/Variable + `=` 分隔合并）、`x.y`（piece）、`-5`（数字短名）均被 Scanner 正确切分。
- 变量值吸收**全部非空白字符**（值内可含 `. - = \ / : @`）。
- 未终止字符串 → `ErrorLexer(LexError_E::UNTERMINATED_STRING)`；裸 `-` → `BARE_DASH`。
- 长选项支持 `--long value` 空格形式：下一个 token 非 `-` 开头则作为值，否则 `NEED_VALUE`；
  布尔长选项走 bool 赋值路径（`option_parser`）。
- `--` 之后全部方位参数；此前命名参数先经 `parseNamedOnly` 消化，再 `parsePositionalOnly`。
- 解析收尾（`__finalize`）：先 `__seedEnvDefaults`（CLI > 配置 > 环境 > 内置默认，跳过 `bExplicit__ || bExternal__`），
  再 `__expandEnvValues`，最后若有 `--save-config` 则写出 YAML。
- 选项 action（system_call）：`.action("cmd {value}")` 绑定后，仅 CLI 显式命中触发
  （`__assign` 路径，按 token 出现顺序执行）；环境变量兜底 / `set_external`（YAML）/ 内置默认均不触发；
  `{value}` 原样替换为本次命中值；`std::system` 返回非 0 → stderr 告警不中断；不参与 schema/YAML 导出。
- 剩余 `-` 开头 token → `UNKNOWN_OPTION`（"unrecognized option or argument: ..."）。
- 方位参数数量：剩余 token 数须落在 `[未外部赋值数, 总数]` 区间；
  恰好等于未赋值数时跳过配置已填位，多余 token 覆盖配置值（CLI > 配置）。

---

## 7. 错误体系（三域枚举 + 双通道）

| 域 | 枚举 | 抛出的异常 | 典型码 |
|----|------|-----------|--------|
| 构筑 | `detail::BuildError_E` | `detail::ErrorArgument` | `DUPLICATE_NAME` / `NEED_NAME` / `NEED_HELP` / `UNSUPPORT_TYPE` / `BAD_PREFIX` |
| 词法 | `LexError_E` | `ErrorLexer` | `UNTERMINATED_STRING` / `BARE_DASH` / `EMPTY_SOURCE` / `INVALID_TOKEN` |
| 语法 | `ParseError_E` | `ErrorParse` | `UNKNOWN_OPTION` / `UNKNWON_ARGS`（历史拼写勿改）/ `NEED_VALUE` / `MUST_BE_BOOL` / `UNEXCEPTED_BARE` / `WRONG_PREFIX` / `NOT_ENOUGH_ARGS` |
| 执行 | `InterpError_E` | `ErrorInterp` | `STOPPED` |

每个域都提供 `make_error_code`/`make_error_condition` 与 `std::is_error_code_enum` 特化；
`Context::parse` 同时提供抛异常与 `std::error_code&` 两种形式（noexcept 版捕获后写入 ec 并记入错误堆栈）。

---

## 8. 构建与测试（xmake）

### 8.1 本机沙箱环境前提（Windows，必须）

AppData 缓存目录在沙箱下不可创建，构建前必须先设：

```powershell
$env:XMAKE_GLOBALDIR='H:\dsh_space\cmd_args\build\.xmake-global'
$env:XMAKE_PROFILE_DIR='H:\dsh_space\cmd_args\build\.xmake-profile'
```

### 8.2 命令表

| 意图 | 命令 |
|------|------|
| 配置 | `xmake f`（或直接 `xmake`） |
| 全量默认目标 | `xmake` — 注意 `once` / `config_demo` / `unit_test` 均为 `set_default(false)`，**不会**被全量构建，必须显式指定 |
| 构建示例 | `xmake build once` / `xmake build config_demo` |
| 运行示例 | `xmake run once -- --hello hi --timeout 99 -v peter 3`；PowerShell 传参受限时直接用 `build\windows\x64\release\*.exe` |
| 构建单测 | `xmake build unit_test` |
| 运行单测 | `xmake run unit_test` |
| 清理 | `xmake clean` / `xmake f -c` |

### 8.3 编译设置事实

- 根 `xmake.lua`：`set_languages("c++20")`、`set_warnings("all", "error")`、`mode.debug` / `mode.release`。
- Windows 强制 `/utf-8`（源文件含 UTF-8 中文注释，避免 C4819）。
- `example/once` 额外 `/EHa`（airbag 的 `_set_se_translator` 需要）。debug 用 `/ZI` + `MDd`，release 用 `/MP` + `MT`。
- 产物在 `build\windows\x64\{debug,release}\`；`/build`、`/xmake`、`/vscode` 均在 `.gitignore` 中。

---

## 9. 编码与提交约定

1. **源码风格**：每个头文件保留 Apache-2.0 许可头；成员命名 `m_xxx__`（双下划线后缀）；
   命名空间一律经 `CMD_ARGS_NAMESPACE_BEGIN/END` 宏；注释用中文。
2. **提交信息**：gitmoji 风格 `type(scope): emoji message`，例如
   `feat(Lexer): ✨ intput to tokens`、`fix((builder)): 🏁 fix mingw error`、
   `feat(core): ✨ complete CLI pipeline, env vars as first-class citizens`。
   scope 取子系统（core / Lexer / Parser / builder / env / example / test / docs）。
3. **仓库身份**：repo-local 配置为 `ValenciaFly <pengwanring@live.com>`（不要改成全局身份）。
4. **README 镜像**：改公共 API 或行为时必须同步更新 `README.md` 与 `README_zh.md`，二者保持一致。
5. **测试门槛**：`dfx/unit_test/head.cpp` 目前 34 个 TEST_CASE / 187 条断言；
   任何行为改动应保持或增加覆盖，禁止回退。
6. **已知注意点**：`push` 到 GitHub 受当前网络限制（无代理、无 SSH key，连接被重置）——
   本地提交即可，推送受阻不是提交失败；不要反复重试 push。
7. 根目录遗留 `vc140.idb` / `vc140.pdb` 已被 `.gitignore` 覆盖，可忽略或清理。
8. 不得重建已删除的 `include/cmd_args/error/exception.h`；错误类型统一走 `error/` 下的现成头。
9. 示例产物 `demo.yaml`（config_demo 生成）已在 `.gitignore`（`/demo.yaml`），勿入库。

---

## 10. 未来路线图（范围外，仅文档化，不得实现）

- 组合/管道语法：`&&`、`|`
- 可变参数：`...`
- 扩展模块：**json 配置（YAML 之外的第二种脚本格式）**、lua、dll、bench、meta、mock
  （`scripts/` 下已有占位 `build_*.bat`）；`config/` 目录为新增格式预留位置

任何此类需求先记录到本节/README 的计划段落，再与用户确认是否立项。

---

## 11. Agent 工作守则

1. 修改前先读目标文件与相关头文件（本仓库头文件即文档，行为以源码为准）。
2. 优先最小改动；改公共 API 必须先更新 §4 对应签名与双语 README。
3. 每次改动后至少运行 `xmake build unit_test` + `xmake run unit_test` 验证；
   涉及示例的改动再跑 `xmake build once`。
4. 新功能默认落在"环境变量一等公民"这条差异主线上，与既有管线（Builder→Lexer→Parser→Model）无缝衔接。
5. 与用户用中文沟通；解释性内容（注释、README 中文镜像）也用中文。