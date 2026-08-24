# context — 上下文/解析管线

## 模块概述
Context 为通用容器：持有 Model（数据仓库 + Expression 统一视图）与可注入的执行引擎成员
（Builder 构筑、Compiler 批量编译、Executor 批量执行、Interpreter 流式解释）。
执行方式由调用入口（parse vs once/loop）与数据内容（Model.hasAction）共同决定。

## 文件职责

| 文件 | 类 | 职责 |
|------|---|------|
| `expression.h` | `Expression` / `ExpressionKind_E` | **表达式基类**：Argument/Option/Env 的统一视图，kind() 自动分类 |
| `context.h` | `Context` | **通用容器**：组合 Model+Builder+Compiler+Executor+Interpreter；build/parse/once/loop/model() |
| `compiler.h` | `Compiler` | **编译引擎**（原 Parser）：Lexer → 三 Parser → Expression 分类入库 Model；ensureBuiltins/finalize |
| `interpreter.h` | `Interpreter` | **流式引擎**：Scanner::step 逐字符分词 → Token 完成即分类 → 按 Expression 类型立即执行 |
| `executor.h` | `Executor` | **批量执行器**：hasAction 门控下按 actionOrder 顺序执行 OptionAction |
| `model.h` | `Model` | **数据仓库**：三类 Data + 程序名 + 内置标记 + hasAction() + expressions() 统一视图 |
| `lexer.h` | `Lexer` / `Scanner` | Scanner 状态机（批量 scan）+ step() 增量分词（流式）|
| `builder.h` | `Builder` / `BuildHelper` | 构筑桥接 → ArgumentBuilder / OptionBuilder |
| `parse_fwd.h` | — | 共用类型别名 |

## 批量模式（Context::parse）
```
Compiler::ensureBuiltins → Compiler::parse(tokens)
  → Tokens → 三 Parser → Expression 分类入库 Model
→ if (Model.hasAction()) Executor.execute()
→ Compiler.finalize()
```

## 流式模式（Interpreter::loop / interpret）
```
per-line: Interpreter.interpret(line)
  → reset → Scanner.step 逐字符分词
  → 每个 Token 完成即分类（短回路/Option/命名参数/方位参数）
  → 按 Expression 类型立即执行（回调触发 / fnOnSet__+模板 / set / 缓冲）
  → 方位参数按序提交（数量校验同批量）→ finalize
```

## Compiler vs Interpreter
- **Compiler**（批量）：先编译全部 Token 进 Model；用户 action 由 Executor 后置统一执行。
- **Interpreter**（流式）：逐字符分词，Token 完成即执行；无需 Executor / actionOrder。
- 内置交互选项两条路径均在解析期即时生效。

## 命名约定
- Context 成员：m_strPrjName__ / m_instData__ / m_compiler__ / m_executor__ / m_interp__ / m_bFired__
- 引擎内部挂起状态：strPendingKey / bPendingInlineEq / PendingKind_E（interpreter.h）
