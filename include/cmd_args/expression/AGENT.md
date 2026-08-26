# expression — 表达式/编译引擎层

## 模块概述
本目录承载「词法 → 语法 → 语义」完整前端与两套执行引擎（类型化管道）：
- **词法分析**：Lexer/Scanner 将字节流切分为类型化 Token 流（TokenStream_T）；支持批量 scan() 和增量 pop()
- **语法分析**：ExpressionRecognizer 委托三 Parser 的 recognizeToken() 规则把 Token 序列识别为 ClassifiedExpr
- **语义分析**：Compiler 批量处理 / Interpreter 流式执行共用同一识别器
- **Env 空间**：支持 `::` 级联（A::B::C）词法与语义识别（ScopeSep Token + recognizeEnvAssign）

> 2026-08 目录重组：本目录 7 个头文件自 `context/` 迁入；跨目录引用一律写全路径
> `cmd_args/expression/xxx.h`（勿用裸相对路径或旧 `cmd_args/context/...` 路径）。

## 文件职责

| 文件 | 类 | 职责 |
|------|---|------|
| `token.h` | `Token` / `TokenStream_T` | **公共词法单元**：Type_E 含 ScopeSep；Scanner 产出 / Recognizer 输入 |
| `expression.h` | `Expression` / `ExpressionKind_E` | **表达式基类**：Argument/Option/Env/Conditional/Dependency 统一视图，kind() 自动分类 |
| `classified_expr.h` | `ClassifiedExpr` | **中间表示**：Token 经模式匹配产出的结构化数据（瞬态，不入库 Model）；含 vecSpacePath/strRaw/bPositional |
| `lexer.h` | `detail::Scanner` / `Lexer` | **词法前端**：Scanner 状态机 LexMode_E{CommandLine,EnvLine}；Lexer 类型化门面 scan()/pop()/scanEnvLine()/scanStrings()；兼容 tokens()/tokensFromLine() |
| `expression_recognizer.h` | `ExpressionRecognizer` | **语法前端**：类型化 feed(Token)/finish()/recognizeAll(TokenStream_T)；委托三 Parser 的 recognizeToken() 规则 |
| `compiler.h` | `Compiler` | **批量编译引擎**：ExpressionRecognizer 驱动 → __applyOne() 分派入库 → OptionGraph.evaluate()；兼容旧 Tokens_T 管线 |
| `interpreter.h` | `Interpreter` | **流式执行引擎**：含 ExpressionRecognizer rec__ 成员；getBufferSize() 委托 rec__ |
| `executor.h` | `Executor` | **批量执行器**：hasAction 门控下按 actionOrder 触发 fnOnSet__ + action 模板 |

## 两级表示：Expression vs ClassifiedExpr
- **Expression 基类**是 Model 中持久存储的实体：Argument / OptionValue / OptionAction / environment 均继承之，
  `kind()` 非虚自动分类，`getName()/getHelp()` 虚函数可返回派生类自有存储。
- **ClassifiedExpr** 是解析管线中的瞬态结构化数据，字段分组：通用（name/value/bExplicit）、
  Option 专用（cShortName/bIsBool/bIsBuiltin/bIsAction/strType）、Conditional 专用（strCondition/strThenBranch/strElseBranch）、
  Dependency 专用（strDepSource/strDepTarget）、Env 专用（bEnvNew）；`isRelation()` 判定关系表达式。

## 词法层要点（token.h + lexer.h）
- **token.h** 定义公共 `Token` 类（`Type_E` 含 `ScopeSep` 用于 `::` 级联）和 `TokenStream_T = std::vector<Token>`
- Scanner 支持 `LexMode_E::CommandLine`（默认）和 `LexMode_E::EnvLine`（环境变量行：`::` ScopeSep、`=` 断句、值吸收）
- Scanner 增量 `step(char)` 支持增量喂入；批量 `scan()` 返回完整 TokenStream_T
- Lexer 类型化门面：`scan()`/`scanLine()`/`scanStrings()`/`scanEnvLine()` 返回 `TokenStream_T`；`pop()`/`hasNext()` 增量吐出
- Lexer 兼容外观：`tokens()`/`tokensFromLine()` 基于类型化流归一化实现，返回 `Tokens_T`（字符串列表）

