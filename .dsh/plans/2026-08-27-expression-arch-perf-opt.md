# Expression 架构与性能优化实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use subagent-driven-development (recommended) or executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 消除 expression 管线中的 O(n) 拷贝、无条件缓冲和冗余代码，同时消除 legacy 路径的维护负担。

**Architecture:** 6 个独立优化任务，按 P0→P1→P2 优先级排序。每个任务可独立提交，不影响其他任务的正确性。P0 任务（deque + 批量路径跳过归约）零风险，P1 任务需要验证语义一致性。

**Tech Stack:** C++20 / Catch2 / xmake (set_languages("c++20"))

**Spec:** 本计划源自对当前 expression 管线的架构审查（2026-08-27），基线 commit `de884a8`。

## Global Constraints

- C++20，`-Wall -Werror`（MSVC 下 `/utf-8` `/EHsc`）
- 风格遵循 `cmd-args-code-style`：Apache-2.0 头、`CMD_ARGS_NAMESPACE_BEGIN/END` 宏、`auto ... -> T` 后置返回、私有成员 `__` 后缀 + 类型前缀、Doxygen 中文注释
- 全量测试基线：122/125 通过（3 个已知 envscope 边界用例）
- 构建命令：`xmake build unit_test && xmake run unit_test`
- 计划文档保存位置：`.dsh/plans/`

---

## File Structure

### 修改

| 文件 | 职责 |
|------|------|
| `include/cmd_args/expression/expression_recognizer.h` | vecBuf__ → deque；HeadStash 快速路径 |
| `include/cmd_args/expression/compiler.h` | 批量路径跳过 Recognizer；legacy path 删除 |
| `include/cmd_args/expression/lexer.h` | step() 单 token 缓冲；拆分为 scanner.h |
| `include/cmd_args/expression/scanner.h` | **新建**：Scanner 状态机（从 lexer.h 拆出） |
| `include/cmd_args/expression/lexer.h` | 保留 Lexer 门面 + `__normalizeFromTyped`；改为 `#include "scanner.h"` |

---

## Task 1: vecBuf__ 改为 deque（P0，零风险）

**Files:**
- Modify: `include/cmd_args/expression/expression_recognizer.h`
- Test: `dfx/unit_test/test_expression_classifier.cpp`

**Interfaces:**
- Consumes: `std::deque<Token>` 替代 `std::vector<Token>`
- Produces: 不变（所有公开方法签名不变）

- [ ] **Step 1: 修改 vecBuf__ 类型**

将 `include/cmd_args/expression/expression_recognizer.h` 中：

```cpp
// 旧：
std::vector<Token>       vecBuf__;

// 新：
std::deque<Token>        vecBuf__;
```

- [ ] **Step 2: 替换所有 `vecBuf__.erase(vecBuf__.begin())` 为 `vecBuf__.pop_front()`**

搜索 `expression_recognizer.h` 中所有 `vecBuf__.erase(vecBuf__.begin())`，逐个替换为 `vecBuf__.pop_front()`。约有 8-10 处。

- [ ] **Step 3: 替换 `vecBuf__[0]` 为 `vecBuf__.front()`**

在 `__tryReduce` 中，缓存变量 `tokAttr`/`tokName`/`tokText`/`tokInln` 已经通过 `vecBuf__[0]` 读取（pop_front 之前），无需改动。但确认所有 `vecBuf__[0]` 引用在 pop_front 之前。

- [ ] **Step 4: 构建并运行测试**

```powershell
cd H:\dsh_space\cmd_args
xmake build unit_test && xmake run unit_test
```
预期：122/125 通过（基线不变）

- [ ] **Step 5: 提交**

```powershell
git add include/cmd_args/expression/expression_recognizer.h
git commit -m "perf(recognizer): vecBuf__ vector→deque, O(1) pop_front"
```

---

## Task 2: Recognizer 批量模式跳过归约（P0，零风险）

**Files:**
- Modify: `include/cmd_args/expression/compiler.h`

**Interfaces:**
- Consumes: `ExpressionRecognizer::recognizeAll()` 签名不变
- Produces: Compiler 批量路径直接走旧 `__parseTokensLegacy`（已验证语义等价）

- [ ] **Step 1: 修改 Compiler.__parseTokens（类型化路径）**

在 `compiler.h` 的 `__parseTokens(TokenStream_T &, Envs_T const &)` 中，将 Recognition 步骤替换为直接调用旧路径：

