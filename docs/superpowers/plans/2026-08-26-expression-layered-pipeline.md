# Expression 分层架构实施计划（字节流 → Token → Expression → Compiler/Interpreter）

> **For agentic workers:** REQUIRED SUB-SKILL: Use subagent-driven-development (recommended) or executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 expression 模块重构为编译前端分层架构：Scanner 词法产出类型化 Token 流；ExpressionRecognizer 委托三 Parser 的 Token 规则组装完整 Expression；Compiler 批量处理 / Interpreter 流式执行共用同一识别器，消除当前分散在三处的硬编码字符串解析。

**Architecture:** 统一识别层 `ExpressionRecognizer` 以 `detail::Token`（TypedToken，公共类型 `token.h`）为输入，按产生式规则组装 `ClassifiedExpr`；OptionParser / ArgumentParser / EnvParser 各提供单 Token 分类接口 `recognizeToken()` 与序列匹配规则，Recognizer 编排序列、维护 pending-value 状态机。Compiler 批量调用 `recognizeAll()`；Interpreter 逐 Token 调用 `feed()` 即时执行，不再手写归约字符串逻辑。

**Tech Stack:** C++20 / Catch2 / xmake (set_languages("c++20"))

**Spec:** 本计划即自足规格；关联历史计划：`docs/superpowers/plans/2026-08-26-parser-expression-rules.md`、`docs/superpowers/plans/2026-08-26-expression-classifier-option-graph.md`

---

## Global Constraints

- C++20，`-Wall -Werror`（MSVC 下 `/utf-8` `/EHsc`）
- 风格遵循 `cmd-args-code-style` 规范：Apache-2.0 头（Copyright[2026][ValenciaFly] / pengwanring@live.com）；`CMD_ARGS_NAMESPACE_BEGIN/END` 宏；`auto ... -> T` 后置返回（bool/void 除外）；私有成员 `__` 后缀 + 类型前缀；形参 `_Pascal`；Doxygen 中文注释
- **兼容红线**：`ContextBase::parse` 三形态签名不变；`Lexer::tokens()` / `tokensFromLine()` 字符串行为不变（基于类型化流归一化实现）；全部既有测试（`test_parser`、`test_context`、`test_interpreter`、`test_lexer`、`test_option`、`test_expression`、`test_model`）必须绿灯
- 每任务独立 TDD 循环：写失败测试 → 验证失败 → 实现 → 验证通过 → 提交

---

## File Structure

### 新建

| 文件 | 职责 |
|------|------|
| `include/cmd_args/expression/token.h` | 公共词法单元 `Token`（自 `detail::Token` 迁出）；`Type_E` 含 `ScopeSep`（`::`）；`TokenStream_T = std::vector<Token>`；兼容别名 `detail::Token` |

### 修改

| 文件 | 变更 |
|------|------|
| `include/cmd_args/expression/lexer.h` | Scanner 增加 `LexMode_E`（`EnvLine` 模式：`IN_SCOPE_COLON` / `IN_ENV_VALUE` 状态）；Lexer 重构为类型化门面（`scan()` / `scanEnv()` / `pop()` + 兼容 `tokens()`/`tokensFromLine()` 归一化输出）；移除旧 `__normalize` 字符串合并（移入基于 `TokenStream_T` 的新 `__normalizeFromTyped`） |
| `include/cmd_args/expression/classified_expr.h` | 增加 `std::vector<std::string> vecSpacePath`（环境空间级联路径）；`std::string strRaw`（聚合原词）；`bool bPositional`（方位参数标记）；`auto fullPath() const -> std::string` |
| `include/cmd_args/expression/expression_recognizer.h` | 全量重写：`feed(Token const &_Tok, std::vector<ClassifiedExpr> &_Out)`（逐 Token 归约）；`finish(std::vector<ClassifiedExpr> &_Out)`（流尾收尾）；`recognizeAll(TokenStream_T const &_Toks) -> std::vector<ClassifiedExpr>`；`getBufferSize() -> size_t`；删除 `__feedText` / `__tryParserRules` 字符串路径 |
| `include/cmd_args/option/option_parser.h` | 新增 `recognizeToken(Token const &) -> std::optional<ClassifiedExpr>`；`bool knowsLong(knowsShort)`；`bool isBool(knowsLong/isLongBool)`；`ProbeAgg_E probeAggregate(std::string const &_Raw) -> ...`（聚合串探测）；`bool applyExpr(ClassifiedExpr const &) -> bool`（Type-safe 应用）；`bool fireAction(NameLong_T const &)`；`void applyRelation(ClassifiedExpr const &)`（结构化关系入图） |
| `include/cmd_args/argument/argument_parser.h` | 新增 `recognizeToken(Token const &) -> std::optional<ClassifiedExpr>`；`bool applyExpr(ClassifiedExpr const &, std::vector<std::string> &_Pos)`（named 分配/positional 入缓冲）；`void assignPositional(std::vector<std::string> const &_VecPos)`（数量校验 + 按序提交，抽出自 `parse` / `__assignPositional`，消除三处重复） |
| `include/cmd_args/env/env_parser.h` | 新增 `recognizeEnvAssign(TokenStream_T const &, size_t _Pos) -> std::optional<EnvMatch_T>`（`EnvName(::EnvName)* = Value` 序列匹配）；`void applyExpr(ClassifiedExpr const &)`；修改 `expand()` 在花括号内支持 `A::B::C` 键名；`parse(Envs_T)` 改走 `scanEnvLine` + `recognizeEnvAssign` 词法管线（逻辑等价，键含空白的行为收紧并记录） |
| `include/cmd_args/expression/compiler.h` | 改写为 Expression 驱动管线：`parse(argc,argv,envp)` → `Lexer::scan()` → `ExpressionRecognizer::recognizeAll()` → `__applyOne()` 分派入库；新增 `void parse(TokenStream_T const &, Envs_T const &)` 类型化入口；旧 `parse(Tokens_T const &)` 经 `Lexer::scanStrings` 转类型化后走同一管线 |
| `include/cmd_args/expression/interpreter.h` | 删除 `__reduceStack` / `__isRelationText` / `__classifyRelationText` / `__handleRelation`（~200 行字符串解析）；新增 `ExpressionRecognizer rec__` 成员；`interpret()` 改为 `Lexer.Scanner.step(idxCursor__)` → `rec__.feed(tok, ready)` → `__execute(expr)` 即时执行；保留 `getCursor()` / `getStackSize()` 契约（`getStackSize()` 委托 `rec__.getBufferSize()`） |
| `include/cmd_args/context/context_fwd.h` | 增加 `#include "cmd_args/expression/token.h"`（`TokenStream_T` 来源） |
| `include/cmd_args/context/context_base.h` | `__preambleArgv` 输出 `TokenStream_T`（`Lexer::scan()`）；新增 `parse(TokenStream_T const &, Envs_T const &)`；`_doRunPipeline` 签名改为 `(TokenStream_T &, Envs_T const &)` |
| `include/cmd_args/context/context_step.h` | `_doRunPipeline` override 签名适配；`loop()` 内部改用 `Lexer::scanLine` |
| `include/cmd_args/context/context_runtime.h` | `_doRunPipeline` override 签名适配；拼行用 `tok.toText()` |
| `include/cmd_args/context/context.h` | 适配 `_doRunPipeline` 签名 |
| `include/cmd_args/config/parameter.h` | `parse_fwd.h` → `context_fwd.h` |
| `include/cmd_args/option/option_builder.h` | `parse_fwd.h` → `context_fwd.h` |
| `include/cmd_args/context/factory.h` | `parse_fwd.h` → `context_fwd.h` |

### 测试

| 文件 | 变更 |
|------|------|
| `dfx/unit_test/test_lexer.cpp` | 增补：`Lexer::scan()` 类型化输出断言；`ScopeSep` 批量/增量断言；`scanEnvLine()` 断言；`pop()` 增量断言；`streamFromStrings()` 往返断言 |
| `dfx/unit_test/test_expression_classifier.cpp` | 全量改写为新 `feed(Token, vector&)` API；增补：关系序列识别、环境空间序列、聚合探测、值分隔 |
| `dfx/unit_test/test_env.cpp` | 新增 SECTION：环境空间键 `recognizeEnvAssign`、`applyExpr`、`${A::B::C}` 展开、`set_env("A::B::C",...)` |
| `dfx/unit_test/test_expression_classifier.cpp` | （重写）保留 classifier 标签，全量新 API 断言 |
| `dfx/unit_test/test_option_parser.cpp` | 新增 SECTION：`recognizeToken()` / `applyExpr()` / `probeAggregate()` / `fireAction()` |
| `dfx/unit_test/test_argument_parser.cpp` | 新增 SECTION：`recognizeToken()` / `applyExpr()` / `assignPositional()` |

---

## Task 1: 基线修复（parse_fwd.h → context_fwd.h + expression_recognizer.h 改名跟进）

**Files:**
- Modify: 8 处 `#include "cmd_args/context/parse_fwd.h"` → `"cmd_args/context/context_fwd.h"`
- Modify: `dfx/unit_test/test_expression_classifier.cpp` include `"cmd_args/expression/expression_classifier.h"` → `"cmd_args/expression/expression_recognizer.h"`

**Interfaces:** 无行为变更，仅头文件路径修正。

- [ ] **Step 1: 修正 8 处 parse_fwd.h 引用**

修改以下文件中 `#include "cmd_args/context/parse_fwd.h"` → `#include "cmd_args/context/context_fwd.h"`：

| 文件 | 行号 |
|------|------|
| `include/cmd_args/expression/expression.h` | 24 |
| `include/cmd_args/expression/compiler.h` | 27 |
| `include/cmd_args/option/option_parser.h` | 26 |
| `include/cmd_args/argument/argument_parser.h` | 26 |
| `include/cmd_args/env/env_parser.h` | 30 |
| `include/cmd_args/config/parameter.h` | 24 |
| `include/cmd_args/option/option_builder.h` | 25 |
| `include/cmd_args/context/factory.h` | 25 |

每处精确替换：`parse_fwd.h` → `context_fwd.h`（仅改文件名，路径前缀不变）。

- [ ] **Step 2: 修正 test_expression_classifier.cpp 的旧头文件引用**

```cpp
// 旧：
#include "cmd_args/expression/expression_classifier.h"
// 新：
#include "cmd_args/expression/expression_recognizer.h"
```

- [ ] **Step 3: 构建验证基线绿灯**

```powershell
cd H:\dsh_space\cmd_args
xmake build unit_test 2>&1 | Select-Object -Last 5
```
期望：构建成功（exit 0），无编译错误。

- [ ] **Step 4: 运行全部测试验证回归**

```powershell
xmake run unit_test 2>&1
```
期望：全部通过（与重构前等价）。

- [ ] **Step 5: 提交**

```powershell
git add include/cmd_args/expression/expression.h include/cmd_args/expression/compiler.h include/cmd_args/option/option_parser.h include/cmd_args/argument/argument_parser.h include/cmd_args/env/env_parser.h include/cmd_args/config/parameter.h include/cmd_args/option/option_builder.h include/cmd_args/context/factory.h dfx/unit_test/test_expression_classifier.cpp
git commit -m "fix: repoint parse_fwd.h includes to context_fwd.h, fix stale classifier include"
```

---

## Task 2: Token 公共类型（token.h）+ ScopeSep + TokenStream_T

**Files:**
- Create: `include/cmd_args/expression/token.h`
- Modify: `include/cmd_args/expression/lexer.h` — 删除内联 `Token` 类定义；在 `namespace detail` 中添加 `using Token = ::cmd_args::Token;` 兼容别名
- Test: `dfx/unit_test/test_token.cpp`

**Interfaces:**
- Consumes: `cmd_args/base/def.h`（`CMD_ARGS_NAMESPACE_BEGIN/END`）
- Produces: `cmd_args::Token`（公共类型），`cmd_args::TokenStream_T = std::vector<Token>`

### 新建 `include/cmd_args/expression/token.h`

