# test_parser 补测 + Interpreter「游标 + token 栈」实时解析重构 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use subagent-driven-development (recommended) or executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 新建 `dfx/unit_test/test_parser.cpp` 补齐「token → Expression」批量解析流程的测试空白（OptionParser / ArgumentParser / EnvParser 三件套）；同时把 `Interpreter` 的实时解析重构为显式状态机——以成员 `idxCursor__` 作输入流游标自入口逐字符泵入 Scanner，识别出的 token 压入 token 栈 `vecStack__`，栈内内容凑满一个 Expression 时立即出栈执行，行为与现有实现逐字节一致。

**Architecture:** Task 1 是纯增量测试（库代码零改动），白盒直接构造 `Model + 三 Parser`，断言 token 流被消费后以正确的 Expression 派生类型入库（Option/Argument/Env）。Task 2 把 `interpret()` 内联 lambda 泵提升为类私有状态机：新增私有成员 `idxCursor__ / vecStack__ / strPendingKey__ / enPendingKind__ / bPendingInlineEq__ / bOptEnd__ / vecPos__ / vecDashLeft__`，拆出 `__lineReset / __feedChar / __reduceStack` 三个私有方法，归约规则逐条对齐现有语义；另加两个只读观测 getter `getCursor()/getStackSize()`，让新测试能在 `onSet` 回调触发的瞬间断言游标位置与栈已排干，从而证明「凑满一个 Expression 就立即执行」的时点语义。

**Tech Stack:** C++20（header-only）、xmake、Catch2（dfx/unit_test，vendored 于 dfx/internal）

**Spec:** 用户需求原文——"项目缺少test_parser,需要对把token变成Expression的流程进行测试, 同时，在Interpreter进行实时解析时，应该用一个idxCursor指向输入流的入口，进行token识别后压入栈中，栈vector满足一个Expression那就执行。"

已确认的设计决策：
1. `test_parser.cpp` 覆盖三个 Parser 的 **token→Expression 批量流程**（对应 Compiler 模式管线），直接构造 `Model` + 各 Parser 白盒测试；Expression 类层次本身已有 `test_expression.cpp`，不重复覆盖。
2. Interpreter 重构为「游标 + 栈」：`idxCursor__` 指向输入流入口（下标 0），每识别出一个完整 token 即压入 `vecStack__`，随后 `__reduceStack()` 检查栈——凑满一个 Expression 立即出栈执行。
3. **挂起取值保留为辅助状态**（`strPendingKey__ / enPendingKind__ / bPendingInlineEq__`）：聚合短选项串内的挂起（如 `-bt` 的 t 在等值）无法用整 token 栈表达，这是「栈满足 Expression」判定规则的补充而非违背；长选项挂起时头部 token 出栈、名字转入挂起键，与现行为等价。
4. 方位参数仍是「逐个识别、流空后统一数量校验按序提交」——单个方位 token 是完整 Expression 但其提交天然延后，与批量 `ArgumentParser::parse` 数量校验语义对齐（现状即如此）。
5. 新增只读观测 getter `getCursor()` / `getStackSize()`（公开、noexcept、无 setter）：测试经 `onSet` 回调在解析中途快照二者，验证「凑满即执行」时点；同时服务 REPL 调试。
6. **不加公共逐字符喂入 API**（feed/step 不引入）：`interpret(line)` 单一公共入口不变，`ContextRuntime` 等调用方零改动。
7. 函数局部 `PendingKind_E` 提升为类私有嵌套枚举（成员化状态所需），枚举名与值不变。
8. 兼容铁律：公开 API 零变化；`example/**` 零改动继续编译；既有测试文件除追加段落外零逻辑改动，全部必须原样通过（回归安全网）。

## Global Constraints

- 语言标准 C++20（`xmake.lua: set_languages("c++20")`）；MSVC 下 `/utf-8` 已全局强制。
- 警告即错误（`set_warnings("all", "error")`）：所有新代码必须零警告。
- 库为 header-only：库代码一律改 `include/cmd_args/` 下头文件，不新建 .cpp；测试才用 .cpp（放 `dfx/unit_test/`）。
- **成员命名**：私有成员＝小写类型前缀+`__` 后缀（bool 用 `b`、指针用 `p`、枚举用 `en`、下标用 `idx`、串用 `str`、向量用 `vec`）；形参 `_` 前缀+PascalCase；私有方法 `__` 前缀+小写动词+PascalCase。
- 方法声明：返回类型非 4 字符一律 `auto ... -> T` 后置；`void`/`bool` 经典前置；限定符顺序 参数→const→noexcept→后置返回。
- 注释 Doxygen 全中文；命名空间只用宏 `CMD_ARGS_NAMESPACE_BEGIN/END`；Apache 头保持不动。
- 排除区：`dfx/internal/**` 为 vendored 第三方（FakeIt/catch2/tlp），不得触碰。
- 构建与运行（工作目录＝仓库根 `H:\dsh_space\cmd_args`）：
  - 构建：`xmake build unit_test`
  - 按标签过滤：`xmake run unit_test "[parser]"`（**引号必须带**，pwsh 下方括号是通配符）
  - 全量回归：`xmake run unit_test`