## 语法层要点（expression_recognizer.h）
- 类型化 API：`feed(Token, vector<ClassifiedExpr>&)` / `finish(vector<ClassifiedExpr>&)` / `recognizeAll(TokenStream_T)`
- 归约状态机：PendingKind_E {None, LongOpt, ShortOpt, NamedArg}；SeqKind_E {None, HeadStash, CondAwaitColon, CondAwaitElse}
  LongOpt 拒绝 dash 开头值（除非挂起方带内联 `'='`）；ShortOpt/NamedArg 无条件接受下一 token。
- feed() 流式逐 token（nullopt=仍在收集）；recognizeAll() 批量一次识别；__flushPending 把流尾缺值产出空 value 表达式供下游报 NEED_VALUE。

## 批量管线（Compiler）
```
parse(argc, argv[, envp])
  → Lexer.tokens()                       # argv[0] 程序名原样保留，只扫 argv[1..]
  → envParser.parse(环境源) → ensureBuiltins() → optParser.setGraph(&Model.optionGraph)
  → optParser.parse()                    # 短回路选项命中即 bFired__ 提前返回
  → isOptionsEnded ? argParser.parseNamedOnly+parsePositionalOnly
                   : argParser.parse() + optionGraph.evaluate()
  → 残留 dash token → ErrorParse(UNKNOWN_OPTION)
```
- tryParse* 三重载 noexcept 包装：捕获 ErrorParse / ErrorLexer / std::exception 返回 error_code。
- 后置步骤：resetData()（还原默认值）/ ensureBuiltins()（懒注册一次）/ finalize()（seed env 兜底 + ${VAR} 展开 + save-config 写出）。
- 异常路径回滚：clearParseOrder + optionGraph.clear 后 rethrow。

## 流式管线（Interpreter）
```
interpret(line):
  reset(args/opts) + __lineReset()
  → idxCursor__ 逐字符 _Sc.step(c) → vecStack__ 压栈 → __reduceStack()
  → 流尾 step('\0') flush
  → strPendingKey__ 残留报 NEED_VALUE；vecDashLeft__ 报 UNKNOWN_OPTION
  → __assignPositional 数量校验按序提交 → __finalize()
```
归约五规则（与批量 Parser 语义逐条对齐）：
1. 有挂起取值方：栈顶 token 即值 → 组合立即执行并清除挂起；
2. `'--'` 之后一律方位参数；
3. `--name[=v]`：短回路即回调、bool 即完成、非 bool 转挂起键；
4. `-x` / `-abc` 聚合：bool 位即刻完成；值选项吃余串或挂起等下个 token；
5. 其余 token 入方位缓冲（流空后统一校验数量再按序提交）。
关系语法即时处理：__isRelationText → __classifyRelationText → __handleRelation
（条件边查条件选项 parse_bool 决定激活 then/else 分支；依赖边激活 target）。

## 执行时序对比
| 路径 | 引擎组合 | action 触发时机 |
|------|---------|----------------|
| 批量（ContextStep） | Compiler + Executor | 内置交互选项解析期内联即时；用户 action 由 Executor 在 hasAction 门控下按 actionOrder 后置统一执行 |
| 流式（ContextRuntime） | Interpreter | 全部经 __executeOptionImmediate 即时触发（fnOnSet__ 回调 + action 模板 system_call），不经 Executor/actionOrder |

## 依赖方向（禁止反向）
```
expression.h ← classified_expr.h ← 三 Parser 的 recognize()
lexer.h ← expression_classifier.h / compiler.h / interpreter.h
interpreter.h   ⇢ 私有继承 ArgumentParser / OptionParser / EnvParser
compiler.h      ⇢ 组合 ArgumentParser/OptionParser/EnvBuilder/EnvParser
                  + context/{builtin_options,describe,model,parse_fwd}.h
executor.h      ⇢ 仅依赖 option 层 + cmd_args/context/model.h
```

## 命名约定
- Compiler 成员：argParser__ / optParser__ / envBuilder__ / envParser__ / pModel__ / program_name / description / bFired__
- Interpreter 流式状态（每次 interpret 经 __lineReset 复位）：idxCursor__ / vecStack__ /
  strPendingKey__ / enPendingKind__(PendingKind_E) / bPendingInlineEq__ / bOptEnd__ / vecPos__ / vecDashLeft__
- Scanner：enState__(Phrase_E) / tblScanInput__ / tCur__（流式当前 token）/ tResult__
- ExpressionRecognizer：vecBuffer__ / strPendingKey__ / enPendingKind__ / bPendingInlineEq__ / bOptEnd__ / pOptParser__ / pArgParser__ / pEnvParser__
