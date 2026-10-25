# expression — 词法/语法前端与批量、流式双引擎

> **摘要**：本目录承载 cmd_args 的「词法 → 语法 → 语义」前端与两套执行引擎：`Scanner`/`Lexer` 把字节流切成类型化 `Token`，`ExpressionRecognizer` 委托三 Parser 的规则归约为 `ClassifiedExpr`，`Compiler`（批量）与 `Interpreter`（流式）再把它落到 `Model`，`Executor` 负责批量模式的后置 action，`validation.h` 提供统一校验。`Expression` 与 `ClassifiedExpr` 构成「持久实体 / 瞬态中间表示」两级表示。
> **联系**：[上级索引 README_zh 管线](../../../README_zh.md#管线)与 [Expression 表达式类层次](../../../README_zh.md#expression-表达式类层次)；同级文档 [context/context.md](../context/context.md)、[env/env.md](../env/env.md)；依赖头文件 [../option/option_parser.h](../option/option_parser.h)、[../argument/argument_parser.h](../argument/argument_parser.h)、[../env/env_parser.h](../env/env_parser.h)、[../context/model.h](../context/model.h)；使用方 [example/once/main.cpp](../../../example/once/main.cpp)、[dfx/unit_test/test_expression.cpp](../../../dfx/unit_test/test_expression.cpp)。

## truth

目录内 10 个源码文件，全部为头文件内联实现：

- `token.h`：`class Token` —— `enum class Type_E : char`{`Unknown`, `LongName`, `ShortName`, `ContinualName`, `Variable`, `Integer`, `Real`, `String`, `EnvName`, `Delimiter`, `ScopeSep`, `Keyword`}，`enum class KeyWord_E : char`{`Env_`, `File_`, `Plugin_`}；方法 `getAttr/getName/back/setLoc/setAttr/pick/resetName/setInlineEq/hasInlineEq/clear/getLoc/toText` 与 `operator bool`；`toText()` 规则：LongName → `"--"`+name、ShortName/ContinualName → `"-"`+name、ScopeSep → `ENV_SCOPE_SEP`（`CMD_ARGS_ENV_SCOPE_SEP`，默认 `"::"`）、其余原名；`using TokenStream_T = std::vector<Token>`。
- `expression.h`：`enum class ExpressionKind_E : char { Argument, Option, Env, Conditional, Dependency }`；`class Expression` 构造 (kind, name, help)，`kind()` 非虚、`getName()`/`getHelp()` 虚函数，保护方法 `_setName`/`_setHelp`。
- `classified_expr.h`：`struct ClassifiedExpr` 字段分组 —— 通用 `kind/name/value/bExplicit`；Option `cShortName/bIsBool/bIsBuiltin/bIsAction/strType`；Conditional `strCondition/strThenBranch/strElseBranch`；Dependency `strDepSource/strDepTarget`；Env `bEnvNew`、`vecSpacePath`、`strRaw`、`bPositional`；方法 `isRelation()`（Conditional 或 Dependency）与 `fullPath()`（按 `ENV_SCOPE_SEP` 拼接空间路径，默认 `A::B::C`）。
- `scanner.h`：`expression_detail::Scanner` —— `enum class LexMode_E : char { CommandLine, EnvLine }`；`enum class Phrase_E` 15 态（BEGIN/IN_KEYWORD/IN_STRING/IN_INTEGER/IN_REAL/IN_SIENCE/IN_LONG_NAME/IN_SHORT_NAME/IN_CONTINUAL_NAME/IN_VARIABLE/IN_SINGLE_DELIMITER/IN_DOUBLE_DELIMITER/IN_SCOPE_SEP/IN_ENV_VALUE/END）；状态分派表 `tblScanInput__`（`std::map<Phrase_E, ScanProc_T>`）；`Scanner(Source_T, LexMode_E = CommandLine)`、`scan()`、`step(char)`（`'\0'` 表示流结束并 flush）。
- `lexer.h`：`expression_detail::Lexer` 门面 —— 构造 (argc/argv/envp)、(string line)、(Tokens_T)；批量 `scan()`/`scanLine()`/`scanStrings()`/`scanEnvLine()`；增量 `hasNext()`/`pop()`；兼容外观 `run()`/`tokens()`/`tokensFromLine()`（基于类型化流归一化为字符串流，含关系与 `'='` 合并）。
- `expression_recognizer.h`：`class ExpressionRecognizer` 构造（OptionParser*/ArgumentParser*/EnvParser*）；`feed(Token const&, std::vector<ClassifiedExpr>&)`、`finish(std::vector<ClassifiedExpr>&)`、`recognizeAll(TokenStream_T const&)`、`reset()`、`hasPending()`、`getBufferSize()`；私有 `PendingKind_E{None, LongOpt, ShortOpt, NamedArg}`、`SeqKind_E{None, HeadStash, CondAwaitColon, CondAwaitElse}`、`__tryConsumePending/__tryReduce/__makePositional/__makeAggregateCandidate`。
- `compiler.h`：`class Compiler` 构造 (Model*)；`set_program_name`/`set_description`；`parse(int, char const*[], char*[] = nullptr)` 与 `parse(Tokens_T const&, Envs_T const&)`；`tryParse` 三重载 `noexcept`；`isParsed` 两重载、`isFired`、`get_program_name`、`getOption<T>`/`getArgument<T>`；`print_usage/print_help/print_env`；`resetData/ensureBuiltins/finalize`；私有 `__tryParse/__parseTokensLegacy/__parseTokens/__applyOne/__seedEnvDefaults/__expandEnvValues/__descUsage/__descHelp/__descEnv/__descUsageOf/__descHelpOf/__descEnvOf`；成员 `argParser__/optParser__/envBuilder__/envParser__/pModel__/program_name/description/bFired__`。
- `interpreter.h`：`class Interpreter : private ArgumentParser, private OptionParser, private EnvParser`；`interpret(std::string const&, Errors&) -> std::error_code`；`finalize()`、`setExitCode/exitCode/markParsed/prepared`、`describeError`、`getCursor()`/`getStackSize()`、`once()`（`[[noreturn]]`）、`loop(Errors&)`；私有 `__lineReset/__feedChar/__reduceStack/__handleRelation/__isRelationText/__classifyRelationText/__executeOptionImmediate/__assignPositional/__seedEnvDefaults/__expandEnvValues/__finalize`；成员 `pModel__/rec__/bParsed__/nExitCode__/idxCursor__/vecStack__/strPendingKey__/enPendingKind__/bPendingInlineEq__/bOptEnd__/vecPos__/vecDashLeft__`。
- `executor.h`：`class Executor` 构造 (Model*)；`execute()` 按 `opts.actionOrder()` 逐项触发 `fnOnSet__(strValue)`、把 action 模板中的 `{value}` 替换后交 `std::system`，非 0 退出码写 stderr `(action) command exit N: cmd`，最后 `clearParseOrder()`。
- `validation.h`（namespace `validate_detail`）：`struct Result { std::error_code ec; std::string msg; bool ok() }`；`checkEnvSpecs(Model&)`（缺省补全 + 必存在性 + 值校验，`MISSING_ENV`/`VALIDATION_FAILED`）、`checkRequired(Model const&)`（`MISSING_ARGUMENT`/`MISSING_OPTION`，首错即返）、`checkValues(Model const&)`（`VALIDATION_FAILED`）、`validate(Model&)`（顺序：env spec → required → values）。

两级表示：`Expression` 是 `Model` 中持久存储的实体（`Argument` / `OptionValue` / `OptionAction` / `environment` 均继承之，`kind()` 构造期自动分类），`ClassifiedExpr` 是解析管线中的瞬态结构化数据、不入库；下游按 `kind` 分派 apply 到三类 Data。

批量管线（`Compiler`）：`parse(argc, argv, envp)` → 程序名取 `argv[0]` → `Lexer(argc - 1, argv + 1).scan()` → `__parseTokens` 把每个 `Token` 经 `toText()` 变回字符串流 → `__parseTokensLegacy`：
1) `envParser__.parse(_Env)` → `ensureBuiltins()` → `optParser__.setGraph(&pModel__->optionGraph)`；
2) `bFired__ = optParser__.parse(tokens)`，短路即返回；
3) `isOptionsEnded()` 为真 → `parseNamedOnly` + `parsePositionalOnly`，否则 `argParser__.parse` + `optionGraph.evaluate()`；
4) 仍以 `'-'` 开头的残留 token → `ErrorParse(ParseError_E::UNKNOWN_OPTION)`；
5) `catch(...)` → `clearParseOrder()` + `optionGraph.clear()` 后 rethrow。
`finalize()`：`__seedEnvDefaults`（跳过已有值/外部赋值，env 命中则 set + `markEnvHit`/`bEnvHit__`）→ `__expandEnvValues`（以 `property().raw()` 为源、`setRaw` 旁路，所有参数/选项值过 `EnvParser::expand`）→ `validate`（失败抛 `ErrorValidation`）→ `--save-config` 写出。
注意：`__applyOne(ClassifiedExpr&&, std::vector<std::string>&)` 已实现按 kind 分派（Option/Conditional/Dependency/Argument/Env），但全文件无调用点；批量路径不经过 `ExpressionRecognizer`。