- 提交信息用英文 conventional commits（仓库惯例：`test(scope): ...` / `refactor(scope): ...`）。

---

## 文件结构总览

```
dfx/unit_test/
├── test_parser.cpp        [新] 三 Parser 的 token→Expression 批量流程测试（标签 [parser]）
├── test_interpreter.cpp   [改] 末尾追加「实时解析状态机」测试段（既有用例零改动）
└── main.cpp               [改] 头部文件清单注释补 test_parser.cpp 一行
include/cmd_args/context/
└── interpreter.h          [改] interpret 重构为 idxCursor+token 栈归约状态机（含类文档注释更新）
include/cmd_args/context/
└── AGENT.md               [改] 流式模式说明与内部状态名同步
docs/superpowers/plans/
└── 2026-08-25-parser-tests-interpreter-cursor-stack.md   [本计划]
```

依赖方向：Task 1 与 Task 2 相互独立（可并行/换序）；Task 2 内部严格 TDD——先加依赖新 getter 的测试（编译红）再动 interpreter.h。

---

### Task 1: 新建 test_parser.cpp —— token → Expression 批量解析流程测试

**Files:**
- Create: `dfx/unit_test/test_parser.cpp`
- Modify: `dfx/unit_test/main.cpp`（仅注释清单）

**Interfaces:**
- Consumes: `Model{args,opts,envs}` 公开成员；`OptionBuilder::addOption<T>(long, short, default) / addFlag(short, long, help) / addShortCircuit(short, long, help, fn)`；`ArgumentBuilder::build<ArgumentNamed>(name, desc) / build<ArgumentPosition>(name, desc)`；`OptionParser(OptionData*).parse(Tokens_T&) -> bool`、`isFired()`、`isOptionsEnded()`、`isParsed(NameLong_T const&)`、`isParsed(NameShort_T)`；`ArgumentParser(ArgumentData*).parse(Tokens_T&)`、`parsePositionalOnly(Tokens_T&)`、`parse(std::string const&)`、`isParsed(NameLong_T const&)`；`EnvParser(EnvData*).parse(Envs_T const&)`、`has/get/find/size/referencedCount/expand`；`Tokens_T = std::list<std::string>`、`Envs_T = std::vector<std::string>`；异常类型 `ErrorParse`。
- Produces: 无（纯测试任务）；后续若 Parser 行为变更，`[parser]` 标签是其回归安全网。

- [ ] **Step 1: 基线确认——当前全量测试绿**

Run: `xmake build unit_test` 然后 `xmake run unit_test`
Expected: 编译零警告，全部用例 PASS（记录用例总数；后续步骤以此为回归基线）

- [ ] **Step 2: 写 test_parser.cpp（全部用例一次写入）**

创建 `dfx/unit_test/test_parser.cpp`，内容如下：