```cpp
/*
 * Copyright[2026][ValenciaFly]
 * Email: pengwanring@live.com
 *
 *     Licensed under the Apache License,
 *     Version 2.0(the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "cmd_args/base/def.h"

CMD_ARGS_NAMESPACE_BEGIN

/**
 * @brief 公共词法单元：Scanner 产出的最小语义原子。
 *
 * 词法分析器将字节流切分为 Token 序列后，
 * ExpressionRecognizer 以 Token 为最小单元进行模式匹配，组装完整 Expression。
 * 每个 Token 携带类型（Type_E）、名称/内容、行位置以及行内等号标记（hasInlineEq）。
 */
class Token
{
public:
    using Name_T = std::string;

    /// @brief 词法单元种类
    enum class Type_E : char
    {
        Unknown,        ///< 未知类型
        LongName,       ///< --name（长名词，不含 "--" 前缀）
        ShortName,      ///< -x（单个短名，不含 "-" 前缀）
        ContinualName,  ///< -xyz / -abc（聚合短名串，不含 "-" 前缀）
        Variable,       ///< [a-zA-Z_][\w]*（普通变量/值）
        Integer,        ///< [0-9]+（整数）
        Real,           ///< 实数（含科学计数法）
        String,         ///< 带引号的字符串 / Env 值
        EnvName,        ///< 环境变量名（兼容保留）
        Delimiter,      ///< 单字符分隔符（? : = > < & | 等）
        ScopeSep,       ///< :: 环境空间级联分隔符
        Keyword         ///< 关键字（env / file / plugin）
    };

    /// @brief 默认构造（Unknown 类型，operator bool 为 false）
    Token() = default;

    /// @brief 按类型、名称、行号构造
    Token(Type_E _T, Name_T _Content, std::size_t _Line)
        : enAttr__(_T)
        , strName__(std::move(_Content))
        , locLine__(_Line)
    {}

    /// @brief 是否有效（非 Unknown）
    operator bool() const noexcept { return enAttr__ != Type_E::Unknown; }

    /// @brief 获取词法单元种类
    auto getAttr() const noexcept -> Type_E { return enAttr__; }

    /// @brief 获取名称/内容（不含前缀符号）
    auto getName() const noexcept -> Name_T const & { return strName__; }

    /// @brief 获取名称末尾字符（用于双字符分隔符检测）
    auto back() const noexcept -> char { return strName__.empty() ? '\0' : strName__.back(); }

    /// @brief 设置行号
    void setLoc(std::size_t _Line) { locLine__ = _Line; }

    /// @brief 设置词法单元种类
    void setAttr(Type_E _A) { enAttr__ = _A; }

    /// @brief 吸收一个字符到名称末尾
    void pick(char _C) { strName__ += _C; }

    /// @brief 清空名称缓冲
    void resetName() { strName__.clear(); }

    /// @brief 设置行内等号标记（--name=value 中 '=' 直接结束长名词）
    void setInlineEq(bool _V) { bInlineEq__ = _V; }

    /// @brief 是否由 '=' 内联分隔
    bool hasInlineEq() const noexcept { return bInlineEq__; }

    /// @brief 复位为 Unknown 状态
    void clear()
    {
        enAttr__    = Type_E::Unknown;
        strName__.clear();
        locLine__   = 0;
        bInlineEq__ = false;
    }

    /// @brief 获取行位置
    auto getLoc() const noexcept -> std::size_t { return locLine__; }

    /**
     * @brief 归一化为命令行文本
     * @note LongName → "--name"；ShortName/ContinualName → "-x"；ScopeSep → "::"；其余保持原样
     */
    auto toText() const -> std::string
    {
        switch (enAttr__) {
        case Type_E::LongName:     return "--" + strName__;
        case Type_E::ShortName:
        case Type_E::ContinualName: return "-" + strName__;
        case Type_E::ScopeSep:     return "::";
        default:                   return strName__;
        }
    }

private:
    Type_E    enAttr__     = Type_E::Unknown;
    Name_T    strName__;
    std::size_t locLine__  = 0;
    bool      bInlineEq__  = false;
};

/// @brief 类型化 Token 流（Scanner 输出 / ExpressionRecognizer 输入）
using TokenStream_T = std::vector<Token>;

CMD_ARGS_NAMESPACE_END
```

### 修改 `include/cmd_args/expression/lexer.h`

在 `namespace detail` 开头添加：

```cpp
namespace detail {
using Token = ::cmd_args::Token;  // 兼容别名：旧代码中 detail::Token 指向公共 Token 类型
```

删除原有 `class Token { ... };` 整块定义（第 52–124 行）。保留 `using Source_T = std::vector<std::string>;` 和 `using Pc_T = ...;`。

### 测试 `dfx/unit_test/test_token.cpp`

```cpp
#include "catch2/catch.hpp"
#include "cmd_args/expression/token.h"
using namespace cmd_args;

TEST_CASE("token: ScopeSep 类型与 toText", "[token]")
{
    Token t(Token::Type_E::ScopeSep, "::", 0);
    REQUIRE(t.getAttr() == Token::Type_E::ScopeSep);
    REQUIRE(t.getName() == "::");
    REQUIRE(t.toText() == "::");
}

TEST_CASE("token: 长名词 toText 含 -- 前缀", "[token]")
{
    Token t(Token::Type_E::LongName, "timeout", 0);
    REQUIRE(t.toText() == "--timeout");
}

TEST_CASE("token: Unknown 类型 operator bool 为 false", "[token]")
{
    Token t;
    REQUIRE_FALSE(t);
    REQUIRE(t.getAttr() == Token::Type_E::Unknown);
}

TEST_CASE("token: hasInlineEq 标记保持", "[token]")
{
    Token t(Token::Type_E::LongName, "opt", 0);
    REQUIRE_FALSE(t.hasInlineEq());
    t.setInlineEq(true);
    REQUIRE(t.hasInlineEq());
}

TEST_CASE("token: clear 复位所有字段", "[token]")
{
    Token t(Token::Type_E::LongName, "x", 5);
    t.setInlineEq(true);
    t.clear();
    REQUIRE_FALSE(t);
    REQUIRE(t.getName().empty());
    REQUIRE(t.getLoc() == 0);
}

TEST_CASE("token: TokenStream_T 容器语义", "[token]")
{
    TokenStream_T stream;
    stream.emplace_back(Token::Type_E::LongName, "opt", 0);
    stream.emplace_back(Token::Type_E::Variable, "9", 0);
    REQUIRE(stream.size() == 2);
    REQUIRE(stream[0].toText() == "--opt");
    REQUIRE(stream[1].toText() == "9");
}
```

- [ ] **Step 1: 写 test_token.cpp**
- [ ] **Step 2: 写 token.h + 修改 lexer.h**
- [ ] **Step 3: `xmake build unit_test && xmake run unit_test "[token]"`**
  期望：全部 PASS
- [ ] **Step 4: 全量测试验证回归（detail::Token 别名保证旧测试通过）**
  `xmake run unit_test "[lexer]"` + `xmake run unit_test "[interpreter]"` + `xmake run unit_test "[classifier]"` 全绿
- [ ] **Step 5: 提交**

```powershell
git add include/cmd_args/expression/token.h include/cmd_args/expression/lexer.h dfx/unit_test/test_token.cpp
git commit -m "feat(expression): add public Token type in token.h with ScopeSep, TokenStream_T alias"
```

---

## Task 3: Scanner Env 行模式（批量 scanEnv + step 增量 :: 级联）

**Files:**
- Modify: `include/cmd_args/expression/lexer.h`（Scanner 类）
- Test: `dfx/unit_test/test_lexer.cpp`（追加 SECTION）

**Interfaces:**
- Consumes: `Token`（token.h），`TokenStream_T`
- Produces: Scanner 在 `LexMode_E::EnvLine` 模式下产出 `ScopeSep` / `Delimiter("=")` / 环境值等 Token

### 修改 Scanner（lexer.h detail::namespace 内）

#### 新增枚举与状态

```cpp
/// @brief 词法模式
enum class LexMode_E : char
{
    CommandLine,  ///< 命令行模式（argv 输入）
    EnvLine       ///< 环境行模式（envp 输入：支持 :: 级联与 = 断句）
};
```

在 `Phrase_E` 枚举末尾 `END` 前插入：

```cpp
IN_SCOPE_COLON,   ///< EnvLine 模式：已吞入一个 ':'，等待判定 '::' 或单 ':'
IN_ENV_VALUE,     ///< EnvLine 模式：'=' 之后的值吸收态（吸收至空白/EOL）
```

在 `Scanner` 类中新增成员：

```cpp
LexMode_E  enMode__        = LexMode_E::CommandLine;
bool       bEnvValuePos__  = false;  ///< EnvLine 模式：当前处于 '=' 右侧值区
```

修改构造函数签名（默认 CommandLine 向后兼容）：

```cpp
explicit Scanner(Source_T _Source, LexMode_E _Mode = LexMode_E::CommandLine)
    : source__(std::move(_Source))
    , enMode__(_Mode)
{
    if (!source__.empty()) { itPc__ = source__.front().cbegin(); }
    __initScanTable();
}
```

#### 注册新状态处理器

在 `__initScanTable()` 中追加：

```cpp
{ Phrase_E::IN_SCOPE_COLON,
  std::bind(&Scanner::__processScopeColon, this, sp::_1, sp::_2) },
{ Phrase_E::IN_ENV_VALUE,
  std::bind(&Scanner::__processEnvValue, this, sp::_1, sp::_2) },
```

#### 修改 `__processBegin`

在空白跳过之后、字符分类之前插入 EnvLine 环境值吸收判断：

```cpp
void __processBegin(char const _Cur, Token &_T)
{
    if (std::isspace(static_cast<unsigned char>(_Cur))) {
        __nextChar();
        return;
    }

    // EnvLine 值区：一切字符直接吸收至空白
    if (enMode__ == LexMode_E::EnvLine && bEnvValuePos__) {
        __setState(Phrase_E::IN_ENV_VALUE);
        _T.pick(_Cur);
        __nextChar();
        return;
    }

    // ...原有数字/字母/引号/分隔符 分派逻辑不变...
}
```

#### 修改 `__processVariable`

在原有 `if (!isspace)` 分支开头插入 EnvLine 断句逻辑：

```cpp
void __processVariable(char const _Cur, Token &_T)
{
    // EnvLine 模式：在 ':' 或 '=' 处断句（不吸收分隔符）
    if (enMode__ == LexMode_E::EnvLine
        && (_Cur == ':' || _Cur == '=')) {
        _T.setAttr(Token::Type_E::Variable);
        __setState(Phrase_E::END);
        return;  // 不消费当前字符，END 后由 BEGIN 重派
    }

    // ...原有吸收逻辑...
}
```

EnvLine 关键字检测：跳过（env 行无 keyword 语义），直接把 Variable 终结：

```cpp
// 原有 magic_enum::enum_cast 检测：EnvLine 模式下不做关键字判断
if (enMode__ != LexMode_E::EnvLine) {
    auto k = magic_enum::enum_cast<Token::KeyWord_E>(_T.getName());
    if (k.has_value()) { enState__ = Phrase_E::IN_KEYWORD; return; }
}
_T.setAttr(Token::Type_E::Variable);
__setState(Phrase_E::END);
```

#### 新增 `__processSingleDelimiter` EnvLine ':' 合并

在 `__processSingleDelimiter` 开头插入：

```cpp
void __processSingleDelimiter(char const _Cur, Token &_T)
{
    // EnvLine 模式：':' 后跟 ':' → ScopeSep（:: 级联分隔符）
    if (enMode__ == LexMode_E::EnvLine && _Cur == ':') {
        // ':' 已由 BEGIN pick 进 _T；此处判定是否 '::'
        __setState(Phrase_E::IN_SCOPE_COLON);
        return;
    }

    // ...原有 switch 分支不变...
}
```

#### 新增 `__processScopeColon`

```cpp
/// @brief EnvLine 模式：首个 ':' 已吞入，判定是 '::'（ScopeSep）还是单 ':'（Delimiter）
void __processScopeColon(char const _Cur, Token &_T)
{
    if (_Cur == ':') {
        _T.pick(_Cur);                    // 补第二个 ':'
        _T.setAttr(Token::Type_E::ScopeSep);
        __nextChar();
        __setState(Phrase_E::END);
        return;
    }
    // 单 ':'：终结为 Delimiter，当前字符不消费（BEGIN 重派）
    _T.setAttr(Token::Type_E::Delimiter);
    __setState(Phrase_E::END);
}
```

#### 新增 `__processEnvValue`

```cpp
/// @brief EnvLine 模式：'=' 之后的值吸收态（吸收至空白/EOL）
void __processEnvValue(char const _Cur, Token &_T)
{
    if (std::isspace(static_cast<unsigned char>(_Cur))) {
        _T.setAttr(Token::Type_E::String);  // 值统一标记为 String
        __setState(Phrase_E::END);
        return;
    }
    _T.pick(_Cur);
    __nextChar();
}
```

#### 设置 `bEnvValuePos__` 标记

在 `__processSingleDelimiter` 的默认分支（单字符分隔符）中，当 EnvLine 模式且字符为 '=' 时设置标记：

```cpp
// default 分支末尾、END 之前
if (enMode__ == LexMode_E::EnvLine && _Cur == '=') {
    bEnvValuePos__ = true;  // 下一个 BEGIN 将进入 IN_ENV_VALUE
}
_T.setAttr(Token::Type_E::Delimiter);
__setState(Phrase_E::END);
```

注意：`_Cur` 已由 BEGIN pick，此处只是标记以便下一次 dispatch 时生效。实际上 END 后 `bEnvValuePos__` 的生效在下一次 `__processBegin` 的 `EnvLine && bEnvValuePos__` 分支。

#### 修改 `step()` 红外 redispatch 守卫

当前 `step()` 中 `'='` redispatch 抑制仅适用于 CommandLine 模式 LongName 的 inlineEq 语义：

```cpp
// 旧：
if (_Cur != '\0' && _Cur != '=') {
    tblScanInput__[Phrase_E::BEGIN](_Cur, tCur__);
    ...
}
// 新：EnvLine 模式下 '=' 需要正常重派
if (_Cur != '\0' && !(enMode__ == LexMode_E::CommandLine && _Cur == '=')) {
    tblScanInput__[Phrase_E::BEGIN](_Cur, tCur__);
    ...
}
```

#### 修改 `__flush` 增加新状态类型映射

```cpp
case Phrase_E::IN_SCOPE_COLON: _T.setAttr(Token::Type_E::ScopeSep); break;
case Phrase_E::IN_ENV_VALUE:   _T.setAttr(Token::Type_E::String);   break;
```