流式管线（`Interpreter`）：`interpret(line, Errors&)` → 复位全部参数/选项值 → `clearLast()` → `Scanner sc({})` → `__lineReset()` → 逐字符 `__feedChar`（`Scanner::step` 出 token 即压 `vecStack__`，随后 `__reduceStack()`）→ 流尾 `step('\0')` flush → 挂起取值按 `NamedArg`/`LongOpt`/`ShortOpt` 三种文案报 `NEED_VALUE` → `vecDashLeft__` 非空报 `UNKNOWN_OPTION` → `__assignPositional`（数量校验：`need` 为非外部方位参数数、`total` 为全部；越界抛 `UNKNWON_ARGS` 并附 `give N, but need A..B`）→ `__finalize`（seed + expand + validate + save-config）→ `bParsed__ = true`；异常按 `ErrorParse`/`ErrorLexer`/`ErrorValidation` 捕获并 `setLast` 返回错误码，`std::exception` → `setMsg` + `UNKNWON_ARGS`。

归约五规则（`__reduceStack`，与批量 Parser 语义逐条对齐）：1) 有挂起取值方则栈顶即值并立即执行；2) `--` 之后一律方位参数；3) `--name[=v]`：短回路/bool 即刻完成，非 bool 转挂起；4) `-x`/`-abc` 聚合：bool 位即刻完成，值选项吃余串或挂起等下个 token；5) 其余 token 入方位缓冲。取值消费规则：`LongOpt` 拒绝 `'-'` 开头值（除非挂起方带内联 `'='`），`ShortOpt`/`NamedArg` 无条件接受下一个 token。关系语法即时处理：`__isRelationText`（长度 ≥ 4、以 `'-'` 开头且含 `'?'` 或 `"->"`）→ `__classifyRelationText` → `__handleRelation`（Conditional 查条件选项 `util::parse_bool` 激活 then/else，Dependency 激活 target，值统一 `"1"`）。

