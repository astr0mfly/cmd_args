# context — 上下文/解析管线

## 模块概述
Context 为通用容器：持有 Model（数据仓库 + Expression 统一视图）与可注入的执行引擎成员
（Builder 构筑、Compiler 批量编译、Executor 批量执行、Interpreter 流式解释）。
执行方式由调用入口（parse vs once/loop）与数据内容（Model.hasAction）共同决定。

## 文件职责

| 文件 | 类 | 职责 |
|------|---|------|
| `expression.h` | `Expression` / `ExpressionKind_E` | **表达式基类**：Argument/Option/Env/Conditional/Dependency 的统一视图 |
| `classified_expr.h` | `ClassifiedExpr` | **中间表示**：Token 经模式匹配后产出的结构化数据 |
| `expression_classifier.h` | `ExpressionRecognizer` | **编译前端**：Token 序列 → Expression 模式匹配，可委托 Parser 规则 |
| `context.h` | `Context` | **通用容器**：组合 Model+Builder+Compiler+Executor+Interpreter |
| `compiler.h` | `Compiler` | **编译引擎**：Lexer → Tokens → 三 Parser → Expression 入库 Model；OptionGraph.evaluate() |
| `interpreter.h` | `Interpreter` | **流式引擎**：idxCursor 游标逐字符泵入 Scanner → token 压栈 → 凑满一个 Expression 立即归约执行 |
| `executor.h` | `Executor` | **批量执行器**：hasAction 门控下按 actionOrder 顺序执行 OptionAction |
| `model.h` | `Model` | **数据仓库**：三类 Data + OptionGraph + 程序名 + 内置标记 |
| `lexer.h` | `Lexer` / `Scanner` | Scanner 状态机（批量 scan）+ step() 增量分词（流式）；`__normalize` 合并关系语法 token |
| `builder.h` | `Builder` / `BuildHelper` | 构筑桥接 → ArgumentBuilder / OptionBuilder |
| `parse_fwd.h` | — | 共用类型别名 |

## Parser 规则化架构

每个 Parser 内置自己的 Expression 识别规则：
- `OptionParser::recognize()` — 识别 `--name[=value]`、`-x`、`-abc`、`-a?b:c`、`-a->b`
- `ArgumentParser::recognize()` — 识别 `--name=value`、`--name value`、方位参数
- `EnvParser::recognize()` — 识别 `KEY=VALUE`

每个 Parser 内置自己的应用逻辑：
- `OptionParser::apply()` — 写入 OptionData + 触发回调
- `ArgumentParser::apply()` — 写入 ArgumentData
- `EnvParser::apply()` — 写入 EnvData

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
- 引擎内部流式状态：idxCursor__ / vecStack__ / strPendingKey__ / enPendingKind__ / bPendingInlineEq__ / bOptEnd__ / vecPos__ / vecDashLeft__（interpreter.h）
- OptionGraph：OptionData.optionGraph（model.h）；OptionParser.pGraph__（option_parser.h）
- Parser 规则：OptionParser/ArgumentParser/EnvParser 各自的 recognize() + apply() 方法
