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
   └──►parse()──►    Parser            （纯解析：选项 / 命名参数 / 方位参数分配）
                         │
                         ▼
                       Model            （ArgumentData + OptionData + EnvData）
                         │
                    ┌────┴────┐
                    ▼         ▼
              Executor    Interpreter  （批量: 后置 action 执行 / REPL: 逐 Token 解析即执行）
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

---

## 3. 源码地图

> 各子目录的详细模块说明见对应的 `AGENT.md`（`argument/AGENT.md`、`option/AGENT.md`、`env/AGENT.md`、`context/AGENT.md`）。

```
include/cmd_args/
├── cmd_args.hpp                    # 唯一入口：只 include context/context.h
├── base/
│   ├── def.h                       # 命名空间宏 CMD_ARGS_NAMESPACE_BEGIN/END、ARRAY_SIZE、ENUM_NAME
│   ├── util.h                      # parse_value<T> / parse_bool / type_string / starts_with / trim 等
│   └── defer.hpp                   # RAII defer
├── context/                        # 上下文/解析管线（详见 context/AGENT.md）
│   ├── context.h                   # Context 门面：build/parse/once/loop/env
│   ├── builder.h                   # Builder 构筑桥接
│   ├── parser.h                    # Parser 纯解析层
│   ├── interpreter.h               # Interpreter 实时引擎
│   ├── executor.h                  # Executor 后置 action 执行
│   ├── model.h                     # Model 数据仓库
│   ├── lexer.h                     # Lexer/Scanner 状态机
│   └── parse_fwd.h                 # 类型前置声明
├── argument/                       # 参数子系统（详见 argument/AGENT.md）
│   ├── argument.h                  # Argument / ArgumentNamed / ArgumentPosition
│   ├── argument_builder.h          # ArgumentBuilder 构筑层
│   ├── argument_data.h             # ArgumentData + NamedArgumentMgr / PositionArgumentMgr
│   └── argument_parser.h           # ArgumentParser 解析层
├── option/                         # 选项子系统（详见 option/AGENT.md）
│   ├── option.h                    # Option / OptionValue / OptionAction
│   ├── option_builder.h            # OptionBuilder 构筑层
│   ├── option_data.h               # OptionData + ValueOptMgr / ActionOptMgr + actionOrder
│   └── option_parser.h             # OptionParser 纯解析层（virtual hook 模式）
├── env/                            # 环境变量子系统（详见 env/AGENT.md）
│   ├── env.h                       # environment 条目结构
│   ├── env_data.h                  # EnvData 数据仓库（std::map）
│   ├── env_builder.h               # EnvBuilder 写操作
│   └── env_parser.h                # EnvParser 解析/服务层
├── config/                         # 配置桥接
│   ├── yaml.h                      # 极简 YAML 子集（yaml_doc）
│   ├── parameter.h                 # parameter_entry / parameter_schema / parameter_snapshot
│   └── config_store.h              # 三类输入源转换桥接
├── error/                          # 错误体系
│   ├── errors.h                    # 错误堆栈（dump/getLast/isOk）
│   ├── builder_error.h             # BuildError_E / ErrorArgument
│   ├── lexer_error.h               # LexError_E / ErrorLexer
│   ├── parser_error.h              # ParseError_E / ErrorParse
│   ├── interpreter_error.h         # InterpError_E / ErrorInterp
│   ├── exception_handler.h         # 可选：纯虚调用/崩溃处理
│   └── functional_util.h
└── extern/
    ├── airbag/                     # 内置异常/崩溃辅助
    └── magic_enum/                 # 内置枚举反射（vendored）

example/once/main.cpp               # 演示程序：env 一等公民完整故事
example/config/main.cpp             # config_demo：三类输入源转换（YAML 往返 + 反哺）
dfx/unit_test/head.cpp              # 单元测试（Catch2，仅公共 API）
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

// ── 解析（三形态）────────────────────────────────────
app.parse(argc, argv, envp);                            // 抛 ErrorParse/ErrorLexer
std::error_code ec; app.parse(ec, argc, argv, envp);     // noexcept
app.parse(ec, tokens, envs);                            // token 流形态（loop 复用）

// ── 数据读取 ─────────────────────────────────────────
std::string s = app.getArgument<std::string>("hello");
int t = app.getOption<int>("timeout");
app.hasOption("--timeout");  app.isParsed("verbose");

// ── 环境变量交互 ─────────────────────────────────────
app.has_env("KEY"); app.get_env("KEY"); app.get_env_or("KEY", "<missing>");
app.set_env("KEY", "VALUE"); app.unset_env("KEY");
size_t n = app.apply_env();
std::string e = app.expand_env("${VAR}!");
std::cout << app.env_string();

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

> 详细说明见 `include/cmd_args/env/AGENT.md`。以下为摘要。

- **优先级链**：CLI 显式值 > 配置文件(YAML) > 环境变量 > 内置默认值
- **展示标记**：`*`（被修改）/ `>`（被引用）/ `+`（本次新建）
- **展开**：`${NAME}` / `%NAME%` / `$NAME`（未命中保持原样）
- **回写**：`apply()` 只回写 `bModified__ || bNew__` 条目
- **会话态**：env 具会话跨行保留（loop 逐行 reset 不清 env）

---

## 6. Lexer / Parser 行为约定

> 详细说明见 `include/cmd_args/context/AGENT.md`。以下为关键规则摘要。

- token 归一化：`--foo=bar`（长+值）、`-x=value`（短+值）、`-abc`（聚合短标志）、
  `KEY=VALUE`（Continual+`=`）、`x.y`（piece）、`-5`（数字短名）均被 Scanner 正确切分。
- 变量值吸收**全部非空白字符**（值内可含 `. - = \ / : @`）。
- `--` 之后全部方位参数；此前命名参数先经 `parseNamedOnly` 消化。
- 选项 action（system_call）：Parser 纯解析不触发 action；批量模式由 Executor 后置按 actionOrder 统一执行；REPL 由 Interpreter 的 virtual hook 逐 Token 内联执行。
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
4. **测试门槛**：`dfx/unit_test/head.cpp` 目前 39 个 TEST_CASE / 215 条断言；
   任何行为改动应保持或增加覆盖，禁止回退。

---

## 10. 未来路线图（范围外，不得实现）

- 组合/管道语法：`&&`、`|`
- 可变参数：`...`
- 扩展模块：json 配置、lua、dll、bench、meta、mock

---

## 11. Agent 工作守则

1. 修改前先读目标文件与相关头文件（源码即文档）。
2. **修改子目录代码前先读该目录的 `AGENT.md`**（`argument/AGENT.md`、`option/AGENT.md`、`env/AGENT.md`、`context/AGENT.md`），理解模块设计后再动手。
3. 优先最小改动；改公共 API 必须先更新 §4 签名与双语 README。
4. 每次改动后至少运行 `xmake build unit_test` + `xmake run unit_test`；
   涉及示例的改动再跑 `xmake build once`。
5. 新功能默认落在"环境变量一等公民"这条差异主线上。
6. 与用户用中文沟通；注释用中文 Doxygen。