词法模式：`LexMode_E::CommandLine` 为默认（argv；长名遇 `'='` 或空白结束并 `setInlineEq(true)`）；`LexMode_E::EnvLine` 用于 envp 行（Variable 遇 `':'`/`'='`/`ENV_SCOPE_SEP[0]` 结束；首字符命中即进入 `IN_SCOPE_SEP`，按 `nScopeMatched__` 与 `ENV_SCOPE_SEP_LEN` 逐字符比对：凑满定型 `ScopeSep`，不足则退化为 `Delimiter` 并把当前字符留给 `BEGIN` 重处理；`'='` 置 `bEnvValuePos__` 后进入 `IN_ENV_VALUE` 吸收整段值）。

action 时机：`Compiler` 路径内置短回路选项在识别期 `fireAction` 即回调，用户 action 记录进 `actionOrder` 由 `Executor` 在 finalize 后统一执行；`Interpreter` 路径全部即时（`__executeOptionImmediate` 内触发 `fnOnSet__` + action 模板），不使用 `Executor`。

已知词法缺陷：含 `'.'` 的数值（如 `127.0.0.1`）会被拆成多个 token。机制：`__processBegin` 遇数字进入 `IN_INTEGER`，遇 `'.'` 转 `IN_REAL`；`__processReal` 再遇 `'.'` 即 `setAttr(Real)` + END，随后 `'.'` 落入 `IN_SINGLE_DELIMITER` 产出 Delimiter。证据：`example/validate/main.cpp:17-18` 头部注释明写「argv 路径下 Scanner 会把含 '.' 的数值值（如 127.0.0.1）拆分为多 token（既有词法缺陷，见 README 已知限制/展望）」，`README.md:324` 亦列为已知限制。