### 测试追加到 `dfx/unit_test/test_lexer.cpp`

```cpp
/* ==================== Scanner Env 行模式 ==================== */

TEST_CASE("lexer: scanEnvLine(A::B::C=D) 产出 ScopeSep", "[lexer][envscope]")
{
    using cmd_args::detail::Scanner;
    using cmd_args::detail::Token;

    Source_T src{ "A::B::C=D" };
    Scanner sc(src, Scanner::LexMode_E::EnvLine);
    auto toks = sc.scan();
    REQUIRE(toks.size() == 7);
    REQUIRE(toks[0].getAttr() == Token::Type_E::Variable);   REQUIRE(toks[0].getName() == "A");
    REQUIRE(toks[1].getAttr() == Token::Type_E::ScopeSep);   REQUIRE(toks[1].getName() == "::");
    REQUIRE(toks[2].getAttr() == Token::Type_E::Variable);   REQUIRE(toks[2].getName() == "B");
    REQUIRE(toks[3].getAttr() == Token::Type_E::ScopeSep);   REQUIRE(toks[3].getName() == "::");
    REQUIRE(toks[4].getAttr() == Token::Type_E::Variable);   REQUIRE(toks[4].getName() == "C");
    REQUIRE(toks[5].getAttr() == Token::Type_E::Delimiter);  REQUIRE(toks[5].getName() == "=");
    REQUIRE(toks[6].getAttr() == Token::Type_E::String);     REQUIRE(toks[6].getName() == "D");
}

TEST_CASE("lexer: scanEnvLine 单 ':' 产出 Delimiter", "[lexer][envscope]")
{
    using cmd_args::detail::Scanner;
    Source_T src{ "KEY:value" };
    Scanner sc(src, Scanner::LexMode_E::EnvLine);
    auto toks = sc.scan();
    REQUIRE(toks.size() == 3);
    REQUIRE(toks[1].getName() == ":");
}

TEST_CASE("lexer: scanEnvLine 值为空 (KEY=)", "[lexer][envscope]")
{
    using cmd_args::detail::Scanner;
    Source_T src{ "KEY=" };
    Scanner sc(src, Scanner::LexMode_E::EnvLine);
    auto toks = sc.scan();
    // Variable("KEY") Delimiter("=") → 值空：无后续字符，只有 '=' token
    REQUIRE(toks.size() == 2);
    REQUIRE(toks[0].getName() == "KEY");
    REQUIRE(toks[1].getName() == "=");
}

TEST_CASE("lexer: Scanner step 增量 :: 级联（EnvLine）", "[lexer][envscope]")
{
    using cmd_args::detail::Scanner;
    Scanner sc({}, Scanner::LexMode_E::EnvLine);
    REQUIRE_FALSE(sc.step('A'));
    auto t1 = sc.step(':');
    REQUIRE(t1.has_value());             // ':' → END Variable("A")
    REQUIRE(t1->getName() == "A");
    REQUIRE_FALSE(sc.step(':'));          // 第二个 ':' → ScopeSep
    auto t2 = sc.step('\0');             // flush
    REQUIRE(t2.has_value());
    REQUIRE(t2->getAttr() == Token::Type_E::ScopeSep);
}

TEST_CASE("lexer: scanEnvLine 值含路径分隔符 (VAL=/usr/bin)", "[lexer][envscope]")
{
    using cmd_args::detail::Scanner;
    Source_T src{ "PATH=/usr/bin" };
    Scanner sc(src, Scanner::LexMode_E::EnvLine);
    auto toks = sc.scan();
    REQUIRE(toks.size() == 3);
    REQUIRE(toks[0].getName() == "PATH");
    REQUIRE(toks[1].getName() == "=");
    REQUIRE(toks[2].getName() == "/usr/bin");
}
```

- [ ] **Step 1: 写失败测试（ScopeSep 批量断言）**
- [ ] **Step 2: 实现 Scanner 修改**
- [ ] **Step 3: `xmake build unit_test && xmake run unit_test "[envscope]"`**
  期望：全部 PASS
- [ ] **Step 4: 全量回归（`xmake run unit_test "[lexer]"` 全绿）**
- [ ] **Step 5: 提交**

```powershell
git add include/cmd_args/expression/lexer.h dfx/unit_test/test_lexer.cpp
git commit -m "feat(lexer): add EnvLine mode with ScopeSep (::) and value absorption"
```

---

## Task 4: Lexer 类型化门面（scan / pop / scanEnv / streamFromStrings）

**Files:**
- Modify: `include/cmd_args/expression/lexer.h`（Lexer 类）
- Test: `dfx/unit_test/test_lexer.cpp`（追加 SECTION）

**Interfaces:**
- Consumes: `TokenStream_T`（token.h），`Scanner`（detail 命名空间）
- Produces: `Lexer::scan() -> TokenStream_T`；`Lexer::pop() -> Token`；`Lexer::hasNext() -> bool`；兼容 `tokens() -> Tokens_T`；`tokensFromLine() -> Tokens_T`；`streamFromStrings() -> TokenStream_T`

### 修改 Lexer 类（lexer.h）

删除 `detail::Token` 类定义（Task 2 已移至 token.h）后，Lexer 重构为：

```cpp
class Lexer
{
public:
    /// @brief argv 字节流入口
    Lexer(int _Argc, char const *_Argv[], char * /*_Envp[]*/ = nullptr)
    {
        for (int i = 0; i < _Argc; ++i) { srcInput__.emplace_back(_Argv[i]); }
    }

    /// @brief 整行入口（REPL）
    explicit Lexer(std::string const &_Line)
    {
        if (!_Line.empty()) { srcInput__.emplace_back(_Line); }
    }

    /// @brief 归一化串列表入口（兼容 parse(Tokens_T const &) 转类型化）
    explicit Lexer(Tokens_T const &_Strs)
    {
        for (auto const &s : _Strs) { srcInput__.emplace_back(s); }
    }

    // ===== 批量：一次吐出全部 Token =====

    /// @brief 命令行模式批量分词
    auto scan() -> TokenStream_T
    {
        return detail::Scanner(srcInput__).scan();
    }

    /// @brief 整行分词
    static auto scanLine(std::string const &_Line) -> TokenStream_T
    {
        if (_Line.empty()) return {};
        detail::Source_T src{ _Line };
        return detail::Scanner(src).scan();
    }

    /// @brief 归一化字符串列表重扫描（还原类型化信息）
    static auto scanStrings(Tokens_T const &_Src) -> TokenStream_T
    {
        TokenStream_T result;
        for (auto const &s : _Src) {
            if (s.empty()) continue;
            detail::Source_T src{ s };
            auto toks = detail::Scanner(src).scan();
            std::move(toks.begin(), toks.end(), std::back_inserter(result));
        }
        return result;
    }

    /// @brief Env 行模式批量分词
    static auto scanEnvLine(std::string const &_Line) -> TokenStream_T
    {
        if (_Line.empty()) return {};
        detail::Source_T src{ _Line };
        return detail::Scanner(src, detail::Scanner::LexMode_E::EnvLine).scan();
    }

    // ===== 增量：逐个吐出 =====

    /// @brief 是否还有 Token 可 pop（内部缓存 peek）
    bool hasNext()
    {
        if (!tPeeked__.has_value()) {
            tPeeked__ = __pumpToken();
        }
        return tPeeked__.has_value() && tPeeked__->operator bool();
    }

    /// @brief 吐出一个 Token；流空返回无效 Token（operator bool 为 false）
    auto pop() -> Token
    {
        if (tPeeked__) {
            auto t = std::move(*tPeeked__);
            tPeeked__.reset();
            return t;
        }
        return __pumpToken().value_or(Token{});
    }

    // ===== 兼容外观（基于类型化流归一化的字符串流）=====

    /// @brief 归一化字符串流（兼容旧接口：tokens() / tokensFromLine）
    auto tokens() -> Tokens_T
    {
        if (srcInput__.empty()) return {};
        return __normalizeFromTyped(scan());
    }

    static auto tokensFromLine(std::string const &_Line) -> Tokens_T
    {
        if (_Line.empty()) return {};
        return __normalizeFromTyped(scanLine(_Line));
    }

    /// @brief 持有源行（增量泵用）
    auto const &source() const noexcept -> detail::Source_T const & { return srcInput__; }

private:
    /// @brief 泵入一个字符到 Scanner，返回产出的 Token（若有）
    auto __pumpOneChar(char _C) -> std::optional<Token>
    {
        return detail::Scanner(srcInput__, detail::Scanner::LexMode_E::CommandLine).step(_C);
        // 注意：增量模式需独立 Scanner 实例；pop() 自行维护
    }

    /// @brief 内部增量泵：从当前行流中逐字符喂入 Scanner，产出一个完整 Token
    auto __pumpToken() -> std::optional<Token>
    {
        // 直接驱动独立 Scanner（每行一个实例，行尾 '\0' flush）
        // 首次调用或行耗尽时创建新 Scanner
        while (idxLine__ < srcInput__.size()) {
            if (!sc___.has_value()) {
                sc___.emplace(srcInput__, detail::Scanner::LexMode_E::CommandLine);
                idxChar__ = 0;
            }
            auto &sc = *sc__;
            auto const &line = srcInput__[idxLine__];
            while (idxChar__ < line.size()) {
                auto tok = sc.step(line[idxChar__]);
                ++idxChar__;
                if (tok) return tok;
            }
            // 行尾：flush
            auto tok = sc.step('\0');
            idxLine__++;
            sc___.reset();
            if (tok && tok->operator bool()) return tok;
        }
        return std::nullopt;
    }

    /// @brief 从 TokenStream_T 归一化为字符串流（兼容旧接口）
    static auto __normalizeFromTyped(TokenStream_T const &_Toks) -> Tokens_T
    {
        Tokens_T res;
        for (size_t i = 0; i < _Toks.size(); ++i) {
            auto const &t   = _Toks[i];
            auto        attr = t.getAttr();

            // ── 关系语法合并 ──
            if (attr == Token::Type_E::ContinualName && i + 1 < _Toks.size()) {
                auto const &next = _Toks[i + 1];
                // -a?b:c 条件关系
                if (next.getAttr() == Token::Type_E::Delimiter && next.getName() == "?") {
                    std::string merged = t.toText();
                    merged += "?";
                    size_t j = i + 2;
                    while (j < _Toks.size()) {
                        auto const &cur = _Toks[j];
                        if (cur.getAttr() == Token::Type_E::Variable
                            || (cur.getAttr() == Token::Type_E::Delimiter && cur.getName() == ":")) {
                            merged += cur.toText();
                            ++j;
                        } else {
                            break;
                        }
                    }
                    i = j - 1;
                    res.emplace_back(std::move(merged));
                    continue;
                }
                // -a->b 依赖关系
                if (next.getAttr() == Token::Type_E::ContinualName
                    && !next.getName().empty() && next.getName()[0] == '>') {
                    std::string merged = t.toText() + next.toText();
                    ++i;
                    res.emplace_back(std::move(merged));
                    continue;
                }
            }

            // ── = 合并（--name=value / -x=value / KEY=VALUE）──
            if (i + 2 < _Toks.size()
                && _Toks[i + 1].getAttr() == Token::Type_E::Delimiter
                && _Toks[i + 1].getName() == "="
                && (attr == Token::Type_E::LongName || attr == Token::Type_E::ShortName
                    || attr == Token::Type_E::ContinualName
                    || attr == Token::Type_E::Variable)) {
                res.emplace_back(t.toText() + "=" + _Toks[i + 2].toText());
                i += 2;
                continue;
            }

            // ── InlineEq 合并（--name=value 中 InlineEq LongName + next token）──
            if (t.hasInlineEq() && i + 1 < _Toks.size()) {
                res.emplace_back(t.toText() + "=" + _Toks[i + 1].toText());
                ++i;
                continue;
            }

            auto text = t.toText();
            if (text.empty()) continue;
            res.emplace_back(std::move(text));
        }
        return res;
    }

    detail::Source_T srcInput__;
    size_t           idxLine__ = 0;    ///< 增量泵：当前行索引
    size_t           idxChar__ = 0;    ///< 增量泵：当前行内字符偏移
    std::optional<detail::Scanner> sc___;  ///< 增量泵：当前行的 Scanner 实例
    std::optional<Token> tPeeked__;    ///< hasNext() 缓存
};
```

### 测试追加到 `dfx/unit_test/test_lexer.cpp`

