# context — 上下文/解析管线

## 模块概述
`cmd_args` 的顶层管线：Context 作为核心服务对象，组合 Model（数据仓库）、Builder（构筑）、Parser（纯解析）、Executor（action 执行）、Interpreter（实时引擎），对外提供 build/parse/once/loop/env 交互门面。

## 文件职责

| 文件 | 类 | 职责 |
|------|---|------|
| `context.h` | `Context` | 核心门面：组合 Model+Parser+Executor+Interpreter；build/parse/once/loop/env 交互 |
| `builder.h` | `Builder` / `BuildHelper` | 构筑桥接：把 Context::build/addOption/addFlag 转交给 ArgumentBuilder/OptionBuilder |
| `parser.h` | `Parser` | **纯解析层**（组合 ArgumentParser+OptionParser+EnvParser+EnvBuilder）；含 ensureBuiltins（内置选项懒注册）、finalize（seed 环境兜底+展开+save-config） |
| `interpreter.h` | `Interpreter` | **实时引擎**：继承三解析器，override `onOptionAssigned` 虚拟钩子实现逐 Token 内联 action 执行；提供 interpret/once/loop |
| `executor.h` | `Executor` | **后置执行器**：按 actionOrder 顺序执行 OptionAction（fnOnSet__ + action template） |
| `model.h` | `Model` | 数据仓库：`ArgumentData` + `OptionData` + `EnvData` + 程序名 + 内置选项标记 + saveConfigPath |
| `parse_fwd.h` | — | 类型前置声明：`Name_T` / `NameShort_T` / `NameLong_T` / `Desc_T` / `Tokens_T` / `Envs_T` |
| `lexer.h` | `Lexer` / `Scanner` | Scanner 状态机：argv/行文本 → 归一化 token 流（支持长/短/聚合/KEY=VALUE/数字短名） |

## 三层架构（解析管线）

```
Context（门面）
  ├── Model        数据仓库（ArgumentData + OptionData + EnvData）
  ├── Builder      构筑桥接 → ArgumentBuilder / OptionBuilder
  ├── Parser       纯解析层（组合三解析器 + ensureBuiltins + finalize）
  ├── Executor     后置执行（action 按 actionOrder 执行）
  └── Interpreter  实时引擎（继承三解析器 + onOptionAssigned hook + finalize）
```

## 解析流程

### 批量模式（Context::parse）
```
ensureBuiltins → Parser::parse(tokens) → Executor::execute() → Parser::finalize()
```
- Parser::parse：EnvParser::parse(Env) + OptionParser::parse + ArgumentParser::parse（均纯解析，不触发 action）
- Executor：遍历 OptionData.actionOrder，按顺序触发 fnOnSet__ + action template
- Parser::finalize：seed 环境兜底 → ${VAR} 展开 → save-config 写出

### REPL 模式（Interpreter::loop）
```
per-line: Interpreter::interpret(line)
  → resetData → tokenize → 继承 parse（onOptionAssigned hook 内联触发 action）→ finalize
```

## Parser 纯解析 vs Interpreter 实时
- **Parser**：OptionParser.__assign 仅赋值+标记（virtual hook = no-op），action 由 Executor 后置统一执行。
- **Interpreter**：继承 OptionParser，override `onOptionAssigned` 在解析期内联触发 action，实现逐行实时执行。

## Context::parse 行为约定
- `argc, argv` 形态：argv[0] 为程序名（原样保留），argv[1..] 词法扫描。
- `tokens, envs` 形态：直接传入 Parser（供 loop/token 流复用）。
- noexcept 变体：异常捕获后写入 ec 并记入错误堆栈（`Errors::setLast`）。

## 模块命名约定
- 类型：PascalCase（`Context` / `Parser` / `Interpreter` / `Executor` / `Model`）。
- 数据层（Data）：`ArgumentData` / `OptionData` / `EnvData`。
- Parser 内部：`m_argParser__` / `m_optParser__` / `m_envBuilder__` / `m_envParser__`。
- Context 成员：`m_strPrjName__` / `m_instData__` / `m_parser__` / `m_executor__` / `m_interp__` / `m_bFired__`。