```cpp
#include "catch2/catch.hpp"

#include <list>
#include <string>
#include <vector>

#include "cmd_args/cmd_args.hpp"

using namespace cmd_args;

/* ==================== OptionParser：token → Option(Expression) ==================== */

TEST_CASE("parser: 长选项双形态 + 短选项三形态入库为 Option Expression", "[parser]")
{
    Model model;
    OptionBuilder ob(&model.opts);
    ob.addFlag('a', "ena", "flag a");
    ob.addFlag('b', "enb", "flag b");
    ob.addOption<int>("timeout", "t", 30);
    OptionParser op(&model.opts);

    // --long=value 内联形态
    Tokens_T t1{ "--timeout=9" };
    REQUIRE_FALSE(op.parse(t1));
    auto *p = model.opts.mgrValues.find("timeout");
    REQUIRE(p != nullptr);
    REQUIRE(p->kind() == ExpressionKind_E::Option);   // 入库即携带 Expression 视角
    REQUIRE(p->strValue == "9");
    REQUIRE(p->bExplicit__);
    REQUIRE(op.isParsed("timeout"));
    REQUIRE(t1.empty());                              // 已消费项移出 token 流

    // --long value 分离形态（消费下一个 token 作值）
    model.opts.forEachValue([](OptionValue &o) { o.reset(); });
    Tokens_T t2{ "--timeout", "42" };
    REQUIRE_FALSE(op.parse(t2));
    REQUIRE(model.opts.mgrValues.find("timeout")->strValue == "42");
    REQUIRE(t2.empty());

    // -x value 短名分离形态
    model.opts.forEachValue([](OptionValue &o) { o.reset(); });
    Tokens_T t3{ "-t", "7" };
    REQUIRE_FALSE(op.parse(t3));
    REQUIRE(model.opts.mgrValues.find("timeout")->strValue == "7");
    REQUIRE(op.isParsed('t'));

    // -x=value 短名内联形态（聚合路径吃 '=' 余串）
    model.opts.forEachValue([](OptionValue &o) { o.reset(); });
    Tokens_T t4{ "-t=5" };
    REQUIRE_FALSE(op.parse(t4));
    REQUIRE(model.opts.mgrValues.find("timeout")->strValue == "5");

    // 聚合 bool：-ab 全部置位
    Tokens_T t5{ "-ab" };
    REQUIRE_FALSE(op.parse(t5));
    REQUIRE(model.opts.mgrValues.find("ena")->strValue == "1");
    REQUIRE(model.opts.mgrValues.find("enb")->strValue == "1");
}

TEST_CASE("parser: 缺值报 NEED_VALUE（ErrorParse 异常通道）", "[parser]")
{
    Model model;
    OptionBuilder ob(&model.opts);
    ob.addOption<int>("timeout", "t", 30);
    OptionParser op(&model.opts);

    Tokens_T t1{ "--timeout" };              // 流尾缺值
    REQUIRE_THROWS_AS(op.parse(t1), ErrorParse);

    Tokens_T t2{ "--timeout", "--other" };   // 下一个是 dash 开头 → 拒绝作值
    REQUIRE_THROWS_AS(op.parse(t2), ErrorParse);

    Tokens_T t3{ "-t" };                     // 短名流尾缺值
    REQUIRE_THROWS_AS(op.parse(t3), ErrorParse);
}

TEST_CASE("parser: 聚合串未知短选项当场报 UNKNOWN_OPTION", "[parser]")
{
    Model model;
    OptionBuilder ob(&model.opts);
    ob.addFlag('a', "ena", "flag a");
    OptionParser op(&model.opts);

    Tokens_T toks{ "-az" };                  // z 未注册
    REQUIRE_THROWS_AS(op.parse(toks), ErrorParse);
}

TEST_CASE("parser: 短回路命中立即回调并中止后续解析", "[parser]")
{
    Model model;
    OptionBuilder ob(&model.opts);
    ob.addOption<int>("timeout", "t", 30);
    bool fired = false;
    ob.addShortCircuit('h', "help", "show help", [&fired] { fired = true; });
    OptionParser op(&model.opts);

    Tokens_T toks{ "--help", "--timeout", "9" };
    REQUIRE(op.parse(toks));                 // 返回值 = 是否命中短回路
    REQUIRE(fired);
    REQUIRE(op.isFired());
    // 命中即停止：后续 token 原样保留、未发生赋值
    REQUIRE(toks == Tokens_T{ "--timeout", "9" });
    REQUIRE(model.opts.mgrValues.find("timeout")->strValue == "30");
}

TEST_CASE("parser: -- 终止选项区且其后 token 原样保留", "[parser]")
{
    Model model;
    OptionBuilder ob(&model.opts);
    ob.addOption<int>("timeout", "t", 30);
    OptionParser op(&model.opts);

    Tokens_T toks{ "--", "-x", "plain" };
    REQUIRE_FALSE(op.parse(toks));
    REQUIRE(op.isOptionsEnded());
    REQUIRE(toks == Tokens_T{ "-x", "plain" });   // '--' 自身被移除，其余不动
}

/* ==================== ArgumentParser：token → Argument(Expression) ==================== */

TEST_CASE("parser: 命名参数 + 方位参数一次解析入库", "[parser]")
{
    Model model;
    ArgumentBuilder ab(&model.args);
    ab.build<ArgumentNamed>("hello", "h");
    ab.build<ArgumentPosition>("dest", "target dir");
    ArgumentParser ap(&model.args);

    Tokens_T toks{ "--hello", "world", "dest" };
    ap.parse(toks);

    REQUIRE(ap.isParsed("hello"));
    REQUIRE(model.args.mgrName.doGet("hello")->getValue() == "world");
    REQUIRE(model.args.mgrPos.mutGetBySeq(0)->getValue() == "dest");
    REQUIRE(toks.empty());
}

TEST_CASE("parser: 方位参数数量越界抛 UNKNWON_ARGS", "[parser]")
{
    Model model;
    ArgumentBuilder ab(&model.args);
    ab.build<ArgumentPosition>("only", "the only positional");
    ArgumentParser ap(&model.args);

    Tokens_T toks{ "a", "b" };               // 1 个槽位收到 2 个 token
    REQUIRE_THROWS_AS(ap.parse(toks), ErrorParse);
}

TEST_CASE("parser: parsePositionalOnly 把 dash token 也当方位参数", "[parser]")
{
    Model model;
    ArgumentBuilder ab(&model.args);
    ab.build<ArgumentPosition>("weird", "dash-like value");
    ArgumentParser ap(&model.args);

    Tokens_T toks{ "-weird" };
    ap.parsePositionalOnly(toks);
    REQUIRE(model.args.mgrPos.mutGetBySeq(0)->getValue() == "-weird");
    REQUIRE(toks.empty());
}

TEST_CASE("parser: KEY=VALUE 行形态直接赋值命名参数", "[parser]")
{
    Model model;
    ArgumentBuilder ab(&model.args);
    ab.build<ArgumentNamed>("hello", "h");
    ArgumentParser ap(&model.args);

    ap.parse("hello=world");
    REQUIRE(model.args.mgrName.doGet("hello")->getValue() == "world");

    // 未注册键报错
    REQUIRE_THROWS_AS(ap.parse("nope=x"), ErrorParse);
}

/* ==================== EnvParser："KEY=VALUE" 串 → Env(Expression) ==================== */

TEST_CASE("parser: 环境串入库为 Env Expression 且 get/expand 标记引用", "[parser]")
{
    Model model;
    EnvParser ep(&model.envs);

    ep.parse(Envs_T{ "UT_ALPHA=1", "broken-line" });   // 无 '=' 的行忽略

    REQUIRE(ep.size() == 1);
    REQUIRE(ep.has("UT_ALPHA"));
    auto const *p = ep.find("UT_ALPHA");
    REQUIRE(p != nullptr);
    REQUIRE(p->kind() == ExpressionKind_E::Env);       // 入库即携带 Expression 视角
    REQUIRE(p->getName() == "UT_ALPHA");

    REQUIRE(ep.get("UT_ALPHA") == "1");
    REQUIRE(ep.referencedCount() == 1);                // get 标记“被引用”
    REQUIRE(ep.get("UT_MISSING").empty());             // 缺失返回空串

    REQUIRE(ep.expand("${UT_ALPHA}/log") == "1/log");  // 展开命中同一存储
}
```

