# cmd_args Interpreter 解释执行引擎（once/loop 下沉基类）

> 计划文档：2026-08-23 创建；批准与执行均在 2026-08-23 会话完成。
> 关联文件 `.dsh/AGENT.md`；实施结果见文末「实施结果」。

## 1. 目标

用户检视后提出：**Interpreter 类需要实现解释执行的功能**。

现状：`Interpreter` 是空壳（`once()` 只 `std::exit(0)`），
once/loop 的实际解释执行逻辑全部写在 `Context` 内，
"解释器"名不副实。

目标：把解释执行引擎下沉到 `Interpreter` 基类——
基类驱动 once/loop 流程（识别输入、逐行解释、错误展示、退出），
子类（Context）只提供四个钩子。

## 2. 设计决策（用户确认）

| 决策点 | 结论 |
|---|---|
| action 执行时机 | **保持解析期命中即执行**（上一轮已确认不推翻）；
  Interpreter 驱动"解释"流程，action 在解释（parse）过程中按 token 顺序 system_call |
| 引擎归属 | `Interpreter` 基类实现 `once()`/`loop()` 主体；
  `Context` 删除重复实现，仅提供钩子（公开 API `app.once()/loop()` 不变） |
| 钩子 | `__interpret(line)`（reset+分词+解析+动作执行）、
  `__describeError(ec)`（错误文本）、`__prepared()`、`__exitCode()` |

## 3. 实现

### 3.1 `context/interpreter.h`（重写）

- `once()`：`__prepared()` 为假 → 抛 `ErrorInterp(STOPPED)`；
  否则 `std::exit(__exitCode())`（原 Context::once 语义）；
- `loop()`：REPL 常驻——`trim` 空行跳过、`exit/quit/exit()` 退出、
  每行 `__interpret(cmd)`、错误经 `__describeError` 展示、提示符循环；
- 四个 protected 纯虚钩子（含完整中文文档）。

### 3.2 `context/context.h`

- 删除 `Context::once()`/`Context::loop()` 实现（继承基类版本，API 不变）；
- 新增四个钩子：
  - `__interpret`：`reset()` + `parse(ec, Lexer::tokensFromLine(line), {})`，
    action 随解析按序执行，行间互不影响（reset 语义与历史 loop 一致）；
  - `__describeError`：`"(parse error) " + ec.message() + "\n" + dump()`（输出格式不变）；
  - `__prepared` → `m_bParsed__`；`__exitCode` → `m_nExitCode__`。

## 4. 测试（+3 TEST_CASE）

1. `interpreter: once 未解释时抛 ErrorInterp(STOPPED)`；
2. `interpreter: loop 逐行解释执行，exit 退出、行间不合并`
   —— 重定向 std::cin/std::cout 喂两行 + exit，断言无错误输出、
   状态为末次输入（hello=b、timeout 回默认）；
3. `interpreter: loop 行错误展示后继续下一行` —— 首行错误展示、次行正常生效。

## 5. 验收

- [ ] unit_test debug + release 全绿（37 用例 / 约 192+ 断言）；
- [ ] `once`/`config_demo` 构建运行不受影响；
- [ ] AGENT.md 源码地图同步；本计划文档落 `.dsh/plans/`。

---

## 实施结果（2026-08-23，批准后执行）

**修改**
- `include/cmd_args/context/interpreter.h` —— 重写：once/loop 解释执行引擎 + 四钩子
- `include/cmd_args/context/context.h` —— 删除重复 once/loop；实现四个钩子
- `dfx/unit_test/head.cpp` —— +3 个 interpreter 用例（含 cin/cout 重定向的 REPL e2e）
- `.dsh/AGENT.md` —— §3 源码地图同步
- `.dsh/plans/2026-08-23-interpreter-execute-engine.md` —— 本计划文档

**验收勾选**

- [x] unit_test debug + release 全绿：**37 个测试用例 / 193 条断言**
- [x] `once` / `config_demo` 构建运行通过（行为不变，仅引擎位置变化）
- [x] AGENT.md 同步；计划文档落 `.dsh/plans/`；gitmoji 风格本地提交

**偏差记录**

- 无行为偏差：once/loop 的输出格式、reset 语义、退出码、STOPPED 行为与迁移前一致；
  `Context::once/loop` 返回类型由 `Context&` 变为基类的 `void`（noreturn 下不可观察，
  仓库内无链式用法，公开文档也未承诺返回值）。