`Interpreter` 的 `rec__` 是「类型化管道预留」成员：构造函数注入三 Parser，`getStackSize()` 委托 `rec__.getBufferSize()`，但全文件没有调用 `rec__.feed()`/`reset()`，故 `getStackSize()` 恒为 0。

## 约束

- header-only、C++20 双平台；不得引入需要链接的实现文件；平台 API 必须条件编译（如 action 用 `std::system` 属双平台一致接口）。
- 命名空间只经 `CMD_ARGS_NAMESPACE_BEGIN`/`CMD_ARGS_NAMESPACE_END`；命名规范：私有成员 `__` 后缀、形参 `_` 前缀、返回类型非 4 字符用 `auto ... -> T` 后置。
- 两级表示边界：`ClassifiedExpr` 只作管线瞬态，禁止入库 `Model`；持久实体必须继承 `Expression` 并让 `kind()` 在构造期分类。
- 依赖方向：本目录可依赖 `option/`、`argument/`、`env/` 与 `context/{context_fwd,model,describe,builtin_options}.h`；禁止 include 宿主层 `context/context_base.h`、`context/context.h`、`context/context_step.h`、`context/context_runtime.h`（会形成反向依赖）。
- `Token::Type_E` 与 `Token::toText()` 必须同步：新增类型时同时更新 `toText()`、`Scanner::__flush()` 的属性映射以及识别器/解释器分支。
- Env 空间分隔符只认 `base/def.h` 的 `CMD_ARGS_ENV_SCOPE_SEP`：`Token::toText()`（`token.h`）、`ClassifiedExpr::fullPath()`（`classified_expr.h`）、Scanner 的 `IN_SCOPE_SEP` 状态（`scanner.h`）都必须引用 `ENV_SCOPE_SEP`/`ENV_SCOPE_SEP_LEN`，禁止再写 `"::"` 字面量。
- 词法状态只能经 `tblScanInput__` 分派：新增 `Phrase_E` 必须同时注册 `__initScanTable()` 与 `__flush()` 映射。
- `Lexer` 是门面：不得在 `Lexer` 内复制分词规则，批量与增量都必须走 `Scanner`。
- 语义一致性：`Interpreter::__reduceStack` 的取值规则必须与 `OptionParser`/`ArgumentParser` 对齐（LongOpt 拒绝 dash 值、ShortOpt/NamedArg 接受）；改一侧必须同步另一侧。
- `extern/magic_enum` 属 vendored，不得修改。
- 测试只经公共 API（`Context`/`Model`、`Lexer` 门面、`EnvParser`）；不得直接构造 `Scanner` 私有状态，Scanner 行为经 `Lexer::scanLine`/`tokensFromLine` 验证。

## 索引

| 文件 | 关键符号 | 职责 |
| --- | --- | --- |
| `token.h` | `Token` / `Type_E` / `KeyWord_E` / `TokenStream_T` | 公共词法单元：类型化 token 与文本归一化 |
| `expression.h` | `Expression` / `ExpressionKind_E` | 表达式基类：五类持久实体的统一视图 |
| `classified_expr.h` | `ClassifiedExpr` / `isRelation` / `fullPath` | 分类后的瞬态中间表示 |
| `scanner.h` | `Scanner` / `LexMode_E` / `Phrase_E` | 词法状态机：批量 scan 与增量 step |
| `lexer.h` | `Lexer` / `scanLine` / `tokensFromLine` / `pop` | 词法门面：argv/整行/env 行入口与增量吐 token |
| `expression_recognizer.h` | `ExpressionRecognizer` / `feed` / `finish` / `recognizeAll` | 语法前端：Token 序列归约为 ClassifiedExpr |
| `compiler.h` | `Compiler` / `parse` / `finalize` / `__parseTokensLegacy` | 批量编译引擎：纯解析入库 + 后置 finalize |
| `interpreter.h` | `Interpreter` / `interpret` / `__reduceStack` | 流式执行引擎：字符泵逐 token 即时执行 |
| `executor.h` | `Executor` / `execute` | 批量后置执行：按 actionOrder 触发回调与 action 模板 |
| `validation.h` | `validate` / `checkEnvSpecs` / `checkRequired` / `checkValues` | 校验引擎：env spec → 必选 → 值校验 |