- [ ] **Step 3: 同步 main.cpp 头部文件清单注释**

修改 `dfx/unit_test/main.cpp` 第 4-12 行的注释块，在 `test_lexer.cpp` 行之后插入一行（保持对齐）：

```cpp
/*
    unit_test 入口：仅提供 Catch2 的 main（CATCH_CONFIG_MAIN）。
    测试用例按库子模块拆分在同目录的 test_*.cpp 中：
      test_lexer.cpp       词法（Lexer / Scanner 流式）
      test_parser.cpp      三解析器 token→Expression 批量流程
      test_builder.cpp     构筑重名检查
      test_context.cpp     Context 解析 / 错误码 / 短回路 / loop / 容器化
      test_env.cpp         环境变量三件套 + Context 层联动
      test_config.cpp      YAML 配置 / schema / 往返 / load-save e2e
      test_option.cpp      选项 action（system_call）
      test_interpreter.cpp 流式解释引擎
      test_expression.cpp  Expression 类层次
      test_model.cpp       Model 查询 + Executor 批量执行
*/
```

- [ ] **Step 4: 编译并跑新标签**

Run: `xmake build unit_test` 然后 `xmake run unit_test "[parser]"`
Expected: 编译零警告；9 个用例全 PASS。
若有用例 FAIL：**不要改库代码来迁就测试**——本任务是给既有行为建档；先用 systematic-debugging 对照 option_parser.h / argument_parser.h / env_parser.h 源码核实语义，只有源码证实测试期望写错时才修测试期望。

- [ ] **Step 5: 全量回归**

Run: `xmake run unit_test`
Expected: 全部 PASS，用例数 = Step 1 基线 + 9。

- [ ] **Step 6: 提交**

```bash
git add dfx/unit_test/test_parser.cpp dfx/unit_test/main.cpp
git commit -m "test(parser): cover token-to-Expression flow of three parsers"
```

---

### Task 2: Interpreter 重构 —— idxCursor 输入流游标 + token 栈归约

**Files:**
- Modify: `include/cmd_args/context/interpreter.h`（类文档注释、interpret、新增观测 getter、新增私有方法、成员区）
- Test: `dfx/unit_test/test_interpreter.cpp`（末尾追加一段，既有用例零改动）
- Modify: `include/cmd_args/context/AGENT.md`（两处文档同步）

**Interfaces:**
- Consumes: `detail::Scanner::step(char) -> std::optional<Token>`（token 完成时返回）；`Token::toText()/hasInlineEq()`；既有私有 `__executeOptionImmediate / __assignPositional / __finalize`（签名不变，原样复用）。
- Produces:
  - 公开只读观测：`auto Interpreter::getCursor() const noexcept -> size_t`（当前输入流游标，整行解释完等于行长）；`auto Interpreter::getStackSize() const noexcept -> size_t`（当前 token 栈深，整行解释完为 0）。
  - 公开 API 其余零变化：`interpret(std::string const&, Errors&)`、`finalize()`、`setExitCode/exitCode/markParsed/prepared/describeError/once/loop` 签名不变，`ContextRuntime` 零改动。
  - 类内私有嵌套枚举 `PendingKind_E : char { None, LongOpt, ShortOpt, NamedArg }`（从函数局部提升）。

- [ ] **Step 1: 先写失败测试（引用尚不存在的观测 getter）**

在 `dfx/unit_test/test_interpreter.cpp` 文件末尾追加：