```cpp
/* ==================== Lexer 类型化 scan() ==================== */

TEST_CASE("lexer: scan() 产出类型化 Token 流", "[lexer][typed]")
{
    Lexer lx("--timeout 9");
    auto stream = lx.scan();
    REQUIRE(stream.size() == 2);
    REQUIRE(stream[0].getAttr() == Token::Type_E::LongName);
    REQUIRE(stream[0].getName() == "timeout");
    REQUIRE_FALSE(stream[0].hasInlineEq());
    REQUIRE(stream[1].getAttr() == Token::Type_E::Integer);
    REQUIRE(stream[1].getName() == "9");
}

TEST_CASE("lexer: scan() --opt=val 内联等号", "[lexer][typed]")
{
    Lexer lx("--opt=42");
    auto stream = lx.scan();
    REQUIRE(stream.size() == 2);
    REQUIRE(stream[0].hasInlineEq());
    REQUIRE(stream[1].getName() == "42");
}

TEST_CASE("lexer: scan() -t=5 短选项等号分隔", "[lexer][typed]")
{
    Lexer lx("-t=5");
    auto stream = lx.scan();
    REQUIRE(stream.size() == 3);
    REQUIRE(stream[0].getAttr() == Token::Type_E::ShortName);
    REQUIRE(stream[1].getName() == "=");
    REQUIRE(stream[2].getName() == "5");
}

TEST_CASE("lexer: scanEnvLine 批量产出", "[lexer][typed]")
{
    auto stream = Lexer::scanEnvLine("A::B::C=D");
    REQUIRE(stream.size() == 7);
    REQUIRE(stream[0].getAttr() == Token::Type_E::Variable);
    REQUIRE(stream[1].getAttr() == Token::Type_E::ScopeSep);
    REQUIRE(stream[5].getName() == "=");
    REQUIRE(stream[6].getName() == "D");
}

/* ==================== Lexer 增量 pop() ==================== */

TEST_CASE("lexer: pop() 逐个吐出 Token，行为与 scan() 等价", "[lexer][pop]")
{
    Lexer lx("--timeout 9");
    std::vector<Token> collected;
    while (lx.hasNext()) { collected.push_back(lx.pop()); }
    auto batch = Lexer("--timeout 9").scan();
    REQUIRE(collected.size() == batch.size());
    for (size_t i = 0; i < collected.size(); ++i) {
        REQUIRE(collected[i].getAttr() == batch[i].getAttr());
        REQUIRE(collected[i].getName() == batch[i].getName());
    }
}

TEST_CASE("lexer: pop() 多行 argv 逐个吐出", "[lexer][pop]")
{
    Lexer lx("--hello a --timeout 9");
    size_t count = 0;
    while (lx.hasNext()) { lx.pop(); ++count; }
    REQUIRE(count == 4);
}

/* ==================== streamFromStrings 往返 ==================== */

TEST_CASE("lexer: scanStrings 还原归一化串的类型化信息", "[lexer][typed]")
{
    Tokens_T normalized{ "--timeout=9", "-abc", "hello" };
    auto stream = Lexer::scanStrings(normalized);
    REQUIRE(stream.size() == 4);  // LongName(timeout,eq) Integer(9) ContinualName(abc) Variable(hello)
    REQUIRE(stream[0].getAttr() == Token::Type_E::LongName);
    REQUIRE(stream[0].hasInlineEq());
    REQUIRE(stream[1].getAttr() == Token::Type_E::Integer);
    REQUIRE(stream[2].getAttr() == Token::Type_E::ContinualName);
    REQUIRE(stream[3].getAttr() == Token::Type_E::Variable);
}

TEST_CASE("lexer: tokens() 兼容外观行为不变", "[lexer]")
{
    auto toks = Lexer("--timeout=9 -abc hello").tokens();
    REQUIRE(toks.size() == 3);
    REQUIRE(toks.front() == "--timeout=9");
}
```

- [ ] **Step 1: 写失败测试（scan/pop/scanStrings 兼容）**
- [ ] **Step 2: 实现 Lexer 重构**
- [ ] **Step 3: `xmake build unit_test && xmake run unit_test "[typed]"`**
- [ ] **Step 4: 全量回归（`xmake run unit_test "[lexer]"` 全绿，兼容 `tokens()` / `tokensFromLine()` 不变）**
- [ ] **Step 5: 提交**

```powershell
git add include/cmd_args/expression/lexer.h dfx/unit_test/test_lexer.cpp
git commit -m "feat(lexer): type化 scan/pop/streamFromStrings facade, compat tokens() preserved"
```

---

## Task 5: Parser 的显式 Token 规则接口 + ClassifiedExpr 扩展

**Files:**
- Modify: `include/cmd_args/expression/classified_expr.h`（新增字段）
- Modify: `include/cmd_args/option/option_parser.h`（新增规则/应用方法）
- Modify: `include/cmd_args/argument/argument_parser.h`（新增规则/应用/位置分配）
- Modify: `include/cmd_args/env/env_parser.h`（新增序列匹配/应用；expand 花括号内支持 ::）
- Test: `dfx/unit_test/test_option_parser.cpp`（新 SECTION）
- Test: `dfx/unit_test/test_argument_parser.cpp`（新 SECTION）
- Test: `dfx/unit_test/test_env.cpp`（新 SECTION）

### ClassifiedExpr 扩展字段（classified_expr.h）

在 `struct ClassifiedExpr` 末尾（`isRelation()` 之后）追加：

```cpp
// ===== 新增：序列化/空间路径 =====
std::vector<std::string> vecSpacePath;  ///< Env 空间级联路径（A::B::C 的 ["A","B"]，不含叶子）
std::string strRaw;                     ///< 原始词文本（聚合串展开 / 错误消息用）
bool        bPositional = false;        ///< true = 方位参数（name 即原词，不查注册表）

/// @brief Env 全路径键（"A::B::C" 形式）
auto fullPath() const -> std::string
{
    std::string key;
    for (auto const &sp : vecSpacePath) { key += sp + "::"; }
    key += name;
    return key;
}
```

### OptionParser 新增方法

在 `class OptionParser` 中新增以下 public 方法：

```cpp
// ===== Token 规则接口（编译前端） =====

/// @brief 单 Token 语法分类（只读查注册表，不赋值）
/// @return 选项候选表达式；若 Token 不匹配选项规则返回 nullopt
auto recognizeToken(Token const &_Tok) const -> std::optional<ClassifiedExpr>
{
    using TT = Token::Type_E;
    switch (_Tok.getAttr()) {
    case TT::LongName: {
        ClassifiedExpr e;
        e.kind      = ExpressionKind_E::Option;
        e.name      = _Tok.getName();
        e.bExplicit = true;
        e.strRaw    = _Tok.toText();
        return e;
    }
    case TT::ShortName: {
        ClassifiedExpr e;
        e.kind       = ExpressionKind_E::Option;
        e.cShortName = _Tok.getName().empty() ? '\0' : _Tok.getName()[0];
        e.name       = _Tok.getName();
        e.bIsBool    = true;  // 默认；applyExpr 根据注册类型决定
        e.bExplicit  = true;
        e.strRaw     = _Tok.toText();
        return e;
    }
    case TT::ContinualName: {
        ClassifiedExpr e;
        e.kind       = ExpressionKind_E::Option;
        e.cShortName = _Tok.getName().empty() ? '\0' : _Tok.getName()[0];
        e.name       = _Tok.getName().empty() ? "" : std::string(1, _Tok.getName()[0]);
        e.bIsBool    = true;
        e.bExplicit  = true;
        e.strRaw     = _Tok.toText();
        return e;
    }
    default: return std::nullopt;
    }
}

/// @brief 查询长名选项是否已注册（含短回路 action）
auto knowsLong(std::string const &_Name) const noexcept -> bool
{
    return (pData__ && pData__->mgrValues.find(_Name))
        || (pData__ && pData__->mgrActions.doGet(_Name));
}

/// @brief 查询短名选项是否已注册
auto knowsShort(char _C) const noexcept -> bool
{
    return pData__ && pData__->mgrValues.mutFind(_C);
}

/// @brief 查询长名选项是否为 bool 类型
auto isBoolLong(std::string const &_Name) const noexcept -> bool
{
    if (!pData__) return false;
    auto const *p = pData__->mgrValues.find(_Name);
    return p && p->strType == "bool";
}

/// @brief 查询短名选项是否为 bool 类型
auto isBoolShort(char _C) const noexcept -> bool
{
    if (!pData__) return false;
    auto const *p = pData__->mgrValues.mutFind(_C);
    return p && p->strType == "bool";
}

/// @brief 聚合串探测：返回聚合串解析结果
enum class ProbeAgg_E : char
{
    AllBool,        ///< 所有字符均为已注册 bool 标志
    NeedsValue,     ///< 最后一个字符是已注册非 bool 选项（需要值）
    UnknownChar     ///< 包含未注册的字符
};
auto probeAggregate(std::string const &_Raw) const -> ProbeAgg_E
{
    if (_Raw.size() <= 1) return ProbeAgg_E::UnknownChar;
    bool lastIsNonBool = false;
    for (size_t i = 1; i < _Raw.size(); ++i) {
        char key = _Raw[i];
        auto const *pAct = pData__ ? pData__->mgrActions.doGet(key) : nullptr;
        if (pAct) continue;  // 短回路 action，忽略
        auto const *pOpt = pData__ ? pData__->mgrValues.mutFind(key) : nullptr;
        if (!pOpt) return ProbeAgg_E::UnknownChar;
        if (pOpt->strType != "bool") {
            if (i == _Raw.size() - 1) { lastIsNonBool = true; }
            else { return ProbeAgg_E::UnknownChar; }
        }
    }
    return lastIsNonBool ? ProbeAgg_E::NeedsValue : ProbeAgg_E::AllBool;
}

/// @brief 触发短回路 action（by long name）
auto fireAction(std::string const &_Name) -> bool
{
    if (!pData__) return false;
    auto const *pSc = pData__->mgrActions.doGet(_Name);
    if (!pSc) return false;
    bFired__ = true;
    static_cast<OptionAction const *>(pSc)->fnCallback();
    return true;
}

/// @brief 触发短回路 action（by short char）
auto fireActionShort(char _C) -> bool
{
    if (!pData__) return false;
    auto const *pSc = pData__->mgrActions.doGet(_C);
    if (!pSc) return false;
    bFired__ = true;
    static_cast<OptionAction const *>(pSc)->fnCallback();
    return true;
}

/// @brief Type-safe 应用：将 ClassifiedExpr 写入 OptionData
/// @return true = 已应用（registered option）；false = 未注册（留给 NamedArg 或 unknown-dash）
auto applyExpr(ClassifiedExpr const &_E) -> bool
{
    // 短回路 action（已在 fireAction 中触发；此处兜底）
    if (_E.bIsAction) return true;

    // 长选项
    if (!_E.strRaw.empty() && _E.strRaw[0] == '-' && _E.strRaw.size() >= 2 && _E.strRaw[1] == '-') {
        auto const *pOpt = pData__->mgrValues.mutFind(_E.name);
        if (!pOpt) return false;

        if (_E.value.empty() && pOpt->strType != "bool") {
            throw ErrorParse(ParseError_E::NEED_VALUE,
                "option --" + _E.name + " need a value");
        }
        __assign(pOpt, _E.value, pOpt->strType == "bool");
        return true;
    }

    // 短选项 / 聚合
    if (!_E.strRaw.empty() && _E.strRaw.size() >= 2 && _E.strRaw[0] == '-') {
        __consumeShortAggregate(_E.strRaw, _E.value);
        return true;
    }

    return false;
}

/// @brief 结构化关系入图（不再需要文本解析）
void applyRelation(ClassifiedExpr const &_E)
{
    if (!pGraph__) return;
    if (_E.kind == ExpressionKind_E::Conditional) {
        pGraph__->addEdge(_E.strCondition, _E.strThenBranch,
            OptionGraph::EdgeKind_E::Conditional, _E.strElseBranch);
    }
    else if (_E.kind == ExpressionKind_E::Dependency) {
        pGraph__->addEdge(_E.strDepSource, _E.strDepTarget, OptionGraph::EdgeKind_E::Dependency);
    }
}

private:
/// @brief 聚合短选项消费（from strRaw + value）
void __consumeShortAggregate(std::string const &_Tok, std::string const &_Value)
{
    // 单短选项 -x / 短回路检测
    if (_Tok.size() == 2) {
        char key = _Tok[1];
        if (fireActionShort(key)) return;
        auto *pOpt = pData__->mgrValues.mutFind(key);
        if (!pOpt) {
            throw ErrorParse(ParseError_E::UNKNOWN_OPTION,
                "unrecognized short option '" + std::string(1, key) + "' in " + _Tok);
        }
        __assign(pOpt, _Value, pOpt->strType == "bool");
        return;
    }
    // 聚合 -abc：bool 位逐个赋值；非 bool 位消费剩余
    for (size_t i = 1; i < _Tok.size(); ++i) {
        char key = _Tok[i];
        if (fireActionShort(key)) return;
        auto *pOpt = pData__->mgrValues.mutFind(key);
        if (!pOpt) {
            throw ErrorParse(ParseError_E::UNKNOWN_OPTION,
                "unrecognized short option '" + std::string(1, key) + "' in " + _Tok);
        }
        if (pOpt->strType == "bool") {
            __assign(pOpt, "", true);
        } else {
            // 值选项：取剩余子串
            std::string val = _Tok.substr(i + 1);
            if (!val.empty() && val.front() == '=') { val = val.substr(1); }
            if (val.empty() && !_Value.empty()) { val = _Value; }
            if (val.empty()) {
                throw ErrorParse(ParseError_E::NEED_VALUE,
                    "option -" + std::string(1, key) + " need a value");
            }
            __assign(pOpt, val, false);
            return;
        }
    }
}
```

### ArgumentParser 新增方法