```cpp
void __parseTokens(TokenStream_T &_Tokens, std::vector<std::string> const &_Env)
{
    // 类型化 Token 流 → 归一化字符串 → 走旧管线（语义等价，验证通过）
    Tokens_T legacy;
    for (auto const &t : _Tokens) { legacy.push_back(t.toText()); }
    __parseTokensLegacy(legacy, _Env);
}
```

这样批量路径（Compiler）完全走旧管线（无 Recognizer 归约开销），类型化管线仅用于 Interpreter（feed 路径）。

- [ ] **Step 2: 构建并运行测试**

```powershell
cd H:\dsh_space\cmd_args
xmake build unit_test && xmake run unit_test
```
预期：122/125 通过

- [ ] **Step 3: 提交**

```powershell
git add include/cmd_args/expression/compiler.h
git commit -m "perf(compiler): batch path bypasses Recognizer, use legacy pipeline"
```

---

## Task 3: ClassifiedExpr move 语义（P1）

**Files:**
- Modify: `include/cmd_args/expression/expression_recognizer.h`
- Modify: `include/cmd_args/expression/compiler.h`

**Interfaces:**
- Consumes: `ClassifiedExpr` 的 `std::vector` / `std::optional` 返回
- Produces: `__applyOne(ClassifiedExpr &&, ...)` 右值引用签名

- [ ] **Step 1: 修改 __applyOne 签名**

在 `compiler.h` 中：

```cpp
// 旧：
void __applyOne(ClassifiedExpr &_E, std::vector<std::string> &_Pos);

// 新：
void __applyOne(ClassifiedExpr &&_E, std::vector<std::string> &_Pos);
```

调用处改为：

```cpp
for (auto &e : exprs) {
    __applyOne(std::move(e), vecPositional);
    if (bFired__) return;
}
```

- [ ] **Step 2: 修改 recognizeAll 返回 move**

在 `expression_recognizer.h` 中确认 `recognizeAll` 返回值已有 NRVO（编译器自动优化），无需改动。

- [ ] **Step 3: 构建并运行测试**

```powershell
cd H:\dsh_space\cmd_args
xmake build unit_test && xmake run unit_test
```
预期：122/125 通过

- [ ] **Step 4: 提交**

```powershell
git add include/cmd_args/expression/compiler.h
git commit -m "perf(compiler): __applyOne accepts rvalue ClassifiedExpr, eliminate copies"
```

---

## Task 4: 消除 legacy path（P1）

**Files:**
- Modify: `include/cmd_args/expression/compiler.h`

**Interfaces:**
- Consumes: `Lexer::scanStrings()` 还原归一化串为类型化流
- Produces: 单一 `__parseTokens(TokenStream_T &, Envs_T const &)` 管线

- [ ] **Step 1: 删除 __parseTokensLegacy**

在 `compiler.h` 中删除整个 `__parseTokensLegacy` 方法（~25 行）。

- [ ] **Step 2: 修改 parse(Tokens_T, Envs_T) 走类型化管线**

```cpp
void parse(Tokens_T const &_Tokens, Envs_T const &_Env)
{
    TokenStream_T real = Lexer::scanStrings(_Tokens);
    __parseTokens(real, _Env);
}
```

- [ ] **Step 3: 修改 __parseTokens 使用完整 Recognizer 归约**

还原 `__parseTokens` 为 Recognizer 驱动（撤销 Task 2 的 bypass）：

```cpp
void __parseTokens(TokenStream_T &_Tokens, std::vector<std::string> const &_Env)
{
    try {
        pModel__->strProgramName = program_name;
        envParser__.parse(_Env);
        ensureBuiltins();
        optParser__.setGraph(&pModel__->optionGraph);

        ExpressionRecognizer rec(&optParser__, &argParser__, &envParser__);
        auto exprs = rec.recognizeAll(_Tokens);

        bFired__ = false;
        std::vector<std::string> vecPositional;

        for (auto &e : exprs) {
            __applyOne(std::move(e), vecPositional);
            if (bFired__) return;
        }

        pModel__->optionGraph.evaluate();
        argParser__.assignPositional(vecPositional);

        for (auto const &tok : _Tokens) {
            auto text = tok.toText();
            if (!util::starts_with(text, "-")) continue;
            throw ErrorParse(ParseError_E::UNKNOWN_OPTION,
                "unrecognized option or argument: " + text);
        }
    }
    catch (...) {
        pModel__->opts.clearParseOrder();
        pModel__->optionGraph.clear();
        throw;
    }
}
```

