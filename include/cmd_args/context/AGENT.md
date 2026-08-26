# context — 上下文/解析管线

## 模块概述
Context 为通用容器：持有 Model（数据仓库 + Expression 统一视图）与可注入的执行引擎成员
（Builder 构筑、Compiler 批量编译、Executor 批量执行、Interpreter 流式解释）。
执行方式由调用入口（parse vs once/loop）与数据内容（Model.hasAction）共同决定。

## 文件职责

> 2026-08 目录重组：表达式基类、词法/语法前端与三引擎已迁至 `expression/`
> （expression.h / classified_expr.h / expression_classifier.h / lexer.h /
> compiler.h / interpreter.h / executor.h），详见 `../expression/AGENT.md`。

| 文件 | 类 | 职责 |
|------|---|------|
| `context.h` | `Context` | **通用容器**：组合 Model+Builder+Compiler+Executor+Interpreter |
| `model.h` | `Model` | **数据仓库**：三类 Data + OptionGraph + 程序名 + 内置标记 |
| `builder.h` | `Builder` / `BuildHelper` | 构筑桥接 → ArgumentBuilder / OptionBuilder |
| `context_fwd.h` | — | 共用类型别名（Tokens_T / Envs_T / TokenStream_T） |

## Parser Token 规则接口（编译前端）

每个 Parser 提供类型化 Token→Expression 转换接口，供 ExpressionRecognizer 委托：
- `OptionParser::recognizeToken(Token)` — 单 Token 语法分类（LongName/ShortName/ContinualName）
- `OptionParser::applyExpr(ClassifiedExpr)` — Type-safe 应用（写入 OptionData）
- `OptionParser::applyRelation(ClassifiedExpr)` — 结构化关系入图
- `ArgumentParser::recognizeToken(Token)` — 命名参数 / 方位候选
- `ArgumentParser::applyExpr(ClassifiedExpr, vector<string>&)` — named 分配 / positional 入缓冲
- `ArgumentParser::assignPositional(vector<string>)` — 数量校验 + 按序提交（消除重复实现）
- `EnvParser::recognizeEnvAssign(TokenStream_T, size_t)` — Env 行模式序列匹配（Variable(::Variable)* = Value）
- `EnvParser::applyExpr(ClassifiedExpr)` — 写入 EnvData

`ExpressionRecognizer` 可委托 Parser 规则（通过构造函数注入 Parser 引用）。

## 批量模式（Compiler 管线）
```
Lexer → Tokens → OptionParser.parse() [→ OptionGraph.addEdge()] → OptionGraph.evaluate()
         → ArgumentParser.parse() → finalize()
```

## 流式模式（Interpreter 管线）
```
Scanner.step() → Token → ExpressionRecognizer.feed() → ClassifiedExpr
         → __reduceStack() [→ __handleRelation()] → 立即执行
```

## Option 关系语法（OptionGraph）
```
-a?b:c    → Conditional：a 为真时激活 b，否则激活 c
-a?b:_    → Conditional：a 为真时激活 b，否则无操作
-a->b     → Dependency：先执行 a，再执行 b（拓扑序）
```

## Compiler vs Interpreter
- **Compiler**（批量）：OptionParser 解析时向 OptionGraph 注册关系边，解析完成后 evaluate() 激活目标。
- **Interpreter**（流式）：`__reduceStack` 检测关系 token，`__handleRelation` 即时判断条件并激活。
- 内置交互选项两条路径均在解析期即时生效。

## 命名约定
- Context 成员：strPrjName_ / instData_ / compiler__ / executor__ / interp__ / bFired__
- 引擎内部流式状态：idxCursor__ / vecStack__ / strPendingKey__ / enPendingKind__ / bPendingInlineEq__ / bOptEnd__ / vecPos__ / vecDashLeft__（expression/interpreter.h）
- OptionGraph：OptionData.optionGraph（model.h）；OptionParser.pGraph__（option_parser.h）
- Parser 规则：OptionParser/ArgumentParser/EnvParser 各自的 recognize() + apply() 方法