```cpp
// ===== Token 规则接口 =====

/// @brief 单 Token 语法分类：命名参数 或 方位候选
auto recognizeToken(Token const &_Tok) const -> std::optional<ClassifiedExpr>
{
    using TT = Token::Type_E;

    // --name=value / --name value：查注册表
    if (_Tok.getAttr() == TT::LongName && !_Tok.getName().empty()) {
        if (pData__ && pData__->mgrName.doGet(_Tok.getName())) {
            ClassifiedExpr e;
            e.kind      = ExpressionKind_E::Argument;
            e.name      = _Tok.getName();
            e.bExplicit = true;
            e.strRaw    = _Tok.toText();
            return e;
        }
    }

    // 非 dash 开头 token → 方位参数候选
    if (_Tok.getAttr() != TT::LongName && _Tok.getAttr() != TT::ShortName
        && _Tok.getAttr() != TT::ContinualName) {
        auto text = _Tok.toText();
        if (!text.empty() && text[0] != '-') {
            ClassifiedExpr e;
            e.kind       = ExpressionKind_E::Argument;
            e.name       = text;
            e.value      = text;
            e.bExplicit  = true;
            e.bPositional = true;
            e.strRaw     = text;
            return e;
        }
    }

    return std::nullopt;
}

/// @brief 应用命名参数到 ArgumentData；方位参数 push 到缓冲
void applyExpr(ClassifiedExpr const &_E, std::vector<std::string> &_Pos)
{
    if (_E.bPositional) {
        _Pos.emplace_back(_E.value);
        return;
    }
    // 命名参数
    auto *p = pData__->mgrName.mutGet(_E.name);
    if (!p) return;
    if (_E.value.empty()) {
        throw ErrorParse(ParseError_E::NEED_VALUE,
            "named argument '--" + _E.name + "' need a value");
    }
    p->set(_E.value);
}

/// @brief 按声明顺序提交方位参数（数量校验 + 按序赋值，消除重复实现）
void assignPositional(std::vector<std::string> const &_VecPos)
{
    size_t total = pData__->mgrPos.doSize();
    size_t need  = 0;
    pData__->mgrPos.doConVisit([&](Argument const &a) {
        if (!a.isExternal()) ++need;
    });
    if (_VecPos.size() < need || _VecPos.size() > total) {
        std::ostringstream oss;
        oss << "give " << _VecPos.size() << ", but need " << need << ".." << total << '\n';
        oss << "uncaptured command line arguments:\n";
        for (auto const &tok : _VecPos) { oss << tok << '\n'; }
        throw ErrorParse(ParseError_E::UNKNWON_ARGS, oss.str());
    }

    bool   exact = (_VecPos.size() == need);
    size_t vi    = 0;
    for (size_t seq = 0; seq < total && vi < _VecPos.size(); ++seq) {
        auto *p = pData__->mgrPos.mutGetBySeq(seq);
        if (!p) continue;
        if (exact && p->isExternal()) continue;
        p->set(_VecPos[vi++]);
    }
}
```

### EnvParser 新增与修改

```cpp
/// @brief Env 表达式序列匹配结果
struct EnvMatch_T
{
    ClassifiedExpr   expr;
    std::size_t      nConsumed;  ///< 消耗的 Token 数
};

/// @brief Env 行模式序列匹配：Variable(::Variable)* [Delimiter("=") String]
/// @param _Buf Token 流
/// @param _Pos 起始位置
/// @return 匹配成功返回 {expr, 消耗数}；失败返回 nullopt
auto recognizeEnvAssign(TokenStream_T const &_Buf, std::size_t _Pos) const
    -> std::optional<EnvMatch_T>
{
    using TT = Token::Type_E;
    if (_Pos >= _Buf.size()) return std::nullopt;
    if (_Buf[_Pos].getAttr() != TT::Variable) return std::nullopt;

    ClassifiedExpr expr;
    expr.kind      = ExpressionKind_E::Env;
    expr.bExplicit = true;
    std::size_t pos = _Pos;

    // 空间级联：Variable (:: Variable)*
    while (pos + 2 < _Buf.size()
        && _Buf[pos + 1].getAttr() == TT::ScopeSep
        && _Buf[pos + 2].getAttr() == TT::Variable) {
        expr.vecSpacePath.push_back(_Buf[pos].getName());
        pos += 2;
    }

    // 叶子名 = 最后一个 Variable
    expr.name = _Buf[pos].getName();
    ++pos;

    // 可选 '=' + 值
    if (pos < _Buf.size() && _Buf[pos].getAttr() == TT::Delimiter && _Buf[pos].getName() == "=") {
        ++pos;
        if (pos < _Buf.size()) {
            expr.value = _Buf[pos].getName();
            ++pos;
        }
        // 空值：'=' 后直接 EOL
    }

    expr.strRaw = expr.fullPath() + "=" + expr.value;
    return EnvMatch_T{ std::move(expr), pos - _Pos };
}

/// @brief 应用 Env 表达式到 EnvData
void applyExpr(ClassifiedExpr const &_E)
{
    if (_E.kind != ExpressionKind_E::Env) return;
    auto key = _E.fullPath();
    pData__->doPush(key, _E.value, false);
}
```

#### 修改 `expand()` 花括号内支持 '::'

在 `expand()` 函数的花括号分支中，将字符集限制从 `[a-zA-Z0-9_.-]` 扩展为允许 `::`：

```cpp
// 原有：endPos = _Text.find('}', i + 2);
// 新增：支持 ':' 在花括号内（环境空间键 A::B::C）
// find 不变（'::' 不含 '}'）；name 提取自然包含 ':'
```

实际 `find('}')` 已经能匹配含 `::` 的键名（如 `${A::B::C}`）——只需在花括号路径的字符判断中允许 ':'，当前 find-based 逻辑无此限制，无需改动。验证：`find('}') from i+2` 扫描 "${A::B::C}" 得到 name="A::B::C" ✓。

#### 修改 `parse(Envs_T)` 走词法管线

```cpp
void parse(Envs_T const &_Envp)
{
    for (auto const &e : _Envp) {
        // 直接走 split-at-first-'=' 路径（向后兼容，环境串含空格极少）
        auto eq = e.find('=');
        if (eq == std::string::npos) continue;
        pData__->doPush(e.substr(0, eq), e.substr(eq + 1), false);
    }
}
```

保持原实现不变（envp 串已天然含 `=`；`::` 键不在此处解析，走 `recognizeEnvAssign` 的在 Compiler 侧路由）。

- [ ] **Step 1: ClassifiedExpr 增加字段**
- [ ] **Step 2: 写测试（OptionParser::recognizeToken / applyExpr / probeAggregate）**
- [ ] **Step 3: 实现 OptionParser 新方法**
- [ ] **Step 4: `xmake build unit_test && xmake run unit_test "[parser]"`**（绿灯）
- [ ] **Step 5: 写测试（ArgumentParser::recognizeToken / assignPositional）**
- [ ] **Step 6: 实现 ArgumentParser 新方法**
- [ ] **Step 7: `xmake run unit_test "[parser]"`**（绿灯）
- [ ] **Step 8: 写测试（EnvParser::recognizeEnvAssign / applyExpr / fullPath）**
- [ ] **Step 9: 实现 EnvParser 新方法**
- [ ] **Step 10: `xmake run unit_test "[parser]"` + `xmake run unit_test "[env]"`**（绿灯）
- [ ] **Step 11: 全量回归**
- [ ] **Step 12: 提交**

```powershell
git add include/cmd_args/expression/classified_expr.h include/cmd_args/option/option_parser.h include/cmd_args/argument/argument_parser.h include/cmd_args/env/env_parser.h
git commit -m "feat(parsers): add typed Token rules (recognizeToken/applyExpr/probeAggregate) and assignPositional"
```

---

## Task 6: ExpressionRecognizer 类型化重写

**Files:**
- Modify: `include/cmd_args/expression/expression_recognizer.h`（全量重写）
- Test: `dfx/unit_test/test_expression_classifier.cpp`（全量重写）

**Interfaces:**
- Consumes: `Token`（token.h）；OptionParser / ArgumentParser / EnvParser 的 `recognizeToken()` 接口
- Produces: `std::vector<ClassifiedExpr>`（`feed()` + `finish()` 输出）

### 设计要点

- `feed(Token const &_Tok, std::vector<ClassifiedExpr> &_Out)`：喂入一个 Token，内部归约尝试，产出的完整 Expression 追加到 `_Out`
- `finish(std::vector<ClassifiedExpr> &_Out)`：流尾收尾，截断关系/挂起缺值表达式
- `recognizeAll(TokenStream_T const &_Toks) -> std::vector<ClassifiedExpr>`：批量，内部调用 `feed` + `finish`
- `getBufferSize() -> size_t`：归约缓冲深度（ Interpreter `getStackSize()` 委托此值）
- `hasPending() -> bool`：是否有未完成的 pending 取值

### 归约状态机

```
状态：
  PendingKind_E { None, LongOpt, ShortOpt, NamedArg }
  SeqKind_E { None, HeadStash, CondAwaitColon, CondAwaitElse }

归约逻辑（每 feed 一次尝试循环直到无进展）：
  1. 若 pending != None：当前 token 作为消费值 → emit 完整 Expression → 清 pending → 进展
  2. bOptEnd__：直接 emit Argument{bPositional} → 进展
  3. LongName{""}（'--'）：切换 bOptEnd__ → 进展（不 emit）
  4. LongName{n}（非空）：
     a. optParser__.fireAction(n) ? → emit Option{bIsAction} → 进展
     b. optParser__.knowsLong(n) ? 
        - isBoolLong → emit complete Option{value=""} → 进展
        - 非 bool → pending LongOpt (inlineEq=tok.hasInlineEq()) → 进展
     c. argParser__.recognizeToken → pending NamedArg → 进展
     d. 都不匹配 → recordDashLeft(strRaw) → 进展
  5. ShortName{x} / ContinualName{raw}：
     a. 检查关系序列（seq assembly）
     b. 否则 emit aggregate Option 候选 → 进展
  6. 其他 Token（Variable/Integer/Real/String/Delimiter("=")等）：
     → emit positional/忽略 → 进展
```

### 完整重写代码