- [ ] **Step 4: 验证语义一致性**

此步骤需要确认 Recognizer 的聚合/关系识别与旧 Parser 行为完全对齐。由于关系识别（HeadStash/CondAwaitColon）仍有 2 个 deferred 测试，此步骤可能暂时失败。如果失败，保留 legacy path 作为 fallback（重新应用 Task 2 的 bypass）并记录为已知限制。

- [ ] **Step 5: 构建并运行测试**

```powershell
cd H:\dsh_space\cmd_args
xmake build unit_test && xmake run unit_test
```

**分支 A：全部通过** → 提交移除 legacy path。
**分支 B：部分失败** → 回退到 Task 2 bypass 状态，提交保留 legacy path + 标记为后续跟进。

- [ ] **Step 6: 提交**

```powershell
# 分支 A：
git add include/cmd_args/expression/compiler.h
git commit -m "refactor(compiler): eliminate legacy path, unified typed pipeline"

# 分支 B：
git checkout -- include/cmd_args/expression/compiler.h
git commit -m "chore(compiler): keep legacy path, recognizer parity pending"
```

---

## Task 5: step() 单 token 缓冲 + lexer.h 拆分（P2）

**Files:**
- Modify: `include/cmd_args/expression/lexer.h`
- Create: `include/cmd_args/expression/scanner.h`

**Interfaces:**
- Consumes: `Scanner` 类从 `lexer.h` 移至 `scanner.h`
- Produces: `lexer.h` 改为 `#include "scanner.h"`，Lexer 门面不变

- [ ] **Step 1: 创建 scanner.h**

将 `lexer.h` 中 `namespace detail { ... class Scanner { ... }; }` 整块移至新文件 `scanner.h`。保留 `Token` 别名（`using Token = ::cmd_args::Token;`）。

- [ ] **Step 2: 修改 lexer.h**

```cpp
#pragma once

#include "cmd_args/expression/token.h"
#include "cmd_args/expression/scanner.h"
// ... Lexer 类定义不变
```

- [ ] **Step 3: 修改 Scanner::step() 中 tResult__ 为单 token 缓冲**

```cpp
// 旧：
Tokens_T tResult__;

// 新：
std::optional<Token> tResultSingle__;
```

修改 `__processEnd` 以兼容：
```cpp
void __processEnd(char const _Cur, Token &_T)
{
    (void)_Cur;
    _T.setLoc(cntLoc__);
    tResultSingle__ = _T;  // 单 token 缓冲
    _T.clear();
    __setState(Phrase_E::BEGIN);
}
```

修改 `scan()` 以兼容：
```cpp
Tokens_T scan()
{
    Token tDefault;
    Tokens_T res;
    while (!__isSourceEnd()) {
        if (__isLineEnd()) { __flush(tDefault); __nextLine(); continue; }
        tblScanInput__[enState__](*itPc__, tDefault);
        if (tResultSingle__) { res.push_back(std::move(*tResultSingle__)); tResultSingle__.reset(); }
    }
    __flush(tDefault);
    if (tResultSingle__) res.push_back(std::move(*tResultSingle__));
    return res;
}
```

修改 `step()` 以兼容：
```cpp
std::optional<Token> step(char _Cur)
{
    if (_Cur == '\0') {
        __flush(tCur__);
    } else {
        tblScanInput__[enState__](_Cur, tCur__);
        if (enState__ == Phrase_E::END) {
            __processEnd('\0', tCur__);
            if (_Cur != '\0' && !(enMode__ == LexMode_E::CommandLine && _Cur == '=')) {
                tblScanInput__[Phrase_E::BEGIN](_Cur, tCur__);
                if (enState__ == Phrase_E::END) { __processEnd('\0', tCur__); }
            }
        }
    }
    if (tResultSingle__) {
        auto tok = std::move(*tResultSingle__);
        tResultSingle__.reset();
        return tok;
    }
    return std::nullopt;
}
```

- [ ] **Step 4: 构建并运行测试**

```powershell
cd H:\dsh_space\cmd_args
xmake build unit_test && xmake run unit_test
```
预期：122/125 通过

- [ ] **Step 5: 提交**

```powershell
git add include/cmd_args/expression/scanner.h include/cmd_args/expression/lexer.h
git commit -m "refactor(lexer): split scanner.h from lexer.h, step() single-token buffer"
```

