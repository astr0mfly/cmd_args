# 008 Interpreter 注入式重构：Context 去继承化

> 目标日期：2026（当前会话）

## 目标与成功标准
1. `Interpreter` **不再是 Context 的基类**（废除 NVI 钩子模式）。
2. `Interpreter` 改为**具体引擎类**，构造函数注入组合解析核心 `Parser&` + 错误栈 `Errors*`——其中 `Parser` 内部由 **ArgumentParser + OptionParser + EnvParser(+EnvBuilder)** 组合，各自把解析结果写入 Model 对应的 **ArgumentData / OptionData / EnvData**。
3. 逐行输入 → `Interpreter::interpret(line)`：还原 \*Data → `Lexer::tokensFromLine` → 调 `Parser::parse` 实时解析与执行 → 结果写入 \*Data。
4. `Context` 改为组合式：持有 `Model + Parser + Interpreter`，`once()/loop()/set_exit_code/reset` 门面委托。
5. 公开 API 与行为零回归（once/loop/reset/parse 门面不变）。

## 改动清单

### 1. interpreter.h → 具体引擎
- 构造：`Interpreter(Parser &_Parser, Errors *_Errors) noexcept`
- 公开方法：`setExitCode/exitCode`、`markParsed/prepared`、`interpret(line)->error_code`、`describeError(ec)->string`、`once()`、`loop()`
- interpret 内部：`m_pParser__->resetData()` → `m_pErrors__->clearLast()` → `try { m_pParser__->parse(Lexer::tokensFromLine(_Line), Envs_T{}); markParsed; OK } catch (...) setLast/return ec`
- once/loop 逻辑与现有 Context hook 实现完全一致

### 2. parser.h → 私有继承改成员组合 + 新增 resetData()
- 成员：`m_argParser__(m_pModel__->m_args__)` / `m_optParser__(m_pModel__->m_opts__)` / `m_envBuilder__(m_pModel__->m_envs__)` / `m_envParser__(m_pModel__->m_envs__)`
- 约 15 处基类限定调用改为成员调用（逻辑不变）
- 新增 `void resetData()`（还原 args/opts 默认值，供 Interpreter 每行调用）

### 3. context.h → 去基类改组合
- `class Context : private Errors`（删 `public Interpreter` + 4 个 protected 钩子）
- 成员声明顺序：`m_strPrjName__ → m_instData__ → m_parser__ → m_interp__`
- 构造 init：`m_parser__(&m_instData__)`、`m_interp__(m_parser__, static_cast<Errors *>(this))`
- `once()/loop()` 委托 `m_interp__`；`reset()` 复用 `m_parser__.resetData() + clearLast()`
- `__doParse` 改用持久 `m_parser__`；成功后 `m_interp__.markParsed()`

### 4. head.cpp → 新增注入模式测试
- `Model model; Parser parser(&model); Errors errors; Interpreter interp(parser, &errors);`
- 注册选项 → `interp.interpret("--timeout 9")` → 断言 prepared + 值写入
- 错误行断言 ec 非零 + 错误栈有内容

### 5. AGENT.md §3
- interpreter.h 描述更新为注入式引擎

## 核验
- `xmake clean && xmake build unit_test` → build ok
- `xmake run unit_test` → ≥207 断言 / 39 用例
- `xmake build once && xmake build config_demo` → build ok