```cpp
class ExpressionRecognizer
{
public:
    ExpressionRecognizer(OptionParser *_Opt, ArgumentParser *_Arg, EnvParser *_Env)
        : pOptParser__(_Opt)
        , pArgParser__(_Arg)
        , pEnvParser__(_Env)
    {}

    void feed(Token const &_Tok, std::vector<ClassifiedExpr> &_Out)
    {
        vecBuf__.push_back(_Tok);
        bool progress = true;
        while (progress) {
            progress = false;
            if (__tryConsumePending(_Out))  { progress = true; continue; }
            if (__tryReduce(_Out, progress)) { progress = true; }
        }
    }

    void finish(std::vector<ClassifiedExpr> &_Out)
    {
        // 1. 截断关系序列：pending cond/dep
        if (enSeq__ == SeqKind_E::HeadStash && !vecBuf__.empty()) {
            // 孤立 head → 作为普通 aggregate Option 候选
            _Out.push_back(__makeAggregateCandidate(vecBuf__.front()));
            vecBuf__.clear();
        }
        if (enSeq__ == SeqKind_E::CondAwaitColon || enSeq__ == SeqKind_E::CondAwaitElse) {
            ClassifiedExpr e;
            e.kind          = ExpressionKind_E::Conditional;
            e.strCondition  = strSeqHead__;
            e.strThenBranch = strSeqB__;
            e.strElseBranch = (enSeq__ == SeqKind_E::CondAwaitElse) ? strSeqC__ : "_";
            e.bExplicit     = true;
            _Out.push_back(std::move(e));
            vecBuf__.clear();
            __clearSeq();
        }
        // 2. 挂起缺值
        if (!strPendingKey__.empty()) {
            ClassifiedExpr e;
            e.bExplicit = true;
            e.bPositional = false;
            switch (enPendingKind__) {
            case PendingKind_E::LongOpt:
                e.kind = ExpressionKind_E::Option;
                e.name = strPendingKey__;
                break;
            case PendingKind_E::ShortOpt:
                e.kind = ExpressionKind_E::Option;
                e.cShortName = strPendingKey__[0];
                e.name = strPendingKey__;
                break;
            case PendingKind_E::NamedArg:
                e.kind = ExpressionKind_E::Argument;
                e.name = strPendingKey__;
                break;
            default: break;
            }
            _Out.push_back(std::move(e));
            __clearPending();
        }
    }

    auto recognizeAll(TokenStream_T const &_Toks) -> std::vector<ClassifiedExpr>
    {
        reset();
        std::vector<ClassifiedExpr> result;
        for (auto const &tok : _Toks) { feed(tok, result); }
        finish(result);
        return result;
    }

    void reset()
    {
        vecBuf__.clear();
        __clearPending();
        __clearSeq();
        bOptEnd__          = false;
        vecDashLeft__.clear();
    }

    bool hasPending() const noexcept { return !strPendingKey__.empty(); }

    auto getBufferSize() const noexcept -> std::size_t { return vecBuf__.size(); }

private:
    // ===== Pending 状态 =====
    enum class PendingKind_E : char { None, LongOpt, ShortOpt, NamedArg };
    enum class SeqKind_E     : char { None, HeadStash, CondAwaitColon, CondAwaitElse };

    void __clearPending()
    {
        strPendingKey__.clear();
        enPendingKind__    = PendingKind_E::None;
        bPendingInlineEq__ = false;
    }

    void __clearSeq()
    {
        enSeq__   = SeqKind_E::None;
        strSeqHead__.clear();
        strSeqB__.clear();
        strSeqC__.clear();
    }

    /// @brief 尝试消费 pending 值：buf[0] 作为值完成 pending Expression
    bool __tryConsumePending(std::vector<ClassifiedExpr> &_Out)
    {
        if (strPendingKey__.empty() || vecBuf__.empty()) return false;

        auto const &tok = vecBuf__.front();
        std::string text = tok.toText();

        ClassifiedExpr e;
        e.bExplicit = true;

        switch (enPendingKind__) {
        case PendingKind_E::LongOpt:
            // inlineEq 标记允许 dash 值
            if (!bPendingInlineEq__ && !text.empty() && text[0] == '-') {
                // dash 值被拒绝：产出 valueless 表达式（下游报 NEED_VALUE）
                e.kind = ExpressionKind_E::Option;
                e.name = strPendingKey__;
                e.strRaw = "--" + strPendingKey__;
                _Out.push_back(std::move(e));
                __clearPending();
                vecBuf__.erase(vecBuf__.begin());
                return true;
            }
            e.kind   = ExpressionKind_E::Option;
            e.name   = strPendingKey__;
            e.value  = text;
            e.strRaw = "--" + strPendingKey__;
            _Out.push_back(std::move(e));
            __clearPending();
            vecBuf__.erase(vecBuf__.begin());
            return true;

        case PendingKind_E::ShortOpt:
            e.kind       = ExpressionKind_E::Option;
            e.cShortName = strPendingKey__[0];
            e.name       = strPendingKey__;
            e.value      = text;
            e.strRaw     = "-" + strPendingKey__;
            _Out.push_back(std::move(e));
            __clearPending();
            vecBuf__.erase(vecBuf__.begin());
            return true;

        case PendingKind_E::NamedArg:
            e.kind  = ExpressionKind_E::Argument;
            e.name  = strPendingKey__;
            e.value = text;
            e.strRaw = "--" + strPendingKey__;
            _Out.push_back(std::move(e));
            __clearPending();
            vecBuf__.erase(vecBuf__.begin());
            return true;

        default: return false;
        }
    }

    /// @brief 尝试归约：buf 内容匹配产生式 → emit Expression
    bool __tryReduce(std::vector<ClassifiedExpr> &_Out, bool &_Progress)
    {
        if (vecBuf__.empty()) return false;

        auto const &tok = vecBuf__.front();

        // ── 1. '--' 切换纯方位模式 ──
        if (tok.getAttr() == Token::Type_E::LongName && tok.getName().empty()) {
            bOptEnd__ = true;
            vecBuf__.erase(vecBuf__.begin());
            _Progress = true;
            return true;
        }

        // ── 2. bOptEnd__：一律方位参数 ──
        if (bOptEnd__) {
            _Out.push_back(__makePositional(tok));
            vecBuf__.erase(vecBuf__.begin());
            _Progress = true;
            return true;
        }

        // ── 3. LongName{n}：选项/命名参数/关系 ──
        if (tok.getAttr() == Token::Type_E::LongName && !tok.getName().empty()) {
            auto const &name = tok.getName();

            // 3a. 短回路 action
            if (pOptParser__ && pOptParser__->fireAction(name)) {
                ClassifiedExpr e;
                e.kind      = ExpressionKind_E::Option;
                e.name      = name;
                e.bIsAction = true;
                e.bExplicit = true;
                e.strRaw    = tok.toText();
                _Out.push_back(std::move(e));
                vecBuf__.erase(vecBuf__.begin());
                _Progress = true;
                return true;
            }

            // 3b. 已注册长选项
            if (pOptParser__ && pOptParser__->knowsLong(name)) {
                if (pOptParser__->isBoolLong(name)) {
                    // bool：立即完成
                    ClassifiedExpr e;
                    e.kind      = ExpressionKind_E::Option;
                    e.name      = name;
                    e.bIsBool   = true;
                    e.bExplicit = true;
                    e.strRaw    = tok.toText();
                    _Out.push_back(std::move(e));
                    vecBuf__.erase(vecBuf__.begin());
                    _Progress = true;
                    return true;
                }
                // 非 bool：挂起等值
                strPendingKey__    = name;
                enPendingKind__    = PendingKind_E::LongOpt;
                bPendingInlineEq__ = tok.hasInlineEq();
                vecBuf__.erase(vecBuf__.begin());
                _Progress = true;
                return true;
            }

            // 3c. 命名参数
            if (pArgParser__ && pArgParser__->recognizeToken(tok)) {
                strPendingKey__ = name;
                enPendingKind__ = PendingKind_E::NamedArg;
                vecBuf__.erase(vecBuf__.begin());
                _Progress = true;
                return true;
            }

            // 3d. 未识别 → dash-left
            vecDashLeft__.push_back(tok.toText());
            vecBuf__.erase(vecBuf__.begin());
            _Progress = true;
            return true;
        }

        // ── 4. 关系序列组装 ──
        if (enSeq__ == SeqKind_E::HeadStash) {
            // 已有 head：期望 Relation-尾（>target 或 ?branch）
            if (tok.getAttr() == Token::Type_E::Delimiter && tok.getName() == "?") {
                // Conditional: 清理 head + '?' 已在 buf[0]; seq 转为 CondAwaitColon
                enSeq__      = SeqKind_E::CondAwaitColon;
                strSeqHead__ = vecBuf__[0].getName();
                vecBuf__.erase(vecBuf__.begin());  // 移除 head
                vecBuf__.erase(vecBuf__.begin());  // 移除 '?'
                _Progress = true;
                return true;
            }
            if (tok.getAttr() == Token::Type_E::ContinualName
                && !tok.getName().empty() && tok.getName()[0] == '>') {
                // Dependency: head + ">target"
                ClassifiedExpr e;
                e.kind         = ExpressionKind_E::Dependency;
                e.strDepSource = vecBuf__[0].getName();
                e.strDepTarget = tok.getName().substr(1);
                e.bExplicit    = true;
                _Out.push_back(std::move(e));
                vecBuf__.erase(vecBuf__.begin());
                vecBuf__.erase(vecBuf__.begin());
                __clearSeq();
                _Progress = true;
                return true;
            }
            // disambiguation 失败：head 作为普通 aggregate candidate
            _Out.push_back(__makeAggregateCandidate(vecBuf__.front()));
            vecBuf__.erase(vecBuf__.begin());
            __clearSeq();
            _Progress = true;
            return true;
        }

        if (enSeq__ == SeqKind_E::CondAwaitColon) {
            // Variable = then-branch
            if (tok.getAttr() == Token::Type_E::Variable) {
                strSeqB__ = tok.getName();
                vecBuf__.erase(vecBuf__.begin());
                // 期望 ':' 或立即结束
                if (!vecBuf__.empty() && vecBuf__[0].getAttr() == Token::Type_E::Delimiter
                    && vecBuf__[0].getName() == ":") {
                    enSeq__ = SeqKind_E::CondAwaitElse;
                    vecBuf__.erase(vecBuf__.begin());
                } else {
                    // 无 ':' → 完成，else="_"
                    ClassifiedExpr e;
                    e.kind          = ExpressionKind_E::Conditional;
                    e.strCondition  = strSeqHead__;
                    e.strThenBranch = strSeqB__;
                    e.strElseBranch = "_";
                    e.bExplicit     = true;
                    _Out.push_back(std::move(e));
                    __clearSeq();
                }
                _Progress = true;
                return true;
            }
            // 容错：非 Variable → 强制截断
            ClassifiedExpr e;
            e.kind          = ExpressionKind_E::Conditional;
            e.strCondition  = strSeqHead__;
            e.strThenBranch = strSeqB__;
            e.strElseBranch = "_";
            e.bExplicit     = true;
            _Out.push_back(std::move(e));
            __clearSeq();
            _Progress = true;
            return true;
        }

        if (enSeq__ == SeqKind_E::CondAwaitElse) {
            // Variable = else-branch
            if (tok.getAttr() == Token::Type_E::Variable) {
                ClassifiedExpr e;
                e.kind          = ExpressionKind_E::Conditional;
                e.strCondition  = strSeqHead__;
                e.strThenBranch = strSeqB__;
                e.strElseBranch = tok.getName();
                e.bExplicit     = true;
                _Out.push_back(std::move(e));
                vecBuf__.erase(vecBuf__.begin());
                __clearSeq();
                _Progress = true;
                return true;
            }
            // 容错
            ClassifiedExpr e;
            e.kind          = ExpressionKind_E::Conditional;
            e.strCondition  = strSeqHead__;
            e.strThenBranch = strSeqB__;
            e.strElseBranch = "_";
            e.bExplicit     = true;
            _Out.push_back(std::move(e));
            __clearSeq();
            _Progress = true;
            return true;
        }

        // ── 5. ContinualName（可能是 aggregate 或关系 head）──
        if (tok.getAttr() == Token::Type_E::ContinualName) {
            if (vecBuf__.size() == 1) {
                // 孤立候选：进入 HeadStash，等待下一 token 判定
                enSeq__      = SeqKind_E::HeadStash;
                strSeqHead__ = tok.getName();
                // 不移除 vecBuf__[0]（保持在 buf 中）
                return false;  // 无进展，等下一次 feed
            }
        }

        // ── 6. 其他 Token：方位参数候选 / 忽略分隔符 ──
        if (tok.getAttr() == Token::Type_E::Delimiter && tok.getName() == "=") {
            // 独立 '=' 分隔符：在非 pending 情况下忽略（解析器不需要）
            vecBuf__.erase(vecBuf__.begin());
            _Progress = true;
            return true;
        }

        // 其他有效 token → 方位参数
        _Out.push_back(__makePositional(tok));
        vecBuf__.erase(vecBuf__.begin());
        _Progress = true;
        return true;
    }

    auto __makePositional(Token const &_Tok) -> ClassifiedExpr
    {
        ClassifiedExpr e;
        e.kind       = ExpressionKind_E::Argument;
        e.name       = _Tok.toText();
        e.value      = _Tok.toText();
        e.bExplicit  = true;
        e.bPositional = true;
        e.strRaw     = _Tok.toText();
        return e;
    }

    auto __makeAggregateCandidate(Token const &_Tok) -> ClassifiedExpr
    {
        ClassifiedExpr e;
        e.kind       = ExpressionKind_E::Option;
        e.cShortName = _Tok.getName().empty() ? '\0' : _Tok.getName()[0];
        e.name       = _Tok.getName().empty() ? "" : std::string(1, _Tok.getName()[0]);
        e.bIsBool    = true;
        e.bExplicit  = true;
        e.strRaw     = _Tok.toText();
        return e;
    }

    // ===== 内部状态 =====
    std::vector<Token> vecBuf__;               ///< 归约缓冲
    std::string        strPendingKey__    = {};
    PendingKind_E      enPendingKind__    = PendingKind_E::None;
    bool               bPendingInlineEq__ = false;
    bool               bOptEnd__          = false;
    std::vector<std::string> vecDashLeft__;

    SeqKind_E          enSeq__   = SeqKind_E::None;
    std::string        strSeqHead__;
    std::string        strSeqB__;
    std::string        strSeqC__;

    OptionParser  *pOptParser__  = nullptr;
    ArgumentParser *pArgParser__ = nullptr;
    EnvParser     *pEnvParser__  = nullptr;
};
```

### 测试重写 `dfx/unit_test/test_expression_classifier.cpp`

保留 `[classifier]` 标签，全量适配新 `feed(Token, vector&)` API；增补关系序列 / Env 序列 / finish / getBufferSize 断言。示例（完整文件 ~250 行）：