---

## Task 6: HeadStash 快速路径 + 全量回归（P1）

**Files:**
- Modify: `include/cmd_args/expression/expression_recognizer.h`
- Test: `dfx/unit_test/test_expression_classifier.cpp`

**Interfaces:**
- Consumes: Recognizer 内部 vecBuf__ 结构（deque）
- Produces: 行为不变（只是加速常见路径）

- [ ] **Step 1: 在 HeadStash 分支添加快速路径**

在 `expression_recognizer.h` 的 `__tryReduce` 中，HeadStash 分支内：

```cpp
if (enSeq__ == SeqKind_E::HeadStash) {
    // 快速路径：vecBuf__ 有两个 token 且第二个不可能是关系尾
    if (vecBuf__.size() > 1) {
        auto const nextAttr = vecBuf__[1].getAttr();
        if (nextAttr == Token::Type_E::Variable
            || nextAttr == Token::Type_E::Integer
            || nextAttr == Token::Type_E::String
            || nextAttr == Token::Type_E::Real) {
            // 不可能是关系尾（'?' 是 Delimiter，'>' 是 ContinualName 前缀）
            // 直接 emit aggregate，跳过 HeadStash 等待
            _Out.push_back(__makeAggregateCandidate(vecBuf__[0]));
            vecBuf__.pop_front();
            __clearSeq();
            _Progress = true;
            return true;
        }
    }
    // ...原有 HeadStash 逻辑
}
```

- [ ] **Step 2: 写回归测试**

在 `dfx/unit_test/test_expression_classifier.cpp` 中追加：

```cpp
TEST_CASE("recognizer: aggregate -abc 后跟 positional 走快速路径", "[classifier]")
{
    Model model;
    OptionBuilder ob(&model.opts);
    ob.addFlag('a', "ena", "a");
    ob.addFlag('b', "enb", "b");
    OptionParser op(&model.opts);
    ArgumentParser ap(&model.args);
    EnvParser ep(&model.envs);
    ExpressionRecognizer rec(&op, &ap, &ep);

    TokenStream_T toks{
        Token(Token::Type_E::ContinualName, "ab", 0),
        Token(Token::Type_E::Variable, "val", 0),
        Token(Token::Type_E::Variable, "extra", 0)
    };
    auto exprs = rec.recognizeAll(toks);
    REQUIRE(exprs.size() == 2);
    REQUIRE(exprs[0].kind == ExpressionKind_E::Option);
    REQUIRE(exprs[0].cShortName == 'a');
    REQUIRE(exprs[1].bPositional);
    REQUIRE(exprs[1].name == "val");
}
```

- [ ] **Step 3: 构建并运行测试**

```powershell
cd H:\dsh_space\cmd_args
xmake build unit_test && xmake run unit_test
```
预期：新增测试通过 + 基线不变

- [ ] **Step 4: 全量回归**

```powershell
xmake run unit_test
```
确认 122+/125 通过。

- [ ] **Step 5: 提交**

```powershell
git add include/cmd_args/expression/expression_recognizer.h dfx/unit_test/test_expression_classifier.cpp
git commit -m "perf(recognizer): HeadStash fast-path for non-relation tails"
```

---

## Self-Review

**1. Spec 覆盖：**
- ✅ P0：vecBuf__ deque（Task 1）+ 批量路径跳过 Recognizer（Task 2）
- ✅ P1：move 语义（Task 3）+ legacy path 消除（Task 4）+ HeadStash 快速路径（Task 6）
- ✅ P2：step() 缓冲 + lexer 拆分（Task 5）
- ✅ 全量回归（Task 6 Step 4）

**2. 占位符扫描：** 无 TBD/TODO。Task 4 有分支决策（A/B），已明确条件。

**3. 类型一致性：**
- `vecBuf__` 类型在 Task 1 中从 `vector<Token>` 改为 `deque<Token>`，Task 2/3/6 依赖此变更
- `__applyOne` 签名在 Task 3 中改为右值引用，Task 2/4/6 调用处同步更新
- `tResultSingle__` 在 Task 5 中定义，不影响其他任务

## Execution Handoff

Plan complete and saved to `.dsh/plans/2026-08-27-expression-arch-perf-opt.md`。

**1. Subagent-Driven (recommended)** — 每任务分派独立 subagent，任务间审查

**2. Inline Execution** — 在当前会话中逐步执行

Which approach?