```cpp
/* ==================== 实时解析状态机：idxCursor 游标 + token 栈归约 ==================== */

TEST_CASE("interpreter: 实时解析凑满一个 Expression 立即执行且时点可观测", "[interpreter]")
{
    Model model;
    Errors errors;
    Interpreter interp(&model);
    OptionBuilder ob(&model.opts);

    std::vector<size_t> cursors;   // onSet 触发瞬间的游标快照
    std::vector<size_t> stacks;    // onSet 触发瞬间的栈深快照
    ob.addOption<std::string>("step", "s", "")
        ->onSet([&](std::string const &) {
            cursors.emplace_back(interp.getCursor());
            stacks.emplace_back(interp.getStackSize());
        });

    // "--step one xx"：'one' 在下标 10 的空格处终结成 token，
    // 与挂起的 "step" 凑满一个 Expression → 此刻立即执行：
    // 游标停在 10（该空格尚未越过），值 token 已随归约出栈（栈深 0）
    auto ec = interp.interpret("--step one xx", errors);
    REQUIRE(!ec);

    REQUIRE(cursors.size() == 1);
    REQUIRE(cursors[0] == 10);
    REQUIRE(stacks[0] == 0);
    REQUIRE(model.opts.mgrValues.find("step")->strValue == "one");

    // 整行解释完：游标抵达流尾、栈已排干
    REQUIRE(interp.getCursor() == std::string("--step one xx").size());
    REQUIRE(interp.getStackSize() == 0);
}

TEST_CASE("interpreter: 多个 Expression 按流序逐个即时执行", "[interpreter]")
{
    Model model;
    Errors errors;
    Interpreter interp(&model);
    OptionBuilder ob(&model.opts);

    std::vector<std::string> trace;
    ob.addOption<std::string>("alpha", "a", "")
        ->onSet([&](std::string const &_V) { trace.emplace_back("alpha=" + _V); });
    ob.addOption<std::string>("beta", "b", "")
        ->onSet([&](std::string const &_V) { trace.emplace_back("beta=" + _V); });

    auto ec = interp.interpret("--beta 2 --alpha 1", errors);
    REQUIRE(!ec);
    // 栈每凑满一个 Expression 就地执行一个：顺序 = 流序，而非行尾统一提交
    REQUIRE(trace == std::vector<std::string>{ "beta=2", "alpha=1" });
    REQUIRE(interp.getCursor() == std::string("--beta 2 --alpha 1").size());
    REQUIRE(interp.getStackSize() == 0);
}

TEST_CASE("interpreter: 长选项挂起遇 dash 值当场报 NEED_VALUE 且不吞后续 token", "[interpreter]")
{
    Model model;
    Errors errors;
    Interpreter interp(&model);
    OptionBuilder ob(&model.opts);

    std::vector<std::string> trace;
    ob.addOption<std::string>("alpha", "a", "");
    ob.addOption<std::string>("mark", "m", "")
        ->onSet([&](std::string const &_V) { trace.emplace_back("mark=" + _V); });

    // "--alpha" 挂起等值，下一个 token "--mark" 以 dash 开头 → 归约当场抛 NEED_VALUE，
    // "v" 根本不会被处理（mark 回调不触发）
    auto ec = interp.interpret("--alpha --mark v", errors);
    REQUIRE(ec == make_error_code(ParseError_E::NEED_VALUE));
    REQUIRE(trace.empty());
}
```

- [ ] **Step 2: 确认 RED**

Run: `xmake build unit_test`
Expected: **编译失败**，错误指向 `getCursor` / `getStackSize` 不是 `Interpreter` 的成员（这正是 TDD 的红灯：观测接口尚不存在）。

- [ ] **Step 3: 重构 interpreter.h —— 五个区域依次修改**

**Region A — 类文档注释（第 40-50 行）整体替换为：**

```cpp
/*
    Interpreter（流式实时引擎）：把一行命令行当做字符流，
    以成员 idxCursor__ 作输入流游标自入口（下标 0）逐字符泵入 Scanner；
    每识别出一个完整 Token 即压入 token 栈 vecStack__，
    栈内内容凑满一个 Expression（Option / Argument / 方位参数）时立刻出栈执行——
    选项赋值即刻触发 fnOnSet__ 与 action 模板，命名参数即刻 set，
    方位参数按序缓冲至流空后统一提交（数量校验与批量一致）。直到 Stream 为空。

    与批量 Compiler 的区别：
    - Compiler：先纯解析全部 Token 入库 Model，Executor 按 actionOrder 后置统一执行。
    - Interpreter：解析即执行，无需 Executor / actionOrder。
*/
```

**Region B — `interpret` 成员函数（第 63-229 行）整体替换为：**