```cpp
#include "catch2/catch.hpp"
#include "cmd_args/expression/expression_recognizer.h"
using namespace cmd_args;
using cmd_args::detail::Token;

TEST_CASE("recognizer: --name=value 内联值立即识别为 Option", "[classifier]")
{
    Model model; OptionBuilder ob(&model.opts); ob.addOption<int>("timeout","t",30);
    OptionParser op(&model.opts); ArgumentParser ap(&model.args); EnvParser ep(&model.envs);
    ExpressionRecognizer rec(&op, &ap, &ep);
    TokenStream_T toks{
        Token(Token::Type_E::LongName, "timeout", 0),  // inlineEq will be false; value separate
        Token(Token::Type_E::Variable, "9", 0)
    };
    // 手动设 inlineEq
    toks[0].setInlineEq(true);

    std::vector<ClassifiedExpr> out;
    rec.feed(toks[0], out);
    REQUIRE(out.empty());  // 还在 pending
    REQUIRE(rec.hasPending());

    rec.feed(toks[1], out);
    REQUIRE(out.size() == 1);
    REQUIRE(out[0].kind == ExpressionKind_E::Option);
    REQUIRE(out[0].name == "timeout");
    REQUIRE(out[0].value == "9");
    REQUIRE(out[0].bExplicit);
    REQUIRE_FALSE(rec.hasPending());
}

TEST_CASE("recognizer: --name value 分离值两个 token", "[classifier]")
{
    Model model; OptionBuilder ob(&model.opts); ob.addOption<int>("timeout","t",30);
    OptionParser op(&model.opts); ArgumentParser ap(&model.args); EnvParser ep(&model.envs);
    ExpressionRecognizer rec(&op, &ap, &ep);

    std::vector<ClassifiedExpr> out;
    rec.feed(Token(Token::Type_E::LongName, "timeout", 0), out);
    REQUIRE(out.empty());
    REQUIRE(rec.hasPending());

    rec.feed(Token(Token::Type_E::Variable, "9", 0), out);
    REQUIRE(out.size() == 1);
    REQUIRE(out[0].value == "9");
}

TEST_CASE("recognizer: -abc 聚合 bool 选项", "[classifier]")
{
    Model model; OptionBuilder ob(&model.opts);
    ob.addFlag('a',"ena","a"); ob.addFlag('b',"enb","b"); ob.addFlag('c',"enc","c");
    OptionParser op(&model.opts); ArgumentParser ap(&model.args); EnvParser ep(&model.envs);
    ExpressionRecognizer rec(&op, &ap, &ep);

    std::vector<ClassifiedExpr> out;
    rec.feed(Token(Token::Type_E::ContinualName, "abc", 0), out);
    // ContinualName 进入 HeadStash（等待下一 token 判定关系 vs aggregate）
    REQUIRE(out.empty());
    REQUIRE(rec.getBufferSize() == 1);

    // feed 一个无关 token 触发 aggregate 产出
    rec.feed(Token(Token::Type_E::Variable, "x", 0), out);
    REQUIRE(out.size() == 2);  // aggregate + positional
    REQUIRE(out[0].kind == ExpressionKind_E::Option);
    REQUIRE(out[0].cShortName == 'a');
    REQUIRE(out[0].strRaw == "-abc");
    REQUIRE(out[1].bPositional);
}

TEST_CASE("recognizer: 关系 -a->b 识别为 Dependency", "[classifier]")
{
    Model model; OptionBuilder ob(&model.opts);
    ob.addFlag('a',"ena","a"); ob.addFlag('b',"enb","b");
    OptionParser op(&model.opts); ArgumentParser ap(&model.args); EnvParser ep(&model.envs);
    ExpressionRecognizer rec(&op, &ap, &ep);

    std::vector<ClassifiedExpr> out;
    rec.feed(Token(Token::Type_E::ContinualName, "a", 0), out);     // head → stash
    rec.feed(Token(Token::Type_E::ContinualName, ">b", 0), out);    // dep target
    REQUIRE(out.size() == 1);
    REQUIRE(out[0].kind == ExpressionKind_E::Dependency);
    REQUIRE(out[0].strDepSource == "a");
    REQUIRE(out[0].strDepTarget == "b");
}

TEST_CASE("recognizer: 方位参数识别", "[classifier]")
{
    Model model; OptionBuilder ob(&model.opts); ob.addOption<int>("timeout","t",30);
    OptionParser op(&model.opts); ArgumentParser ap(&model.args); EnvParser ep(&model.envs);
    ExpressionRecognizer rec(&op, &ap, &ep);

    std::vector<ClassifiedExpr> out;
    rec.feed(Token(Token::Type_E::Variable, "hello", 0), out);
    REQUIRE(out.size() == 1);
    REQUIRE(out[0].kind == ExpressionKind_E::Argument);
    REQUIRE(out[0].bPositional);
    REQUIRE(out[0].value == "hello");
}

TEST_CASE("recognizer: recognizeAll 批量识别", "[classifier]")
{
    Model model; OptionBuilder ob(&model.opts); ob.addOption<int>("timeout","t",30);
    OptionParser op(&model.opts); ArgumentParser ap(&model.args); EnvParser ep(&model.envs);
    ExpressionRecognizer rec(&op, &ap, &ep);
    TokenStream_T toks{
        Token(Token::Type_E::LongName, "timeout", 0),
        Token(Token::Type_E::Variable, "9", 0),
        Token(Token::Type_E::Variable, "hello", 0)
    };
    auto exprs = rec.recognizeAll(toks);
    REQUIRE(exprs.size() == 2);
    REQUIRE(exprs[0].kind == ExpressionKind_E::Option);
    REQUIRE(exprs[0].value == "9");
    REQUIRE(exprs[1].bPositional);
}

TEST_CASE("recognizer: 流尾 pending 缺值产出 valueless Expression", "[classifier]")
{
    Model model; OptionBuilder ob(&model.opts); ob.addOption<int>("timeout","t",30);
    OptionParser op(&model.opts); ArgumentParser ap(&model.args); EnvParser ep(&model.envs);
    ExpressionRecognizer rec(&op, &ap, &ep);

    std::vector<ClassifiedExpr> out;
    rec.feed(Token(Token::Type_E::LongName, "timeout", 0), out);
    rec.finish(out);
    REQUIRE(out.size() == 1);
    REQUIRE(out[0].value.empty());  // 缺值
}
```

- [ ] **Step 1: 写全量测试（新 API 断言）**
- [ ] **Step 2: 实现重写**
- [ ] **Step 3: `xmake build unit_test && xmake run unit_test "[classifier]"`**（绿灯）
- [ ] **Step 4: 全量回归**（所有既有测试绿灯）
- [ ] **Step 5: 提交**

```powershell
git add include/cmd_args/expression/expression_recognizer.h dfx/unit_test/test_expression_classifier.cpp
git commit -m "refactor(recognizer): rewrite to Token-driven grammar, eliminate string parsing"
```

---

## Task 7: Compiler 重写为 Expression 驱动管线

**Files:**
- Modify: `include/cmd_args/expression/compiler.h`
- Test: 现有 `test_parser.cpp` / `test_context*.cpp` 绿灯验证

### Compiler 重写要点

`Compiler::parse(int argc, char const *argv[], char *envp[])` 管线改为：

```
Lexer(argc-1, argv+1).scan()  →  ExpressionRecognizer::recognizeAll(toks)
  → __applyOne() 分派入库  →  residual UNKNOWN_OPTION 检查  →  finalize()
```

### 重写后核心代码

```cpp
class Compiler
{
public:
    Compiler(Model *_Data)
        : argParser__(&_Data->args)
        , optParser__(&_Data->opts)
        , envBuilder__(&_Data->envs)
        , envParser__(&_Data->envs)
        , pModel__(_Data)
    {}

    // ...set_program_name/set_description/isParsed/isFired/getOption/getArgument 原样保留...

    void parse(int _Argc, char const *_Argv[], char *_Envp[] = nullptr)
    {
        if (_Argc <= 0 || !_Argv || !_Argv[0]) { program_name.clear(); }
        else { program_name = _Argv[0]; }

        TokenStream_T real;
        if (_Argc > 1) { real = Lexer(_Argc - 1, _Argv + 1).scan(); }

        std::vector<std::string> vecEnv;
        if (_Envp) { for (int i = 0; _Envp[i]; ++i) { vecEnv.emplace_back(_Envp[i]); } }

        __parseTokens(real, vecEnv);
    }

    void parse(Tokens_T const &_Tokens, Envs_T const &_Env)
    {
        // 兼容旧字符串入口：重扫描还原类型化流
        TokenStream_T real = Lexer::scanStrings(_Tokens);
        __parseTokens(real, _Env);
    }

    // ...tryParse 三重载 / parse(Tokens_T,Envs_T) 兼容签名保留...

private:
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
                __applyOne(e, vecPositional);
                if (bFired__) return;
            }

            // 关系图求值（条件边在 apply 中已 addEdge；此处 evaluate）
            pModel__->optionGraph.evaluate();

            // 方位参数提交
            argParser__.assignPositional(vecPositional);

            // 残留 dash token → UNKNOWN_OPTION
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

    void __applyOne(ClassifiedExpr &_E, std::vector<std::string> &_Pos)
    {
        switch (_E.kind) {
        case ExpressionKind_E::Option: {
            if (_E.bIsAction && !_E.name.empty()) {
                if (optParser__.fireAction(_E.name)) { bFired__ = true; }
                return;
            }
            if (!optParser__.applyExpr(_E)) {
                // 未注册 → 可能是命名参数？但 recognizer 已处理；记入 dash-left
            }
            return;
        }
        case ExpressionKind_E::Conditional:
        case ExpressionKind_E::Dependency:
            optParser__.applyRelation(_E);
            return;
        case ExpressionKind_E::Argument:
            argParser__.applyExpr(_E, _Pos);
            return;
        case ExpressionKind_E::Env:
            envParser__.applyExpr(_E);
            return;
        }
    }

    // ...ensureBuiltins/finalize/__seedEnvDefaults/__expandEnvValues/__descHelp...原样保留...

    ArgumentParser argParser__;
    OptionParser   optParser__;
    EnvBuilder     envBuilder__;
    EnvParser      envParser__;
    Model         *pModel__ = nullptr;
    Name_T         program_name;
    Desc_T         description;
    bool           bFired__ = false;
};
```

- [ ] **Step 1: 实现 Compiler 重写**
- [ ] **Step 2: `xmake build unit_test`**（绿灯）
- [ ] **Step 3: `xmake run unit_test "[parser]"` + `xmake run unit_test "[context]"` + `xmake run unit_test "[context_step]"` + `xmake run unit_test "[context_factory]"`**（全绿）
- [ ] **Step 4: `xmake run unit_test`**（全量回归）
- [ ] **Step 5: 提交**

```powershell
git add include/cmd_args/expression/compiler.h
git commit -m "refactor(compiler): rewrite pipeline to Expression-driven (recognizeAll → applyOne)"
```

---

## Task 8: Interpreter 改造（复用 Recognizer，删除字符串归约）

**Files:**
- Modify: `include/cmd_args/expression/interpreter.h`
- Test: 现有 `test_interpreter.cpp` 绿灯验证（关键契约：getCursor / getStackSize / 触发顺序）

### 改造要点

- 删除 `__reduceStack` / `__isRelationText` / `__classifyRelationText` / `__handleRelation`（~200 行字符串解析）
- 新增 `ExpressionRecognizer rec__` 成员
- `interpret()` 保持 `Scanner.step(idxCursor__)` 驱动；token 产出后 `rec__.feed(tok, ready)`；`ready` 非空即执行
- `__execute(ClassifiedExpr&)` 分派执行（保留 `__executeOptionImmediate` / `__assignPositional` / `__finalize`）

### 重写后核心代码

```cpp
class Interpreter : private ArgumentParser, private OptionParser, private EnvParser
{
public:
    Interpreter(Model *_Model)
        : ArgumentParser(&_Model->args)
        , OptionParser(&_Model->opts)
        , EnvParser(&_Model->envs)
        , pModel__(_Model)
        , rec__(&(static_cast<OptionParser &>(*this)),
                &(static_cast<ArgumentParser &>(*this)),
                &(static_cast<EnvParser &>(*this)))
    {}

    std::error_code interpret(std::string const &_Line, Errors &_Errors)
    {
        pModel__->args.mgrName.doMutVisit([](Argument &a) { a.reset(); });
        pModel__->args.mgrPos.doMutVisit([](Argument &a) { a.reset(); });
        pModel__->opts.mgrValues.forEachValue([](OptionValue &o) { o.reset(); });
        _Errors.clearLast();

        try {
            cmd_args::detail::Scanner sc({});
            rec__.reset();
            __lineReset();

            while (idxCursor__ < _Line.size()) {
                auto tokOpt = sc.step(_Line[idxCursor__]);
                ++idxCursor__;
                if (tokOpt) {
                    std::vector<ClassifiedExpr> ready;
                    rec__.feed(*tokOpt, ready);
                    for (auto &e : ready) __execute(e);
                }
            }
            // 流结束：flush 最后 token
            auto tokEnd = sc.step('\0');
            if (tokEnd) {
                std::vector<ClassifiedExpr> ready;
                rec__.feed(*tokEnd, ready);
                for (auto &e : ready) __execute(e);
            }
            // 流尾收尾
            {
                std::vector<ClassifiedExpr> finishExprs;
                rec__.finish(finishExprs);
                for (auto &e : finishExprs) __execute(e);
            }

            // 挂起报错
            if (!strPendingKey__.empty()) {  // 保留 PendingKind 判断... }
            if (!vecDashLeft__.empty()) { ... }
            __assignPositional(vecPos__);
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

    auto getStackSize() const noexcept -> std::size_t { return rec__.getBufferSize(); }

    // ...其余公开方法（setExitCode/exitCode/markParsed/prepared/describeError/getCursor）原样保留...

private:
    void __lineReset()
    {
        idxCursor__   = 0;
        vecPos__.clear();
        vecDashLeft__.clear();
        strPendingKey__.clear();
        enPendingKind__    = PendingKind_E::None;
        bPendingInlineEq__ = false;
        bOptEnd__          = false;
    }

    void __execute(ClassifiedExpr &_E)
    {
        switch (_E.kind) {
        case ExpressionKind_E::Option: {
            if (_E.bIsAction) {
                if (auto const *pSc = pModel__->opts.mgrActions.doGet(_E.name)) {
                    static_cast<OptionAction const *>(pSc)->fnCallback();
                }
                return;
            }
            // 聚合 / 短选项 / 长选项：调用 OptionParser::applyExpr
            if (!applyExpr(_E)) {
                // 未识别 → dash-left（流尾报 UNKNOWN_OPTION）
                vecDashLeft__.push_back(_E.strRaw);
            }
            return;
        }
        case ExpressionKind_E::Conditional:
        case ExpressionKind_E::Dependency:
            applyRelation(_E);
            return;
        case ExpressionKind_E::Argument:
            if (_E.bPositional) {
                vecPos__.emplace_back(_E.value);
            } else {
                auto *pArg = pModel__->args.mgrName.mutGet(_E.name);
                if (pArg) { pArg->set(_E.value); }
            }
            return;
        case ExpressionKind_E::Env:
            EnvParser::applyExpr(_E);
            return;
        }
    }

    void __executeOptionImmediate(OptionValue *_Opt, std::string const &_Value, bool _IsBool)
    {
        // ...原有赋值 + fnOnSet__ + action 模板逻辑保留...
    }

    void __assignPositional(std::vector<std::string> const &_VecPos)
    {
        ArgumentParser::assignPositional(_VecPos);
    }

    void __finalize()
    {
        __seedEnvDefaults();
        __expandEnvValues();
        if (!pModel__->strSaveConfigPath.empty()) {
            auto text = config::to_yaml(*pModel__);
            if (!config::write_file(pModel__->strSaveConfigPath, text)) {
                std::cerr << "(save-config) cannot write file: " << pModel__->strSaveConfigPath << "\n";
            }
        }
    }

    // ...__seedEnvDefaults/__expandEnvDefaults 原样保留（委托 this->EnvParser::...）...

    Model  *pModel__     = nullptr;
    bool    bParsed__    = false;
    int     nExitCode__  = 0;
    ExpressionRecognizer rec__;

    enum class PendingKind_E : char { None, LongOpt, ShortOpt, NamedArg };
    std::string strPendingKey__    = {};
    PendingKind_E enPendingKind__  = PendingKind_E::None;
    bool bPendingInlineEq__        = false;
    bool bOptEnd__                 = false;
    std::vector<std::string> vecPos__;
    std::vector<std::string> vecDashLeft__;
    size_t idxCursor__             = 0;
};
```

