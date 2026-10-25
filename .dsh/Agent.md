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
   └──►parse()──►    Compiler          （编译引擎：Lexer → 三 Parser → Expression 分类入库 Model）
                         │
                         ▼
                       Model            （ArgumentData + OptionData + EnvData；hasAction 门控）
                         │
                    ┌────┴────┐
                    ▼         ▼
              Executor    Interpreter  （批量: hasAction 门控后置执行 / 流式: 字符泵逐 Token 解析即执行）
```

---

## 2. 不可违背的硬约束

| # | 约束 | 说明 |
|---|------|------|
| 1 | 保持 header-only | 所有实现必须留在 `include/cmd_args/**` 的头文件中；禁止引入 `.cpp` 编译单元、静态库链接或运行时依赖 |
| 2 | C++20，双平台可编译 | MSVC（Windows）+ Linux（Clang/GCC）都必须编译通过；任何头文件改动都要双平台验证 |
| 3 | 重名必须报错 | 重复的命名参数/选项名 → 抛 `ErrorArgument(BuildError_E::DUPLICATE_NAME)` |
| 4 | 范围外功能不得擅自实现 | `&&` / `\|` 组合、`...` 可变参数、json / lua / dll / bench / meta / mock 等只可出现在"未来路线图"（见 §10），不得实现或伪实现 |
| 5 | `--` 语义 | `--` 之后的所有 token 一律作为方位参数；`--` 之前的命名参数先行消化 |
| 6 | argv[0] 原样保留 | argv[0] 是程序名（可能含盘符/路径），不是命令行语法，原样保留；只对 `argv[1..]` 做词法扫描 |
| 7 | 测试只用公共 API | 禁止 `#define private public` 等访问黑魔法；测试必须经公开接口驱动 |
| 8 | 环境变量兜底语义 | `.env("VAR")` 显式绑定后才参与兜底；优先级 **CLI 显式值 > 配置文件(YAML) > 环境变量 > 内置默认值**；配置导入走 `setExternal`（记 `bExternal__`），env 兜底跳过，CLI 后续仍可覆盖 |
| 9 | 环境生态以 Env 三件套为准 | env 采用 `EnvData` / `EnvBuilder` / `EnvParser`；历史文件 `environment_parser.h`（含拼写 `envirionment_parser` / `environment_store`）已按用户要求删除，不得重建 |
| 10 | builtins 懒注册 | 内置选项在每个 Model 首次解析时注册一次；`set_builtin_options(false)` 只在**同一 Context 首次 parse 之前**调用才生效 |
| 11 | 参数值只经 `Property` 读写 | `Property<T>` 是值的唯一存储与拦截点；业务写入走 `Argument::set` / `OptionValue::setValue`（可被写钩子拒绝并抛 `ErrorValidation(VALIDATION_FAILED)`），只有配置/快照/`${VAR}` 展开等派生来源才允许 `setExternal` / `property().setRaw`；禁止恢复 `OptionValue::strValue` 公有字段 |
| 12 | env 空间分隔符只能是宏 | `CMD_ARGS_ENV_SCOPE_SEP`（`base/def.h`，默认 `"::"`）唯一决定级联分隔符；`ENV_SCOPE_SEP`/`ENV_SCOPE_SEP_LEN` 为派生常量；`token.h` 的 `toText()`、`classified_expr.h` 的 `fullPath()`、`scanner.h` 的 `IN_SCOPE_SEP` 禁止硬编码字面量。`'-'` 需在 include 前 `#define` 覆盖，且 POSIX shell 无法直接 `export` 任一写法 |

---

## 3. 源码地图

> 各子目录的详细模块说明见与该目录同名的 Markdown 文档：`argument/argument.md`、`option/option.md`、`env/env.md`、`property/property.md`、`context/context.md`、`expression/expression.md` 等（命名规则与覆盖清单见 §12）。

```
include/cmd_args/
├── cmd_args.hpp                    # 唯一入口：include context/{context,context_step,context_runtime,factory}.h + property/property.h
├── base/                           # 基础工具（详见 base/base.md）
│   ├── def.h                       # 命名空间宏 CMD_ARGS_NAMESPACE_BEGIN/END、ARRAY_SIZE、ENUM_NAME、CMD_ARGS_ENV_SCOPE_SEP（env 空间分隔符，默认 "::"）
│   ├── util.h                      # type_string<T> / parse_value<T> / parse_bool / trim / split / unwrap 等
│   ├── defer.hpp                   # RAII defer / UniqueDefer
│   └── validator.h                 # Validator / ValidatorMixin / SemanticTable_T（is_port/is_ipv4/…）
├── context/                        # 上下文门面与数据仓库（详见 context/context.md）
│   ├── context_base.h              # ContextBase：声明/构筑/解析/校验的公共基座
│   ├── context.h                   # Context 门面：子命令 + 批量管线
│   ├── context_step.h              # ContextStep：批量 parse 入口 + loop() 批量 REPL
│   ├── context_runtime.h           # ContextRuntime：流式 parse 入口 + loop() 流式 REPL
│   ├── factory.h                   # make_context(EngineKind_E)：按引擎种类造上下文
│   ├── context_fwd.h               # Name_T/Desc_T 等别名 + EngineKind_E + 前置声明
│   ├── builder.h                   # Builder 构筑桥接（ArgumentBuilder + OptionBuilder）
│   ├── builtin_options.h           # 内置选项懒注册（registerBuiltinOptions / resetModelData）
│   ├── model.h                     # Model 数据仓库（hasAction() / expressions() 统一视图）
│   ├── declarations.h              # OptionDef<T> / FlagDef 声明包装
│   ├── describe.h                  # usageOf / helpOf / envOf 描述生成
│   ├── loader.h                    # 加载器占位（扩展加载预留）
│   └── subcommand.h                # SubcommandRegistry / SubcommandFactory
├── expression/                     # 表达式/编译引擎层（详见 expression/expression.md）
│   ├── expression.h                # Expression 基类 + ExpressionKind_E（统一视图）
│   ├── token.h                     # Token：Scanner 产出的最小语义原子（KeyWord_E / Type_E）
│   ├── scanner.h                   # Scanner 字符泵状态机（scan / step；LexMode_E）
│   ├── lexer.h                     # Lexer 门面：argv/整行/串列表 → Token 流
│   ├── classified_expr.h           # ClassifiedExpr 解析管线瞬态中间表示
│   ├── expression_recognizer.h     # ExpressionRecognizer 语法前端（Token → Expression）
│   ├── compiler.h                  # Compiler 编译引擎：三 Parser → 入库 Model
│   ├── interpreter.h               # Interpreter 流式引擎（字符泵逐 Token 解析即执行）
│   ├── validation.h                # 统一校验入口（env spec → 必选 → 值校验）
│   └── executor.h                  # Executor 批量执行（hasAction 门控按 actionOrder 后置）
├── argument/                       # 参数子系统（详见 argument/argument.md）
│   ├── argument.h                  # Argument / ArgumentNamed / ArgumentPosition
│   ├── argument_builder.h          # ArgumentBuilder 构筑层
│   ├── argument_data.h             # ArgumentData + NamedArgumentMgr / PositionArgumentMgr
│   └── argument_parser.h           # ArgumentParser 解析层
├── option/                         # 选项子系统（详见 option/option.md）
│   ├── option.h                    # Option / OptionValue / OptionAction
│   ├── option_builder.h            # OptionBuilder 构筑层
│   ├── option_data.h               # OptionData + ValueOptMgr / ActionOptMgr + actionOrder
│   ├── option_graph.h              # OptionGraph：conditional(-a?b:c)/dependency(-a->b) 有向图
│   └── option_parser.h             # OptionParser 纯解析层（内置选项解析期即时触发回调）
├── env/                            # 环境变量子系统（详见 env/env.md）
├── property/                       # 参数值的属性载体（详见 property/property.md）
│   └── property.h                  # Property<T>：值 + 默认值 + 写/读/变更钩子 + 修订号
│   ├── env.h                       # environment 条目结构
│   ├── env_data.h                  # EnvData 数据仓库（std::map）
│   ├── env_builder.h               # EnvBuilder 写操作
│   └── env_parser.h                # EnvParser 解析/服务层
├── config/                         # 配置桥接（详见 config/config.md）
│   ├── yaml.h                      # 极简 YAML 子集（yaml_doc）
│   ├── parameter.h                 # parameter_entry / parameter_schema / parameter_snapshot
│   └── config_store.h              # 三类输入源转换桥接
├── error/                          # 错误体系（详见 error/error.md）
│   ├── errors.h                    # 错误堆栈（dump/getLast/isOk）
│   ├── builder_error.h             # BuildError_E / ErrorArgument
│   ├── lexer_error.h               # LexError_E / ErrorLexer
│   ├── parser_error.h              # ParseError_E / ErrorParse
│   ├── interpreter_error.h         # InterpError_E / ErrorInterp
│   ├── validation_error.h          # ValidationError_E / ValidationErrorCategory
│   ├── exception_handler.h         # 可选：airbag 纯虚调用/崩溃兜底（Windows /EHa）
│   └── functional_util.h           # Optional / Lazy（遗留实用集，当前无引用）
└── extern/                         # 第三方 vendored（详见 extern/extern.md）
    ├── airbag/                     # 内置异常/崩溃辅助（详见 extern/airbag/airbag.md）
    └── magic_enum/                 # 内置枚举反射（详见 extern/magic_enum/magic_enum.md）

example/chain/main.cpp              # 链式 SPI 示例（target: chain）
example/config/main.cpp             # config_demo：三类输入源转换（YAML 往返 + 反哺）
example/declare/main.cpp            # declare() 折叠声明（target: declare）
example/deduce/main.cpp             # addOption 类型推导（target: deduce）
example/group/main.cpp              # group() 帮助分组（target: group）
example/once/main.cpp               # env 一等公民完整故事（target: once）
example/validate/main.cpp           # required/range/semantic/requireEnv 校验（target: validation）
example/reactive/main.cpp           # Property 写/读拦截与变更通知（target: reactive）
                                    # 示例总览见 example/example.md（各子目录另有同名文档）
dfx/unit_test/                      # 单元测试（Catch2，仅公共 API）：main.cpp 仅 CATCH_CONFIG_MAIN，
                                    # 19 个 test_*.cpp 按子模块拆分（详见 dfx/unit_test/unit_test.md）
scripts/build_*.bat                 # 预留构建脚本
```

---

## 4. 公共 API 事实（以头文件为准，勿臆造）

```cpp
Context app("my_tool");

// ── 构筑 ─────────────────────────────────────────────
app.build<ArgumentNamed>("hello", "Say hello");
app.build<ArgumentPosition>("name", "your name");
app.addOption<std::string>('o', "output", "help", "out.txt");
app.addOption<int>("timeout", "help", 30);
app.addFlag('v', "verbose", "help");

// 句柄继续定制（在 parse 之前）
app.argument("world")->env("CMD_ARGS_DEMO_WORLD");
app.option("timeout")->env("CMD_ARGS_DEMO_TIMEOUT");
app.option("task")->action("make {value}");

// ── 属性读写拦截（property/property.h；参数与选项共用）──────────
app.argument("world")->property()
   .onWrite([](std::string &v) { return v.empty() ? "不能为空" : ""; })  // 返回非空消息即拒绝
   .onRead([](std::string const &v) { return v.empty() ? "(anonymous)" : v; });
app.option("timeout")->setValue("60");              // 经写钩子；拒绝抛 ErrorValidation(VALIDATION_FAILED)
std::string tv = app.option("timeout")->getValue();  // 经读钩子（按值返回）
bool okSet = app.option("timeout")->property().set("90");  // Property 自身返回 bool，不抛
// 数据源：CLI 与 env seed 走 set（过写钩子）；YAML/快照与 ${VAR} 展开走 setRaw 旁路

// ── 解析（三形态）────────────────────────────────────
app.parse(argc, argv, envp);                            // 抛 ErrorParse/ErrorLexer
std::error_code ec; app.parse(ec, argc, argv, envp);     // noexcept
app.parse(ec, tokens, envs);                            // token 流形态（loop 复用）

// ── 数据读取 ─────────────────────────────────────────
std::string s = app.getArgument<std::string>("hello");
int t = app.getOption<int>("timeout");
app.hasOption("--timeout");  app.isParsed("verbose");

// ── Expression 统一视图（expression/expression.h）──────────────
// ExpressionKind_E::{Argument, Option, Env}：Argument / Option / environment
// 均继承 Expression 基类；kind() 自动分类，getName()/getHelp() 虚函数可返回自有存储
app.model();                                // 只读 Model 引用（Context::model()）
auto const &m = app.model();
bool hasAct = m.hasAction();                // 是否存在待执行用户 action（Executor 门控）
auto exprs  = m.expressions();              // 全部 Expression 派生对象指针的统一视图

// ── 环境变量交互 ─────────────────────────────────────
app.has_env("KEY"); app.get_env("KEY"); app.get_env_or("KEY", "<missing>");
app.set_env("KEY", "VALUE"); app.unset_env("KEY");
size_t n = app.apply_env();
std::string e = app.expand_env("${VAR}!");
std::cout << app.env_string();

// ── env 空间键（分隔符宏，默认 '::'）───────────────────
// A::B::C=V → 键 A::B::C；include 前 #define CMD_ARGS_ENV_SCOPE_SEP "-" 改为 A-B-C
cmd_args::ENV_SCOPE_SEP;       // 实际分隔符（const char*）
cmd_args::ENV_SCOPE_SEP_LEN;   // 字符数
// 注意：POSIX shell 不能 export A-B=1 / A::B=1，需 env 'A::B=1' ./app

// ── 配置脚本 ────────────────────────────────────────
app.schema_string();  app.snapshot();  std::string y = app.to_yaml();
app.save_config("cfg.yaml");  app.load_config("cfg.yaml");
app.env_to_yaml();  app.yaml_to_env(text);
app.reset();

// ── 执行模式 ─────────────────────────────────────────
app.once();   // 解析后以退出码退出进程
app.loop();   // REPL：exit/quit/EOF 退出；每行独立解析
```

---

## 5. 环境变量一等公民（核心差异化功能）

> 详细说明见 `include/cmd_args/env/env.md`。以下为摘要。

- **优先级链**：CLI 显式值 > 配置文件(YAML) > 环境变量 > 内置默认值
- **展示标记**：`*`（被修改）/ `>`（被引用）/ `+`（本次新建）
- **展开**：`${NAME}` / `%NAME%` / `$NAME`（未命中保持原样）
- **回写**：`apply()` 只回写 `bModified__ || bNew__` 条目
- **会话态**：env 具会话跨行保留（loop 逐行 reset 不清 env）

---

## 6. Lexer / Compiler 行为约定

> 详细说明见 `include/cmd_args/context/context.md` 与 `include/cmd_args/expression/expression.md`。以下为关键规则摘要。

- token 归一化：`--foo=bar`（长+值）、`-x=value`（短+值）、`-abc`（聚合短标志）、
  `KEY=VALUE`（Continual+`=`）、`x.y`（piece）、`-5`（数字短名）均被 Scanner 正确切分。
- 变量值吸收**全部非空白字符**（值内可含 `. - = \ / : @`）。
- `--` 之后所有 token 一律作为方位参数（流式路径严格遵循；批量路径历史行为含
  「选项结束后 parseNamedOnly 先行消化命名参数」，以流式为准绳的统一留待后续）。
- 选项 action（system_call）：内置交互选项（--env-set/--load-config 等）**解析期内联生效**：
  即使本行解析随后失败，其副作用（环境变量写入/配置加载）也已发生；批量模式下内置选项
  总是先于用户 action 执行；用户 action 由 Executor 在 `Model.hasAction()` 门控下按
  actionOrder **后置统一执行**；流式路径（REPL）逐 Token **即时执行**（不经 Executor / actionOrder）。
- 方位参数数量：剩余 token 数须落在 `[未外部赋值数, 总数]` 区间。

---

## 7. 错误体系（三域枚举 + 双通道）

| 域 | 枚举 | 异常 | 典型码 |
|----|------|------|--------|
| 构筑 | `BuildError_E` | `ErrorArgument` | `DUPLICATE_NAME` / `NEED_NAME` / `NEED_HELP` / `UNSUPPORT_TYPE` / `BAD_PREFIX` |
| 词法 | `LexError_E` | `ErrorLexer` | `UNTERMINATED_STRING` / `BARE_DASH` |
| 语法 | `ParseError_E` | `ErrorParse` | `UNKNOWN_OPTION` / `UNKNWON_ARGS`（历史拼写）/ `NEED_VALUE` |
| 执行 | `InterpError_E` | `ErrorInterp` | `STOPPED` |

---

## 8. 构建与测试（xmake）

### 8.1 本机沙箱环境前提（Windows，必须）

```powershell
$env:XMAKE_GLOBALDIR='H:\dsh_space\cmd_args\build\.xmake-global'
$env:XMAKE_PROFILE_DIR='H:\dsh_space\cmd_args\build\.xmake-profile'
```

### 8.2 命令表

| 意图 | 命令 |
|------|------|
| 全量配置 | `xmake f` |
| 构建单测 | `xmake build unit_test` |
| 运行单测 | `xmake run unit_test` |
| 构建示例 | `xmake build once` / `xmake build config_demo` |
| 清理 | `xmake clean` / `xmake f -c` |

### 8.3 编译设置
- `set_languages("c++20")`、`set_warnings("all", "error")`。
- Windows `/utf-8`；debug `/ZI`+`MDd`；release `/MP`+`MT`。

---

## 9. 编码与提交约定

1. **源码风格**：Apache-2.0 许可头；成员 `m_xxx__`（私有）/ 类型前缀 + `__` 后缀（如 `strName__`）；
   形参 `_Xxx`（`_Name`/`_Help`）；方法 camelCase 小写动词开头；命名空间用宏；注释用中文 Doxygen。
2. **提交信息**：gitmoji `type(scope): emoji message`，scope 取子系统（core/Lexer/Parser/builder/env/example/test/docs）。
3. **README 镜像**：改公共 API 或行为时必须同步更新 `README.md` 与 `README_zh.md`。
4. **测试门槛**：`dfx/unit_test/` 目前 175 个 TEST_CASE / 913 行 REQUIRE/CHECK（2026-10-25 实测：
   全部通过；
   用例按子模块拆分为 19 个 `test_*.cpp`，`main.cpp` 仅保留 Catch2 main 入口，详见
   `dfx/unit_test/unit_test.md`）；任何行为改动应保持或增加覆盖，禁止回退。

---

## 10. 未来路线图（范围外，不得实现）

- 组合/管道语法：`&&`、`|`
- 可变参数：`...`
- 扩展模块：json 配置、lua、dll、bench、meta、mock

---

## 11. Agent 工作守则

1. 修改前先读目标文件与相关头文件（源码即文档）。
2. **修改子目录代码前先读该目录的同名文档**（`argument/argument.md`、`option/option.md`、`env/env.md`、`property/property.md`、`context/context.md`、`expression/expression.md`、`base/base.md`、`config/config.md`、`error/error.md`），理解模块设计后再动手；命名规则与覆盖清单见 §12。
3. 优先最小改动；改公共 API 必须先更新 §4 签名与双语 README。
4. 每次改动后至少运行 `xmake build unit_test` + `xmake run unit_test`；
   涉及示例的改动再跑 `xmake build once`。
5. 新功能默认落在"环境变量一等公民"这条差异主线上。
6. 与用户用中文沟通；注释用中文 Doxygen。

---

## 12. 子目录文档机制（<目录名>.md）

> 目的：为每个模块子目录保留一份"源码补充说明"，让维护者（与 Agent）先读文档再读代码。文档是补充，不替代源码，也不替代双语 README。

> 生成/更新文档时加载项目技能 `dsi-native-docs`（`.dsh/skills/dsi-native-docs/SKILL.md`），并用 12.6 的脚本核验。

### 12.1 命名与位置

- 文档名 = 所在目录名 + `.md`，与目录同级：`context/` → `context/context.md`，`example/once/` → `example/once/once.md`，`dfx/unit_test/` → `dfx/unit_test/unit_test.md`，`extern/magic_enum/` → `extern/magic_enum/magic_enum.md`。
- 每个覆盖子目录恰好一篇；**不再使用 `AGENT.md`**（历史 5 篇已迁移为同名文档，旧名不得复活）。
- 新增子目录时必须同时新增同名文档，否则视为未完成；新增顶层覆盖目录时同步更新 12.3 清单。

### 12.2 文档结构（全部一致）

```markdown
# <目录名> — <一句话定位>

> **摘要**：<2–4 句：本目录在 cmd_args 中的角色、解决的问题、对外可见能力。>
> **联系**：<上级索引 / 子目录文档 / 依赖头文件 / 使用方（示例或单测）/ README_zh 章节；全部相对链接。>

## truth

<只写可从源码直接验证的硬事实：文件→类型/职责表、关键公共签名、数据流与状态机、错误码、常量/阈值、构建 target。>

## 约束

<违反即错误的硬规则：header-only、C++20 双平台、命名空间宏、命名规范、范围外功能、vendored 不修改等。>

## 索引

| 文件 | 关键符号 | 职责 |
| --- | --- | --- |
| `xxx.h` | `Class` / `func` | … |

## 决策

- <决策>：<理由与代价>；<被否方案 / 历史迁移（旧路径、日期）>。

## 教训

- <坑/事故/易错点>：<现象 + 规避方式 + 依据文件>。
```

写作规则：

- 中文正文，标识符/签名保留英文原样；源码优先于任何旧文档，旧文档事实需重新核对后才可沿用。
- **索引必须覆盖目录内全部源码文件**（含 `xmake.lua`、`main.cpp`；vendored 目录登记全部头文件）。
- 行号只用于稳定锚点并注明核对日期（如"2026-10-25 核对"）；链接一律相对路径。
- 已知缺陷如实写进「教训」（例：Scanner 把含 `.` 的数值拆成多 token），并给出依据文件。
- 跨目录概念只在一处详述（如 `OptionGraph` 归 `option/option.md`），其它文档留"联系 + 一句话"。
- `.dsh/plans/*.md` 中的 `AGENT.md` 字样属于历史记录，不得据此改回文件名。

### 12.3 覆盖清单（24 篇）

| 目录 | 文档 |
| --- | --- |
| `include/cmd_args/` | `include/cmd_args/cmd_args.md` |
| `include/cmd_args/argument/` | `argument/argument.md` |
| `include/cmd_args/option/` | `option/option.md` |
| `include/cmd_args/context/` | `context/context.md` |
| `include/cmd_args/expression/` | `expression/expression.md` |
| `include/cmd_args/env/` | `env/env.md` |
| `include/cmd_args/property/` | `property/property.md` |
| `include/cmd_args/base/` | `base/base.md` |
| `include/cmd_args/config/` | `config/config.md` |
| `include/cmd_args/error/` | `error/error.md` |
| `include/cmd_args/extern/` | `extern/extern.md` |
| `include/cmd_args/extern/airbag/` | `extern/airbag/airbag.md` |
| `include/cmd_args/extern/magic_enum/` | `extern/magic_enum/magic_enum.md` |
| `example/` | `example/example.md` |
| `example/chain/` | `example/chain/chain.md` |
| `example/config/` | `example/config/config.md` |
| `example/declare/` | `example/declare/declare.md` |
| `example/deduce/` | `example/deduce/deduce.md` |
| `example/group/` | `example/group/group.md` |
| `example/once/` | `example/once/once.md` |
| `example/validate/` | `example/validate/validate.md` |
| `example/reactive/` | `example/reactive/reactive.md` |
| `dfx/unit_test/` | `dfx/unit_test/unit_test.md` |
| `docs/` | `docs/docs.md`（声明式构建 / 响应式数据 / 数据源绑定的概念模型、盘点与候选决策；单文件） |

### 12.4 维护时机

- 新增/删除/改名的源文件 → 同步该目录文档的「索引」与「truth」。
- 公共 API、默认值、错误码、枚举、构建 target 变化 → 同步受影响的全部文档。
- 新增子目录 → 建同名文档并登记到 12.3；不得只写 README。
- 文档与源码冲突 → 以源码为准，立即修正文档，并把冲突记入「教训」。

### 12.5 核查命令（提交前执行）

优先执行（退出码 0 为通过）：

```bash
node .dsh/skills/dsi-native-docs/scripts/check-docs.mjs include/cmd_args example dfx/unit_test docs
```

无 Node 环境时可用下列手工替代：

```bash
# 1) 覆盖目录内不应再有 AGENT.md
find include/cmd_args example dfx/unit_test docs -name AGENT.md

# 2) 每个目录都存在同名文档
for d in include/cmd_args include/cmd_args/*/ include/cmd_args/extern/*/ example example/*/ dfx/unit_test docs; do
  d=${d%/}; [ -f "$d/$(basename "$d").md" ] || echo "missing: $d/$(basename "$d").md"
done

# 3) 文档中的相对链接（人工确认可解析，或逐个 test -e）
grep -rhoE '\]\([^)#]+' --include='*.md' include/cmd_args example dfx/unit_test docs | sed 's/^](//'
```

### 12.6 配套技能（生成与校验）

- 技能：`dsi-native-docs`，位于 `.dsh/skills/dsi-native-docs/`（`SKILL.md` + `scripts/check-docs.mjs`）。生成/迁移/更新子目录文档前先加载，按其模板、逐节写法与 0–7 工作流执行。
- 校验：`node .dsh/skills/dsi-native-docs/scripts/check-docs.mjs include/cmd_args example dfx/unit_test docs`。脚本递归检查：同名文档存在、无 BOM、H1 含目录名、两行 blockquote（摘要/联系）、五节顺序、相对链接可解析、索引覆盖目录内全部非 md 文件、子目录在正文被提及；失败退出码 1。2026-10-25 实测：24 个目录、24 篇文档全部通过（`docs/` 与 `example/reactive/` 各新增 1 篇）。
- 新增覆盖目录时，把目录加入上述命令，并把文档登记到 12.3。