```cpp
    /* ===== 逐行解释 ===== */

    /// @brief 解释执行一行：reset → idxCursor 逐字符泵入 Scanner → token 压栈 → 凑满一个 Expression 立即执行 → 方位参数提交 → finalize
    std::error_code interpret(std::string const &_Line, Errors &_Errors)
    {
        pModel__->args.mgrName.doMutVisit([](Argument &a) { a.reset(); });
        pModel__->args.mgrPos.doMutVisit([](Argument &a) { a.reset(); });
        pModel__->opts.mgrValues.forEachValue([](OptionValue &o) { o.reset(); });
        _Errors.clearLast();

        try {
            cmd_args::detail::Scanner sc({});
            __lineReset();                                       // 游标归零指向输入流入口，栈与挂起状态清空

            while (idxCursor__ < _Line.size()) {
                __feedChar(sc, _Line[idxCursor__]);              // 逐字符识别：完整 token 压栈并尝试归约
                ++idxCursor__;
            }
            __feedChar(sc, '\0');                                // 流结束：flush 最后一个 token

            // 流空后仍有挂起取值（--opt / --name 后无值）：与批量路径一致报 NEED_VALUE
            if (!strPendingKey__.empty()) {
                if (enPendingKind__ == PendingKind_E::NamedArg) {
                    throw ErrorParse(ParseError_E::NEED_VALUE,
                        "named argument '--" + strPendingKey__ + "' need a value");
                }
                if (enPendingKind__ == PendingKind_E::LongOpt) {
                    throw ErrorParse(ParseError_E::NEED_VALUE,
                        "option --" + strPendingKey__ + " need a value");
                }
                throw ErrorParse(ParseError_E::NEED_VALUE,
                    "option -" + strPendingKey__ + " need a value");
            }

            // 未识别 dash token：先于方位参数提交报 UNKNOWN_OPTION（UNKNOWN_OPTION 优先，
            // 且绝不占用方位槽位、不产生赋值副作用——批量语义中 dash token 不进方位缓冲）。
            if (!vecDashLeft__.empty()) {
                throw ErrorParse(ParseError_E::UNKNOWN_OPTION,
                    "unrecognized option or argument: " + vecDashLeft__.front());
            }

            __assignPositional(vecPos__);                        // 数量校验 + 按序提交

            __finalize();
            bParsed__ = true;
            return make_error_code(ParseError_E::OK);
        }
        catch (ErrorParse const &e) { _Errors.setLast(e.code()); return e.code(); }
        catch (ErrorLexer const &e) { _Errors.setLast(e.code()); return e.code(); }
        catch (std::exception const &ex) {
            _Errors.setMsg(ex.what());
            return make_error_code(ParseError_E::UNKNWON_ARGS);
        }
    }
```

**Region C — 在 `describeError(...)` 之后插入观测 getter（公开段）：**

```cpp
    /* ===== 实时解析观测 ===== */

    /// @brief 当前输入流游标（指向下一个待识别字符；行首为 0，整行解释完等于行长）
    auto getCursor() const noexcept -> size_t { return idxCursor__; }

    /// @brief 当前 token 栈深（已识别但尚未凑满 Expression 的 token 数；整行解释完应为 0）
    auto getStackSize() const noexcept -> size_t { return vecStack__.size(); }
```

**Region D — 在私有段开头（`__executeOptionImmediate` 之前）插入流式状态机方法组：**