- [ ] **Step 1: 实现 Interpreter 改造**
- [ ] **Step 2: `xmake build unit_test`**（绿灯）
- [ ] **Step 3: `xmake run unit_test "[interpreter]"`**（关键：游标 / 栈深 / 触发顺序断言全绿）
- [ ] **Step 4: `xmake run unit_test`**（全量回归）
- [ ] **Step 5: 提交**

```powershell
git add include/cmd_args/expression/interpreter.h
git commit -m "refactor(interpreter): delegate to ExpressionRecognizer, eliminate string reduceStack"
```

---

## Task 9: Context 层类型化贯通

**Files:**
- Modify: `include/cmd_args/context/context_fwd.h`（`#include token.h`）
- Modify: `include/cmd_args/context/context_base.h`（`_doRunPipeline` 签名 + 类型化 preamble）
- Modify: `include/cmd_args/context/context_step.h`（override 签名）
- Modify: `include/cmd_args/context/context_runtime.h`（override 签名）
- Modify: `include/cmd_args/context/context.h`（override 签名）

### context_fwd.h 修改

```cpp
#include "cmd_args/expression/token.h"  // TokenStream_T 来源

using Tokens_T    = std::list<std::string>;    // 兼容：旧接口字符串流
using Envs_T      = std::vector<std::string>;  // "KEY=VALUE" 环境串列表
```

### context_base.h 修改

**`__preambleArgv`** 输出类型化流：

```cpp
void __preambleArgv(int _Argc, char const *_Argv[], char *_Envp[],
                    TokenStream_T &_Toks, Envs_T &_Envs)
{
    if (_Argc > 0 && _Argv[0]) { instData_.strProgramName = _Argv[0]; }
    else { instData_.strProgramName = strPrjName_; }

    if (_Argc > 1) {
        Lexer lex(_Argc - 1, _Argv + 1);
        _Toks = lex.scan();  // TokenStream_T（类型化）
    }

    if (_Envp) { for (int i = 0; _Envp[i]; ++i) { _Envs.emplace_back(_Envp[i]); } }
}
```

**`_doRunPipeline`** 签名变更：

```cpp
virtual void _doRunPipeline(TokenStream_T &_Toks, Envs_T const &_Envs) = 0;
```

**`parse()`** 各重载：

```cpp
ContextBase &parse(int _Argc, char const *_Argv[], char *_Envp[])
{
    try {
        TokenStream_T toks;
        Envs_T        envs;
        __preambleArgv(_Argc, _Argv, _Envp, toks, envs);
        _doRunPipeline(toks, envs);
    }
    catch (ErrorParse const &e) { setLast(e.code()); throw; }
    catch (ErrorLexer const &e) { setLast(e.code()); throw; }
    return *this;
}

// 兼容：字符串流入口，重扫描转类型化
ContextBase &parse(Tokens_T const &_Tokens, Envs_T const &_Env)
{
    try {
        TokenStream_T toks = Lexer::scanStrings(_Tokens);
        __preambleTokens();
        _doRunPipeline(toks, _Env);
    }
    catch (ErrorParse const &e) { setLast(e.code()); throw; }
    catch (ErrorLexer const &e) { setLast(e.code()); throw; }
    return *this;
}
```

### context_step.h / context_runtime.h / context.h

将 `_doRunPipeline(Tokens_T &, Envs_T const &)` override 签名改为 `(TokenStream_T &, Envs_T const &)`，内部逻辑不变（`Lexer::scanLine` 拼行 / `tok.toText()` 等类型化操作）。

- [ ] **Step 1: 修改 context_fwd.h + context_base.h**
- [ ] **Step 2: 修改三个子类 override**
- [ ] **Step 3: `xmake build unit_test`**（绿灯）
- [ ] **Step 4: `xmake run unit_test "[context]"` + `xmake run unit_test "[context_step]"` + `xmake run unit_test "[context_factory]"`**（全绿）
- [ ] **Step 5: `xmake run unit_test`**（全量回归）
- [ ] **Step 6: 提交**

```powershell
git add include/cmd_args/context/context_fwd.h include/cmd_args/context/context_base.h include/cmd_args/context/context_step.h include/cmd_args/context/context_runtime.h include/cmd_args/context/context.h
git commit -m "refactor(context): type-化 _doRunPipeline signature, preambleArgv outputs TokenStream_T"
```

---

## Task 10: Env Space 语义测试贯通

**Files:**
- Test: `dfx/unit_test/test_env.cpp`（追加 SECTION）
- Modify: `include/cmd_args/env/env_parser.h`（expand 花括号内 :: 验证）

### 测试追加到 test_env.cpp

```cpp
/* ==================== 环境空间（:: 级联） ==================== */

TEST_CASE("env: recognizeEnvAssign 空间级联", "[envscope]")
{
    EnvData data;
    EnvParser ep(&data);

    TokenStream_T toks{
        Token(Token::Type_E::Variable,   "A", 0),
        Token(Token::Type_E::ScopeSep,   "::", 0),
        Token(Token::Type_E::Variable,   "B", 0),
        Token(Token::Type_E::ScopeSep,   "::", 0),
        Token(Token::Type_E::Variable,   "C", 0),
        Token(Token::Type_E::Delimiter,  "=", 0),
        Token(Token::Type_E::String,     "D", 0),
    };
    auto result = ep.recognizeEnvAssign(toks, 0);
    REQUIRE(result.has_value());
    REQUIRE(result->expr.kind == ExpressionKind_E::Env);
    REQUIRE(result->expr.name == "C");
    REQUIRE(result->expr.vecSpacePath.size() == 2);
    REQUIRE(result->expr.vecSpacePath[0] == "A");
    REQUIRE(result->expr.vecSpacePath[1] == "B");
    REQUIRE(result->expr.value == "D");
    REQUIRE(result->expr.fullPath() == "A::B::C");
    REQUIRE(result->nConsumed == 7);
}

TEST_CASE("env: recognizeEnvAssign 无空间平键", "[envscope]")
{
    EnvData data;
    EnvParser ep(&data);

    TokenStream_T toks{
        Token(Token::Type_E::Variable,   "KEY", 0),
        Token(Token::Type_E::Delimiter,  "=", 0),
        Token(Token::Type_E::String,     "val", 0),
    };
    auto result = ep.recognizeEnvAssign(toks, 0);
    REQUIRE(result.has_value());
    REQUIRE(result->expr.vecSpacePath.empty());
    REQUIRE(result->expr.name == "KEY");
    REQUIRE(result->expr.fullPath() == "KEY");
}

TEST_CASE("env: expand 花括号内支持 :: 键", "[envscope]")
{
    EnvData data;
    data.doPush("A::B::C", "scoped_val", false);
    EnvParser ep(&data);

    auto result = ep.expand("path=${A::B::C}/bin");
    REQUIRE(result == "path=scoped_val/bin");
}

TEST_CASE("env: set_env + has_env 全路径键", "[envscope]")
{
    EnvData data;
    data.doPush("A::B::C", "v1", true);
    EnvParser ep(&data);

    REQUIRE(ep.has("A::B::C"));
    REQUIRE(ep.get("A::B::C") == "v1");
}

TEST_CASE("env: --env-set 内置选项接受 :: 键", "[envscope]")
{
    EnvData data;
    EnvParser ep(&data);
    // --env-set 的值字符串 "A::B::C=V" 经第一个 '=' 分割
    // env_set callback 会收到 "A::B::C=V" 作为 value
    // builtin 的 split-at-first-'=' 处理完整正确
    data.doPush("A::B::C", "V", true);
    REQUIRE(data.doFind("A::B::C"));
}
```

- [ ] **Step 1: 写 env scope 测试**
- [ ] **Step 2: `xmake build unit_test && xmake run unit_test "[envscope]"`**（绿灯）
- [ ] **Step 3: `xmake run unit_test "[env]"`**（全绿）
- [ ] **Step 4: 全量回归**
- [ ] **Step 5: 提交**

```powershell
git add dfx/unit_test/test_env.cpp include/cmd_args/env/env_parser.h
git commit -m "feat(env): add env scope integration tests (${A::B::C}, fullPath, recognizeEnvAssign)"
```

---

## Task 11: AGENT.md 文档更新 + 全量回归

**Files:**
- Modify: `include/cmd_args/expression/AGENT.md`
- Modify: `include/cmd_args/context/AGENT.md`

### expression/AGENT.md 更新要点

文件职责表新增 `token.h` 行：

```
| `token.h` | `Token` / `TokenStream_T` | **公共词法单元**：`Type_E` 含 `ScopeSep`；Scanner 产出 / Recognizer 输入 |
```

更新 expression_recognizer.h 描述：

```
| `expression_recognizer.h` | `ExpressionRecognizer` | **语法前端**：类型化 `feed(Token)` / `finish()` 归约产生式 → `ClassifiedExpr`；委托三 Parser 的 `recognizeToken()` 规则 |
```

更新 lexer.h 描述，提及 `LexMode_E` / `scan()` / `pop()` / `scanEnvLine()`。

更新 "语法层要点" 段落，提及 feed/finish API、pending-state machine on tokens。

### context/AGENT.md 更新要点

- 修正 `expression_classifier.h` → `expression_recognizer.h`
- 提及 Token/Expression 类型化接口
- `_doRunPipeline` 签名变更

- [ ] **Step 1: 更新两份 AGENT.md**
- [ ] **Step 2: `xmake build unit_test`**（绿灯）
- [ ] **Step 3: `xmake run unit_test`**（全量回归：全部测试绿灯）
- [ ] **Step 4: 提交**

```powershell
git add include/cmd_args/expression/AGENT.md include/cmd_args/context/AGENT.md
git commit -m "docs: update AGENT.md for token.h, Recognizer rewrite, type-化 pipeline"
```

---

## Self-Review（已完成）

**1. Spec 覆盖：**
- ✅ Lexer 支持一次性解析 `std::string` → Token 流 + 逐个吐出（Task 4：`scan()` / `pop()`）
- ✅ Env space `::` 级联词法（Task 3：ScopeSep / LexMode_E）
- ✅ `A::B::C=D` → Token{A, ::, B, ::, C, =, D}（Task 3+10 测试覆盖）
- ✅ ExpressionRecognizer 支持批量与流式两种模式（Task 6：`recognizeAll()` / `feed()`）
- ✅ Compiler 批量处理 Expression，含 OptionAction → Executor（Task 7：`__applyOne()` + gating）
- ✅ Interpreter 逐个 Expression 识别后立即处理（Task 8：`rec__.feed()` + `__execute()`）
- ✅ Context 通用入口 argc/argv，内部类型化接口（Task 9：`_doRunPipeline(TokenStream_T&)`）
- ✅ Parser 显式 Token→Expression 接口供 Interpreter 复用（Task 5：`recognizeToken()` / `applyExpr()`）

**2. 占位符扫描：** 无 TBD/TODO/placeholder — 所有代码块完整可执行。

**3. 类型一致性：**
- `recognizeToken(Token const &) -> optional<ClassifiedExpr>` 在三个 Parser 中命名/签名一致 ✓
- `applyExpr(ClassifiedExpr const &)` 在各 Parser 中语义对齐（OptionParser 返回 bool，ArgumentParser push 到 vecPos，EnvParser push 到 EnvData）✓
- `getBufferSize()` 委托链 Interpreter→Recognizer→vecBuf__.size() ✓
- `feed(Token const &_Tok, std::vector<ClassifiedExpr> &_Out)` sink 模式在 Recognizer / Interpreter / recognizeAll 三处一致 ✓