grep 锚点（2026-10-25 核对）：`grep -n "class Scanner" include/cmd_args/expression/scanner.h`；`grep -n "enum class LexMode_E" include/cmd_args/expression/scanner.h`；`grep -n "__applyOne" include/cmd_args/expression/compiler.h`；`grep -n "class ExpressionRecognizer" include/cmd_args/expression/expression_recognizer.h`。

## 决策

- 两级表示（`Expression` vs `ClassifiedExpr`）：持久实体与瞬态中间表示分离，避免 `Model` 依赖识别器的全部字段；代价是字段并集膨胀、两套表示概念重叠。
- 批量路径「类型化 Token → `toText()` → 既有字符串管线」：复用既有 Parser 规则并避免逐 token 归约开销；代价是 `__applyOne` 分派成为未接线代码（`compiler.h:232` 无调用点），`ClassifiedExpr` 在批量路径上实际不参与。
- 流式采用 `Scanner::step()` 字符泵而非行级 `Lexer`：支持增量与「识别即执行」；代价是 `'\0'` flush、`'='` 特判（`scanner.h:109-130`）与 `__lineReset` 手工复位。
- `Interpreter` 私有继承三 Parser 而不组合/继承钩子：可直接复用其数据指针并即时执行；代价是与 `OptionParser::__assign` 逻辑重复，内置回调需在 `__executeOptionImmediate` 复刻。
- 校验抽到 `validation.h` 统一入口（先 env spec、后 required、再 values）：批量 `Compiler::finalize` 与流式 `Interpreter::__finalize` 共用；代价是 `ValidationError_E` 需在宿主层（ContextBase）再包装/转换。
- 关系语法两条路径分别实现：`Compiler` 走 `OptionGraph`（`addEdge` + `evaluate`），`Interpreter` 走 `__handleRelation` 即时判断；代价是两套语义需人工对齐。
- 词法承载 vendored `magic_enum` 做关键字枚举转换（`Scanner::__processVariable` 中 `enum_cast<Token::KeyWord_E>`），避免手写字符串比较。

## 教训

- Scanner 点号拆分缺陷：`127.0.0.1` 一类值被拆成 Real + Delimiter + …，下游按 token 数取值会错位。规避：IPv4/IPv6 语义校验不要依赖 argv 词法保留原样（示例改用 hostname/整数，`example/validate/main.cpp:17-18`）；`README.md:324` 已列为已知限制。
- `Interpreter::getStackSize()` 恒为 0：`rec__` 只被构造、`feed()` 从未调用（`interpreter.h:63`、`157`、`534`），调试/测试不要把它当栈深观测；真实栈是 `vecStack__`。
- `Compiler::__applyOne` 是死代码：修改「ClassifiedExpr → Parser」分派不会影响批量 parse，批量行为在 `__parseTokensLegacy`（`compiler.h:224-229` 与 `232`）。
- 生产管线不经过 EnvLine：`ContextBase::parse` 走 `Compiler::__parseTokensLegacy` → `EnvParser::parse(envp)`，按键在首个 `'='` 处 `substr`（`env_parser.h:79-86`），空间级联只在 `Lexer::scanEnvLine` / `recognizeEnvAssign` 中生效（当前仅单测使用）；改分隔符不影响 parse 主路径。
- `'='` 处理两处耦合：`Scanner::step` 在 CommandLine 模式下遇 `'='` 结束后不重新 BEGIN（`scanner.h:118`），依赖 `Token::hasInlineEq()` 传递；改动会同时影响 `ExpressionRecognizer` 的 `bPendingInlineEq__` 与 `Interpreter` 的同名字段。
- LongOpt 挂起拒绝 `'-'` 开头值、ShortOpt/NamedArg 无条件接受（`expression_recognizer.h:180-210`、`interpreter.h:232-255`）：`--opt -5` 报 `NEED_VALUE`，而 `-o -5` 接受；测试需分别覆盖。
- `ExpressionRecognizer` 的 HeadStash 快速路径：缓冲第二个 token 是 Variable/Integer/String/Real 时直接 emit aggregate（`expression_recognizer.h:307-323`）→ `-a b` 不会等待关系尾；关系语法必须紧邻出现。
- `Interpreter::__finalize` 与 `Compiler::finalize` 各有一份 seed/expand/save-config 逻辑（`compiler.h:148-164`、`interpreter.h:513-529`），改一处忘另一处会导致两条路径行为分叉。