```cpp
    // ===== 流式解析状态机 =====

    /// @brief 行首复位：游标归零指向输入流入口，token 栈与挂起状态全部清空
    void __lineReset()
    {
        idxCursor__        = 0;
        vecStack__.clear();
        strPendingKey__.clear();
        enPendingKind__    = PendingKind_E::None;
        bPendingInlineEq__ = false;
        bOptEnd__          = false;
        vecPos__.clear();
        vecDashLeft__.clear();
    }

    /// @brief 泵入单字符：Scanner 识别出完整 token 即压入 token 栈，随后尝试归约执行
    void __feedChar(detail::Scanner &_Sc, char _C)
    {
        auto tokOpt = _Sc.step(_C);
        if (!tokOpt) return;
        vecStack__.emplace_back(std::move(*tokOpt));  ///< 识别即压栈
        __reduceStack();                              ///< 栈凑满一个 Expression 就立即执行
    }

    /**
     * @brief 归约：检查 token 栈顶，凑满一个 Expression 即出栈立即执行
     * @note 归约规则（与批量 Parser 语义逐条对齐）：
     *       1. 有挂起取值方：栈顶 token 就是值 → 组合立即执行并清除挂起；
     *       2. '--' 之后一律方位参数；'--' 本身切换纯方位模式；
     *       3. --name[=v]：短回路/bool 即刻完成；非 bool 等下个 token 凑满（头出栈转挂起键）；
     *       4. -x / -abc 聚合：bool 位即刻完成；值选项吃余串或挂起等下个 token；
     *       5. 普通 token 入方位缓冲（流空后统一校验数量再按序提交）。
     */
    void __reduceStack()
    {
        auto tok = std::move(vecStack__.back());
        vecStack__.pop_back();
        std::string text      = tok.toText();
        bool        bInlineEq = tok.hasInlineEq();

        // ── 1. 挂起取值：当前 token 就是值 ──
        if (!strPendingKey__.empty()) {
            // 消费规则（批量语义）：
            // - LongOpt：拒绝 dash 开头值，除非挂起方带内联 '='（--opt=-5）
            // - ShortOpt：无条件接受下一个 token（option_parser.h 短路径无 dash 检查）
            // - NamedArg：无条件接受下一个 token（ArgumentParser::parseNamedOnly 语义）
            if (enPendingKind__ == PendingKind_E::LongOpt && !bPendingInlineEq__
                && util::starts_with(text, "-")) {
                throw ErrorParse(ParseError_E::NEED_VALUE,
                    "option --" + strPendingKey__ + " need a value");
            }
            if (enPendingKind__ == PendingKind_E::NamedArg) {
                if (auto *pArg = pModel__->args.mgrName.mutGet(strPendingKey__)) {
                    pArg->set(text);
                }
            }
            else if (enPendingKind__ == PendingKind_E::ShortOpt) {
                // 短名按 char 查（mutFind(std::string) 只按长名匹配）
                if (auto *pOpt = pModel__->opts.mgrValues.mutFind(strPendingKey__.front())) {
                    __executeOptionImmediate(pOpt, text, pOpt->strType == "bool");
                }
            }
            else if (auto *pOpt = pModel__->opts.mgrValues.mutFind(strPendingKey__)) {
                __executeOptionImmediate(pOpt, text, pOpt->strType == "bool");
            }
            strPendingKey__.clear();
            enPendingKind__    = PendingKind_E::None;
            bPendingInlineEq__ = false;
            return;
        }

        // ── 2. '--' 之后全部方位参数 ──
        if (bOptEnd__) { vecPos__.emplace_back(text); return; }

        if (text == "--") { bOptEnd__ = true; return; }

        // ── 3. 长选项 / 命名参数：--name[=value] ──
        if (util::starts_with(text, "--") && text.size() > 2) {
            auto        body  = text.substr(2);
            auto        eq    = body.find('=');
            std::string name  = body.substr(0, eq == std::string::npos ? body.size() : eq);
            std::string value = eq == std::string::npos ? "" : body.substr(eq + 1);

            if (auto const *pSc = pModel__->opts.mgrActions.doGet(name)) {
                static_cast<OptionAction const *>(pSc)->fnCallback();  // 短回路：立即回调
                return;
            }
            if (auto *pOpt = pModel__->opts.mgrValues.mutFind(name)) {
                // 无内联值：等待下一 token 取值。bInlineEq 表达「值在下个 token
                // 且允许 dash 开头」（--opt=value 被 Scanner 在 '=' 处切开）
                if (value.empty() && (bInlineEq || pOpt->strType != "bool")) {
                    strPendingKey__    = name;
                    enPendingKind__    = PendingKind_E::LongOpt;
                    bPendingInlineEq__ = bInlineEq;
                    return;
                }
                __executeOptionImmediate(pOpt, value, pOpt->strType == "bool");
                return;
            }
            if (auto *pArg = pModel__->args.mgrName.mutGet(name)) {
                if (value.empty()) {
                    strPendingKey__    = name;
                    enPendingKind__    = PendingKind_E::NamedArg;
                    bPendingInlineEq__ = false;
                    return;
                }
                pArg->set(std::move(value));  // 命名参数表达式：立即 set
                return;
            }
            vecDashLeft__.emplace_back(text);  // 未识别长选项：独立缓冲，流末统一报 UNKNOWN_OPTION
            return;
        }

        // ── 4. 短选项：-x / 聚合 -abc ──
        if (util::starts_with(text, "-") && text.size() >= 2) {
            for (size_t i = 1; i < text.size(); ++i) {
                char key = text[i];
                if (auto const *pSc = pModel__->opts.mgrActions.doGet(key)) {
                    static_cast<OptionAction const *>(pSc)->fnCallback();
                    continue;
                }
                auto *pOpt = pModel__->opts.mgrValues.mutFind(key);
                if (!pOpt) {
                    throw ErrorParse(ParseError_E::UNKNOWN_OPTION,
                        "unrecognized short option '" + std::string(1, key) + "' in " + text);
                }
                if (pOpt->strType == "bool") {
                    __executeOptionImmediate(pOpt, "", true);
                    continue;
                }
                std::string val = text.substr(i + 1);  // 值选项：余下即值（-tvalue / -t=value）
                if (!val.empty() && val.front() == '=') { val = val.substr(1); }
                if (val.empty()) {
                    // 值在下一个 token（含聚合串末尾，如 -bt）：批量短路径无条件接受
                    strPendingKey__.assign(1, key);
                    enPendingKind__    = PendingKind_E::ShortOpt;
                    bPendingInlineEq__ = false;
                    return;
                }
                __executeOptionImmediate(pOpt, val, false);
                return;                              // 聚合串中值选项即终结
            }
            return;
        }

        // ── 5. 普通 token → 方位参数 ──
        vecPos__.emplace_back(text);
    }
```

**Region E — 文件末尾私有成员区（原第 382-386 行的三成员）替换为：**

```cpp
private:
    // ===== 引擎常驻状态 =====
    Model *pModel__ = nullptr;
    bool   bParsed__   = false;
    int    nExitCode__ = 0;

    // ===== 流式解析状态（每次 interpret 经 __lineReset 复位） =====

    /// 挂起方类型：决定取值消费规则与 NEED_VALUE 文案（对齐批量语义）
    enum class PendingKind_E : char { None, LongOpt, ShortOpt, NamedArg };

    size_t                     idxCursor__        = 0;                    ///< 输入流游标：指向当前行下一待识别字符（入口为 0）
    std::vector<detail::Token> vecStack__;                                ///< token 栈：识别即压入，凑满一个 Expression 立即归约执行
    std::string                strPendingKey__    = {};                   ///< 等待取值的选项/命名参数名
    PendingKind_E              enPendingKind__    = PendingKind_E::None;  ///< 挂起取值方类型
    bool                       bPendingInlineEq__ = false;                ///< 挂起方是否由 '=' 内联分隔（仅 LongOpt 有意义）
    bool                       bOptEnd__          = false;                ///< '--' 之后（纯方位模式）
    std::vector<std::string>   vecPos__;                                  ///< 方位参数缓冲（流空后统一校验数量按序提交）
    std::vector<std::string>   vecDashLeft__;                             ///< 未识别 dash token 缓冲（流末统一报 UNKNOWN_OPTION）
```

- [ ] **Step 4: 确认 GREEN（新标签）**

Run: `xmake build unit_test` 然后 `xmake run unit_test "[interpreter]"`
Expected: 编译零警告；既有 8 个用例 + 新增 3 个用例全 PASS。
若既有用例 FAIL：重构引入了行为偏移，用 systematic-debugging 对比新旧 `__reduceStack` 分支顺序定位（分支顺序必须与本计划 Region D 一字不差——尤其「挂起消费 → bOptEnd → '--' → 长选项 → 短选项 → 方位」的先后）。

- [ ] **Step 5: 全量回归**

Run: `xmake run unit_test`
Expected: 全部 PASS（含 Task 1 的 `[parser]` 9 例）。这一步证明重构对外行为逐字节保持。

- [ ] **Step 6: 同步 AGENT.md 文档（两处）**

`include/cmd_args/context/AGENT.md`：

① 第 15 行文件职责表 interpreter.h 行改为：

```markdown
| `interpreter.h` | `Interpreter` | **流式引擎**：idxCursor 游标逐字符泵入 Scanner → token 压栈 → 凑满一个 Expression 立即归约执行 |
```

② 「流式模式（Interpreter::loop / interpret）」代码块（第 31-37 行）改为：

```markdown
## 流式模式（Interpreter::loop / interpret）
```
per-line: Interpreter.interpret(line)
  → reset → __lineReset（idxCursor 归零指向流入口，token 栈清空）
  → idxCursor 逐字符喂 Scanner.step，完整 Token 即压入 vecStack__
  → __reduceStack：栈凑满一个 Expression 立即出栈执行（短回路回调 / fnOnSet__+模板 / 命名参数 set / 方位缓冲）
  → 方位参数按序提交（数量校验同批量）→ finalize
```
```

③ 第 46 行命名约定行改为：

```markdown
- 引擎内部流式状态：idxCursor__ / vecStack__ / strPendingKey__ / enPendingKind__ / bPendingInlineEq__ / bOptEnd__ / vecPos__ / vecDashLeft__（interpreter.h）
```

- [ ] **Step 7: 提交**

```bash
git add include/cmd_args/context/interpreter.h include/cmd_args/context/AGENT.md dfx/unit_test/test_interpreter.cpp
git commit -m "refactor(interpreter): drive real-time parsing with idxCursor and token-stack reduction"
```

---

## 行为保持核对表（Task 2 验收时逐条对照）

| # | 场景 | 输入 | 必须保持的行为 | 已锚定的测试 |
|---|------|------|----------------|--------------|
| 1 | 长选项分离值 | `--hello a --timeout 9` | 与批量结果一致 | 既有「流式 interpret 与批量 parse 结果一致」 |
| 2 | 聚合 bool + 带值混合 | `-ab --step one` | 每个 Expression 完成即触发 onSet | 既有「流式逐 Token 立即执行 fnOnSet…」 |
| 3 | 内联等号 | `--step=x` / `-t=-5` | hasInlineEq 放行 dash 值 | 既有两例 |
| 4 | 未知长选项 | `--nope x` | UNKNOWN_OPTION 且不占方位槽位 | 既有「未识别选项报 UNKNOWN_OPTION…」 |
| 5 | 挂起遇 dash 值 | `--opt --x` | 当场 NEED_VALUE，不吞后续 | 新增 Task 2 第三例 |
| 6 | `--` 之后 | `-- -x y` | 全部入方位缓冲 | 既有 + Region D 分支 2 |
| 7 | 行间 reset | 连续两次 interpret | 上行残留（含成员状态）不泄漏 | 既有「行间 reset」+ `__lineReset` |
| 8 | 流尾缺值 | `--step` | NEED_VALUE（三类文案区分） | 既有 + Region B 尾部检查 |

## Self-Review 记录

1. **Spec 覆盖**：「缺少 test_parser / token→Expression 流程测试」→ Task 1 全覆盖（Option 6 例 + Argument 4 例 + Env 1 例，含 Expression kind 断言）；「idxCursor 指向输入流入口 / token 压栈 / 栈满足一个 Expression 就执行」→ Task 2 Region A/B/D/E（成员化游标与栈 + 归约方法 + 时点观测测试）。无遗漏。
2. **占位符扫描**：所有代码块均为完整可粘贴内容，无 TBD/TODO/“类似 Task N”。
3. **类型一致性**：`getCursor()/getStackSize()` 在 Step 1 测试与 Region C 定义一致；`PendingKind_E` 值域与旧函数局部枚举一致；`__feedChar(detail::Scanner&, char)` 与 Region B 调用一致；`vecPos__/vecDashLeft__` 类型 `std::vector<std::string>` 与 `__assignPositional(std::vector<std::string> const&)` 既有签名匹配。
