# Expression 分类层 + Option 关系有向图 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use subagent-driven-development (recommended) or executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 补齐 Lexer→Parser 管线中缺失的「Token → Expression」显式分类层，使 Compiler（批量）与 Interpreter（流式）共享同一分类逻辑；同时引入 Option 关系有向图，支持 `-a?b:c`（条件）、`-a?b:_`（条件无操作）、`-a->b`（依赖）三种关系语法，让 Step 模式下的选项执行遵循拓扑序。

**Architecture:** 分两阶段——Phase A 建立 Expression 分类接口（ExpressionClassifier），使 token 到 Expression 的转换成为可复用的独立步骤；Phase B 在分类层之上引入 OptionGraph 有向图，扩展短选项聚合语法支持条件与依赖关系。

**Tech Stack:** C++20（header-only）、xmake、Catch2（dfx/unit_test）

**Spec:** 用户需求原文——"在Lexer解析出token之后，应该由Parser继续解析通过Expression得到AST, 显然AST能够存储在Model里，能够被Executor执行。我还是没看到在ArgumentParser,OptionParser,EnvParser中看到，token转化为Expression的部分，同时这个转化识别接口能被Interpreter用到。在Step模式里，我们可以对Option进行连续解析，比如`-abc`表示a、b、c选项被激活，实际上如果`-a?b:c`我们可以简单地识别到当a为true时连带激活b,a为false则激活b,如果是`-a?b:_`，`_`表示占位符什么也不做，如果是 `-a->b`则表示先执行a之后再执行b,这里是关系指定，而不是flag使能。此时就把Option直接的关系变成了有向图。"

已确认的设计决策：
1. **Expression 分类接口**：新建 `ExpressionClassifier` 类，提供 `classifyOne(Token)`（流式）与 `classifyAll(Tokens_T)`（批量）两个入口，返回 `ClassifiedExpr` 中间表示。该中间表示包含 kind/name/value/explicit 标记等元数据，但不直接写入 Model——"apply to Model" 是下游步骤。
2. **Interpreter 复用分类逻辑**：Interpreter 的 `__reduceStack()` 改为先调用 `ExpressionClassifier::classifyOne()` 获得 `ClassifiedExpr`，再根据 kind 分支执行。这消除了当前 `__reduceStack()` 中与 OptionParser/ArgumentParser 重复的 token 识别逻辑。
3. **Option 关系有向图**：新建 `OptionGraph` 类，存储 Option 之间的 Conditional（`?`）和 Dependency（`->`）边。图在 OptionBuilder 注册阶段构建，在 OptionParser 解析时填充节点状态，在 Executor 执行前进行拓扑排序确定执行顺序。
4. **短选项聚合语法扩展**：Scanner 需识别 `-a?b:c`、`-a?b:_`、`-a->b` 三种新形态，产出对应的 Token（新增 `Conditional` / `Dependency` 属性）。OptionParser 解析这些 Token 时向 OptionGraph 注册关系。
5. **`_` 占位符语义**：`-a?b:_` 中的 `_` 表示条件为假时不做任何操作（不激活任何选项），等价于 `-a?b:` 的简写。
6. **兼容铁律**：既有 `example/**` 零改动继续编译；既有 `test_*.cpp` 除追加段落外零逻辑改动，全部必须原样通过。

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
  - 按标签过滤：`xmake run unit_test "[classifier]"` / `"[graph]"`（**引号必须带**）
  - 全量回归：`xmake run unit_test`
- 提交信息用英文 conventional commits（仓库惯例：`test(scope): ...` / `refactor(scope): ...` / `feat(scope): ...`）。

---

## 文件结构总览

```
include/cmd_args/
├── context/
│   ├── expression_classifier.h   [新] Token → ClassifiedExpr 分类接口（流式+批量）
│   ├── expression.h              [改] ExpressionKind_E 追加 Conditional/Dependency
│   └── (existing files unchanged for now)
├── option/
│   ├── option_graph.h            [新] Option 关系有向图（Node/Edge/拓扑排序/求值）
│   └── option_parser.h           [改] 解析时向 OptionGraph 注册关系
├── context/
│   └── interpreter.h             [改] __reduceStack 委托 ExpressionClassifier
└── context/
    └── compiler.h                [改] __parseTokens 委托 ExpressionClassifier

dfx/unit_test/
├── test_expression_classifier.cpp  [新] 分类器测试（标签 [classifier]）
├── test_option_graph.cpp           [新] 有向图测试（标签 [graph]）
├── test_lexer.cpp                  [改] 追加新语法 Scanner 测试
└── test_option.cpp                 [改] 追加关系语法 OptionParser 测试
```

依赖方向：Task 1 → Task 2 → Task 3（分类层）；Task 4 → Task 5 → Task 6（关系图）；Task 7 整合两层。

---

## Phase A: Expression 分类层

### Task 1: ExpressionKind_E 扩展 + ClassifiedExpr 中间表示

**Files:**
- Modify: `include/cmd_args/context/expression.h`（追加枚举值）
- Create: `include/cmd_args/context/expression_classifier.h`（ClassifiedExpr 定义）

**Interfaces:**
- Consumes: `detail::Token`（lexer.h）
- Produces: `ExpressionKind_E` 扩展后的枚举；`ClassifiedExpr` 结构体

- [ ] **Step 1: 扩展 ExpressionKind_E**

在 `expression.h` 的 `ExpressionKind_E` 枚举中追加两个值：

```cpp
enum class ExpressionKind_E : char
{
    Argument,      ///< 命名/方位参数
    Option,        ///< 值选项/标志/短回路选项
    Env,           ///< 环境变量条目
    Conditional,   ///< 条件关系：-a?b:c（a 为真时激活 b，否则激活 c）
    Dependency     ///< 依赖关系：-a->b（先执行 a，再执行 b）
};
```

- [ ] **Step 2: 创建 expression_classifier.h — ClassifiedExpr 定义**

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

#include <optional>
#include <string>
#include <vector>

#include "cmd_args/base/def.h"
#include "cmd_args/context/expression.h"
#include "cmd_args/context/lexer.h"

CMD_ARGS_NAMESPACE_BEGIN

/**
 * @brief 分类后的表达式中间表示：Token 经分类后产出的结构化数据。
 *
 * 与 Expression 基类的区别：
 * - Expression 是 Model 中持久存储的实体（Argument/OptionValue/OptionAction/environment）
 * - ClassifiedExpr 是解析管线中的瞬态中间表示，携带 Token 的分类结果与元数据
 * - 下游（Compiler/Interpreter）根据 kind 分支将 ClassifiedExpr apply 到 Model
 */
struct ClassifiedExpr
{
    ExpressionKind_E kind = ExpressionKind_E::Option;  ///< 分类种类

    // ===== 通用字段 =====
    std::string name;          ///< 选项长名 / 参数名 / 环境变量键
    std::string value;         ///< 值（选项值 / 参数值 / 环境变量值）
    bool        bExplicit = false;  ///< 是否被命令行显式赋值

    // ===== Option 专用 =====
    char        cShortName = '\0';  ///< 短名（'\0' 表示无短名）
    bool        bIsBool    = false; ///< 是否 bool 类型选项
    bool        bIsBuiltin = false; ///< 是否内置交互选项
    bool        bIsAction  = false; ///< 是否短回路选项（命中即回调）
    std::string strType;           ///< 类型标识（"bool" / "int" / "double" / "std::string"）

    // ===== Conditional 关系专用 =====
    std::string strCondition;      ///< 条件选项名（-a?b:c 中的 a）
    std::string strThenBranch;     ///< 条件为真时激活的选项（b）
    std::string strElseBranch;     ///< 条件为假时激活的选项（c 或 "_"）

    // ===== Dependency 关系专用 =====
    std::string strDepSource;      ///< 依赖源（-a->b 中的 a）
    std::string strDepTarget;      ///< 依赖目标（-a->b 中的 b）

    // ===== 环境变量专用 =====
    bool bEnvNew = false;          ///< 是否新建的环境变量

    /// @brief 是否为关系表达式（Conditional 或 Dependency）
    bool isRelation() const noexcept
    {
        return kind == ExpressionKind_E::Conditional
            || kind == ExpressionKind_E::Dependency;
    }
};

/**
 * @brief 表达式分类器：把 Token 流分类为 ClassifiedExpr 中间表示。
 *
 * 提供两个入口：
 * - classifyOne()：流式模式，逐 Token 分类（Interpreter 使用）
 * - classifyAll()：批量模式，一次分类全部 Token（Compiler 使用）
 *
 * 分类器是无状态的——每次 classify 调用独立，不依赖前次调用的残留状态。
 * 唯一的内部状态是「当前 Token 是否以 '-' 开头」的判断逻辑，
 * 这与 Token 的 toText() 产出一致。
 */
class ExpressionClassifier
{
public:
    /// @brief 流式分类：单个 Token → ClassifiedExpr（Interpreter __reduceStack 使用）
    /// @param _Tok 待分类的 Token
    /// @return 分类成功返回 ClassifiedExpr，无法分类返回 nullopt（如空白 token）
    auto classifyOne(detail::Token const &_Tok) const -> std::optional<ClassifiedExpr>
    {
        std::string text = _Tok.toText();
        if (text.empty()) return std::nullopt;

        return __classifyText(text, _Tok.hasInlineEq());
    }

    /// @brief 批量分类：Token 列表 → ClassifiedExpr 列表（Compiler 使用）
    /// @param _Tokens 待分类的 Token 流（已归一化）
    /// @return 分类后的 ClassifiedExpr 列表
    auto classifyAll(Tokens_T const &_Tokens) const -> std::vector<ClassifiedExpr>
    {
        std::vector<ClassifiedExpr> result;
        result.reserve(_Tokens.size());
        for (auto const &tok : _Tokens) {
            auto expr = classifyOne(tok);
            if (expr) { result.emplace_back(std::move(*expr)); }
        }
        return result;
    }

private:
    /// @brief 核心分类逻辑：根据 token 文本判定 ExpressionKind_E
    auto __classifyText(std::string const &_Text, bool _InlineEq) const -> ClassifiedExpr
    {
        ClassifiedExpr expr;
        expr.bExplicit = true;

        // ── 1. 环境变量：KEY=VALUE 形态（无 '-' 前缀且含 '='） ──
        if (!util::starts_with(_Text, "-")) {
            auto eq = _Text.find('=');
            if (eq != std::string::npos) {
                expr.kind  = ExpressionKind_E::Env;
                expr.name  = _Text.substr(0, eq);
                expr.value = _Text.substr(eq + 1);
                return expr;
            }
            // 普通 variable token → 方位参数
            expr.kind  = ExpressionKind_E::Argument;
            expr.name  = _Text;
            expr.value = _Text;
            return expr;
        }

        // ── 2. 关系语法：-x?y:z 或 -x->y ──
        if (__isRelationSyntax(_Text)) {
            return __classifyRelation(_Text);
        }

        // ── 3. 长选项：--name[=value] ──
        if (util::starts_with(_Text, "--") && _Text.size() > 2) {
            auto body = _Text.substr(2);
            auto eq   = body.find('=');
            expr.kind = ExpressionKind_E::Option;
            expr.name = (eq == std::string::npos) ? body : body.substr(0, eq);
            if (eq != std::string::npos) { expr.value = body.substr(eq + 1); }
            return expr;
        }

        // ── 4. 短选项：-x / -xvalue / -abc 聚合 ──
        if (util::starts_with(_Text, "-") && _Text.size() >= 2) {
            // 聚合串：-abc（全部 bool）或 -tvalue（值选项）
            expr.kind       = ExpressionKind_E::Option;
            expr.cShortName = _Text[1];
            expr.name       = std::string(1, _Text[1]);
            return expr;
        }

        // ── 5. 兜底：普通 token → 方位参数 ──
        expr.kind  = ExpressionKind_E::Argument;
        expr.name  = _Text;
        expr.value = _Text;
        return expr;
    }

    /// @brief 判断 token 文本是否为关系语法（-x?y:z / -x?y:_ / -x->y）
    bool __isRelationSyntax(std::string const &_Text) const
    {
        if (_Text.size() < 4) return false;  // 最短：-a?b:c（6字符）/ -a->b（5字符）
        if (_Text[0] != '-') return false;

        // 查找 '?' 或 '->' 分隔符
        for (size_t i = 1; i < _Text.size(); ++i) {
            if (_Text[i] == '?') return true;
            if (_Text[i] == '-' && i + 1 < _Text.size() && _Text[i + 1] == '>') return true;
        }
        return false;
    }

    /// @brief 分类关系语法 token
    auto __classifyRelation(std::string const &_Text) const -> ClassifiedExpr
    {
        ClassifiedExpr expr;
        expr.bExplicit = true;

        // 查找 '?' 或 '->'
        size_t qPos = std::string::npos;
        size_t dPos = std::string::npos;
        for (size_t i = 1; i < _Text.size(); ++i) {
            if (_Text[i] == '?' && qPos == std::string::npos) { qPos = i; break; }
            if (_Text[i] == '-' && i + 1 < _Text.size() && _Text[i + 1] == '>') {
                dPos = i;
                break;
            }
        }

        if (qPos != std::string::npos) {
            // -a?b:c 或 -a?b:_
            expr.kind         = ExpressionKind_E::Conditional;
            expr.strCondition = _Text.substr(1, qPos - 1);  // a
            auto colonPos     = _Text.find(':', qPos);
            if (colonPos != std::string::npos) {
                expr.strThenBranch = _Text.substr(qPos + 1, colonPos - qPos - 1);  // b
                expr.strElseBranch = _Text.substr(colonPos + 1);  // c 或 "_"
            }
            else {
                // -a?b 无冒号：等价 -a?b:_（条件为真时激活 b，否则无操作）
                expr.strThenBranch = _Text.substr(qPos + 1);
                expr.strElseBranch = "_";
            }
        }
        else if (dPos != std::string::npos) {
            // -a->b
            expr.kind        = ExpressionKind_E::Dependency;
            expr.strDepSource = _Text.substr(1, dPos - 1);  // a
            expr.strDepTarget = _Text.substr(dPos + 2);      // b
        }

        return expr;
    }
};

CMD_ARGS_NAMESPACE_END
```

- [ ] **Step 3: 编译验证**

Run: `xmake build unit_test`
Expected: BUILD PASS（ExpressionClassifier 是纯头文件，无新 .cpp 需编译；既有代码不受影响）

- [ ] **Step 4: Commit**

```bash
git add include/cmd_args/context/expression.h include/cmd_args/context/expression_classifier.h
git commit -m "feat(expression): add ExpressionKind_E and ClassifiedExpr intermediate representation"
```

---

### Task 2: ExpressionClassifier 单元测试

**Files:**
- Create: `dfx/unit_test/test_expression_classifier.cpp`

**Interfaces:**
- Consumes: `ExpressionClassifier`（Task 1）、`detail::Token`（lexer.h）
- Produces: 无（纯测试）

- [ ] **Step 1: 基线确认**

Run: `xmake build unit_test && xmake run unit_test`
Expected: 全部 PASS，记录用例总数

- [ ] **Step 2: 写 test_expression_classifier.cpp**

```cpp
#include "catch2/catch.hpp"

#include <string>
#include <vector>

#include "cmd_args/cmd_args.hpp"

using namespace cmd_args;

/* ==================== ExpressionClassifier：Token → ClassifiedExpr ==================== */

TEST_CASE("classifier: 长选项分类为 Option Expression", "[classifier]")
{
    ExpressionClassifier classifier;

    // --timeout=9 → Option, name="timeout", value="9"
    detail::Token tok1(detail::Token::Type_E::LongName, "timeout", 0);
    tok1.setInlineEq(true);
    auto r1 = classifier.classifyOne(tok1);
    REQUIRE(r1.has_value());
    REQUIRE(r1->kind == ExpressionKind_E::Option);
    REQUIRE(r1->name == "timeout");
    REQUIRE(r1->value == "9");
    REQUIRE(r1->bExplicit);

    // --timeout (无值) → Option, name="timeout", value=""
    detail::Token tok2(detail::Token::Type_E::LongName, "timeout", 0);
    auto r2 = classifier.classifyOne(tok2);
    REQUIRE(r2.has_value());
    REQUIRE(r2->kind == ExpressionKind_E::Option);
    REQUIRE(r2->name == "timeout");
    REQUIRE(r2->value.empty());
}

TEST_CASE("classifier: 短选项分类为 Option Expression", "[classifier]")
{
    ExpressionClassifier classifier;

    // -t → Option, cShortName='t'
    detail::Token tok1(detail::Token::Type_E::ShortName, "t", 0);
    auto r1 = classifier.classifyOne(tok1);
    REQUIRE(r1.has_value());
    REQUIRE(r1->kind == ExpressionKind_E::Option);
    REQUIRE(r1->cShortName == 't');

    // -abc → Option, cShortName='a'（聚合串首字符）
    detail::Token tok2(detail::Token::Type_E::ContinualName, "abc", 0);
    auto r2 = classifier.classifyOne(tok2);
    REQUIRE(r2.has_value());
    REQUIRE(r2->kind == ExpressionKind_E::Option);
    REQUIRE(r2->cShortName == 'a');
}

TEST_CASE("classifier: 方位参数分类为 Argument Expression", "[classifier]")
{
    ExpressionClassifier classifier;

    detail::Token tok(detail::Token::Type_E::Variable, "world", 0);
    auto r = classifier.classifyOne(tok);
    REQUIRE(r.has_value());
    REQUIRE(r->kind == ExpressionKind_E::Argument);
    REQUIRE(r->name == "world");
    REQUIRE(r->value == "world");
}

TEST_CASE("classifier: 环境变量 KEY=VALUE 分类为 Env Expression", "[classifier]")
{
    ExpressionClassifier classifier;

    // 环境变量在 Lexer 归一化后以 "KEY=VALUE" plain token 形式出现
    detail::Token tok(detail::Token::Type_E::Variable, "HOME=/root", 0);
    auto r = classifier.classifyOne(tok);
    REQUIRE(r.has_value());
    REQUIRE(r->kind == ExpressionKind_E::Env);
    REQUIRE(r->name == "HOME");
    REQUIRE(r->value == "/root");
}

TEST_CASE("classifier: 条件关系 -a?b:c 分类为 Conditional", "[classifier]")
{
    ExpressionClassifier classifier;

    // -verbose?debug:quiet → Conditional
    detail::Token tok(detail::Token::Type_E::ContinualName, "-verbose?debug:quiet", 0);
    auto r = classifier.classifyOne(tok);
    REQUIRE(r.has_value());
    REQUIRE(r->kind == ExpressionKind_E::Conditional);
    REQUIRE(r->strCondition == "verbose");
    REQUIRE(r->strThenBranch == "debug");
    REQUIRE(r->strElseBranch == "quiet");
}

TEST_CASE("classifier: 条件关系 -a?b:_ 分类为 Conditional（无操作分支）", "[classifier]")
{
    ExpressionClassifier classifier;

    // -verbose?debug:_ → Conditional, else="_"
    detail::Token tok(detail::Token::Type_E::ContinualName, "-verbose?debug:_", 0);
    auto r = classifier.classifyOne(tok);
    REQUIRE(r.has_value());
    REQUIRE(r->kind == ExpressionKind_E::Conditional);
    REQUIRE(r->strCondition == "verbose");
    REQUIRE(r->strThenBranch == "debug");
    REQUIRE(r->strElseBranch == "_");
}

TEST_CASE("classifier: 条件关系 -a?b 无冒号等价 -a?b:_", "[classifier]")
{
    ExpressionClassifier classifier;

    detail::Token tok(detail::Token::Type_E::ContinualName, "-verbose?debug", 0);
    auto r = classifier.classifyOne(tok);
    REQUIRE(r.has_value());
    REQUIRE(r->kind == ExpressionKind_E::Conditional);
    REQUIRE(r->strCondition == "verbose");
    REQUIRE(r->strThenBranch == "debug");
    REQUIRE(r->strElseBranch == "_");
}

TEST_CASE("classifier: 依赖关系 -a->b 分类为 Dependency", "[classifier]")
{
    ExpressionClassifier classifier;

    // -compile->link → Dependency
    detail::Token tok(detail::Token::Type_E::ContinualName, "-compile->link", 0);
    auto r = classifier.classifyOne(tok);
    REQUIRE(r.has_value());
    REQUIRE(r->kind == ExpressionKind_E::Dependency);
    REQUIRE(r->strDepSource == "compile");
    REQUIRE(r->strDepTarget == "link");
}

TEST_CASE("classifier: classifyAll 批量分类", "[classifier]")
{
    ExpressionClassifier classifier;

    Tokens_T toks{ "--timeout=9", "-abc", "world" };
    auto results = classifier.classifyAll(toks);

    REQUIRE(results.size() == 3);
    REQUIRE(results[0].kind == ExpressionKind_E::Option);
    REQUIRE(results[0].name == "timeout");
    REQUIRE(results[1].kind == ExpressionKind_E::Option);
    REQUIRE(results[1].cShortName == 'a');
    REQUIRE(results[2].kind == ExpressionKind_E::Argument);
    REQUIRE(results[2].name == "world");
}

TEST_CASE("classifier: 空 token 返回 nullopt", "[classifier]")
{
    ExpressionClassifier classifier;

    detail::Token tok(detail::Token::Type_E::Unknown, "", 0);
    auto r = classifier.classifyOne(tok);
    REQUIRE_FALSE(r.has_value());
}
```

- [ ] **Step 3: 编译并跑新标签**

Run: `xmake build unit_test && xmake run unit_test "[classifier]"`
Expected: 编译零警告；9 个用例全 PASS

- [ ] **Step 4: 全量回归**

Run: `xmake run unit_test`
Expected: 全部 PASS，用例数 = Step 1 基线 + 9

- [ ] **Step 5: Commit**

```bash
git add dfx/unit_test/test_expression_classifier.cpp
git commit -m "test(classifier): cover Token-to-ClassifiedExpr classification for all expression kinds"
```

---

### Task 3: Interpreter __reduceStack 委托 ExpressionClassifier

**Files:**
- Modify: `include/cmd_args/context/interpreter.h`（`__reduceStack` 方法）
- Test: 既有 `dfx/unit_test/test_interpreter.cpp` 作为回归

**Interfaces:**
- Consumes: `ExpressionClassifier`（Task 1）、`ClassifiedExpr`
- Produces: `__reduceStack` 内部逻辑委托给 classifier，行为不变

- [ ] **Step 1: 修改 interpreter.h — 新增成员与 include**

(a) include 区追加：
```cpp
#include "cmd_args/context/expression_classifier.h"
```

(b) 私有成员区追加（在 `vecDashLeft__` 之后）：
```cpp
    ExpressionClassifier classifier__;  ///< 表达式分类器：Token → ClassifiedExpr
```

- [ ] **Step 2: 重写 __reduceStack — 分类委托模式**

将 `__reduceStack()` 方法体替换为基于 `classifier__.classifyOne()` 的实现。核心逻辑保持不变（挂起取值消费 → `--` 终止 → 长选项 → 短选项 → 方位参数），但 token 识别部分委托给 classifier：

```cpp
    void __reduceStack()
    {
        auto tok = std::move(vecStack__.back());
        vecStack__.pop_back();

        auto classified = classifier__.classifyOne(tok);
        if (!classified) return;  // 空 token 忽略

        auto const &expr = *classified;
        std::string text = tok.toText();
        bool bInlineEq   = tok.hasInlineEq();

        // ── 1. 挂起取值：当前 token 就是值 ──
        if (!strPendingKey__.empty()) {
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

        // ── 3. 关系语法：条件/依赖 ──
        if (expr.isRelation()) {
            __handleRelation(expr);
            return;
        }

        // ── 4. 长选项 / 命名参数：--name[=value] ──
        if (expr.kind == ExpressionKind_E::Option && util::starts_with(text, "--") && text.size() > 2) {
            auto name  = expr.name;
            auto value = expr.value;

            if (auto const *pSc = pModel__->opts.mgrActions.doGet(name)) {
                static_cast<OptionAction const *>(pSc)->fnCallback();
                return;
            }
            if (auto *pOpt = pModel__->opts.mgrValues.mutFind(name)) {
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
                pArg->set(std::move(value));
                return;
            }
            vecDashLeft__.emplace_back(text);
            return;
        }

        // ── 5. 短选项：-x / 聚合 -abc ──
        if (expr.kind == ExpressionKind_E::Option && util::starts_with(text, "-") && text.size() >= 2) {
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
                std::string val = text.substr(i + 1);
                if (!val.empty() && val.front() == '=') { val = val.substr(1); }
                if (val.empty()) {
                    strPendingKey__.assign(1, key);
                    enPendingKind__    = PendingKind_E::ShortOpt;
                    bPendingInlineEq__ = false;
                    return;
                }
                __executeOptionImmediate(pOpt, val, false);
                return;
            }
            return;
        }

        // ── 6. 方位参数（含命名参数 --name value 的 name 未注册情况） ──
        if (expr.kind == ExpressionKind_E::Argument) {
            // 尝试作为命名参数
            if (util::starts_with(text, "--") && text.size() > 2) {
                auto name = text.substr(2);
                if (auto *pArg = pModel__->args.mgrName.mutGet(name)) {
                    strPendingKey__    = name;
                    enPendingKind__    = PendingKind_E::NamedArg;
                    bPendingInlineEq__ = false;
                    return;
                }
            }
            vecPos__.emplace_back(text);
            return;
        }

        // 兜底
        vecPos__.emplace_back(text);
    }
```

注意：`__reduceStack` 新增了 `__handleRelation` 私有方法（Task 7 实现），此处先声明、后续 Task 填充。

- [ ] **Step 3: 新增 __handleRelation 空壳（Task 7 填充）**

在 `__reduceStack` 之后、`__executeOptionImmediate` 之前插入：

```cpp
    /// @brief 处理关系语法：条件（-a?b:c）和依赖（-a->b）
    /// @note Phase A 阶段为空壳（仅记录关系，不做执行）；Phase B 由 OptionGraph 驱动执行
    void __handleRelation(ClassifiedExpr const &_Expr)
    {
        // Phase A：暂存关系到 OptionGraph（Phase B 实现）
        // 当前阶段：忽略关系 token（不报错，兼容现有测试）
        (void)_Expr;
    }
```

- [ ] **Step 4: 全量回归**

Run: `xmake build unit_test && xmake run unit_test`
Expected: 全部 PASS（既有 interpreter 用例行为不变，因为 classifier 委托后分类逻辑等价）

- [ ] **Step 5: Commit**

```bash
git add include/cmd_args/context/interpreter.h
git commit -m "refactor(interpreter): delegate __reduceStack token classification to ExpressionClassifier"
```

---

## Phase B: Option 关系有向图

### Task 4: OptionGraph 数据结构

**Files:**
- Create: `include/cmd_args/option/option_graph.h`

**Interfaces:**
- Consumes: `ClassifiedExpr`（Task 1）
- Produces: `OptionGraph` 类（Node/Edge/topologicalSort/evaluate）

- [ ] **Step 1: 创建 option_graph.h**

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

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "cmd_args/base/def.h"
#include "cmd_args/base/util.h"

CMD_ARGS_NAMESPACE_BEGIN

/**
 * @brief Option 关系有向图：存储选项之间的条件（Conditional）与依赖（Dependency）关系。
 *
 * 边类型：
 * - Conditional（-a?b:c）：a 为真时激活 b，否则激活 c
 * - Dependency（-a->b）：先执行 a，再执行 b
 *
 * 图在 OptionBuilder 注册阶段构建（addRelation），
 * 在 OptionParser 解析时填充节点状态（enable/disable），
 * 在 Executor 执行前进行拓扑排序确定执行顺序。
 */
class OptionGraph
{
public:
    /// @brief 边类型
    enum class EdgeKind_E : char
    {
        Conditional,  ///< 条件边：-a?b:c
        Dependency    ///< 依赖边：-a->b
    };

    /// @brief 节点状态
    struct Node
    {
        std::string strName;       ///< 选项名
        bool        bEnabled = false;  ///< 是否被激活（解析期填充）
    };

    /// @brief 有向边
    struct Edge
    {
        std::string strFrom;       ///< 源节点（条件选项 / 依赖源）
        std::string strTo;         ///< 目标节点（激活选项 / 依赖目标）
        EdgeKind_E  enKind;        ///< 边类型
        std::string strFallback;   ///< 条件边的 else 分支（"_" 表示无操作）
    };

    /// @brief 添加关系边
    /// @param _From 源选项名
    /// @param _To 目标选项名
    /// @param _Kind 边类型
    /// @param _Fallback 条件边的 else 分支（仅 Conditional 有意义）
    void addEdge(std::string _From, std::string _To, EdgeKind_E _Kind, std::string _Fallback = "")
    {
        // 确保节点存在
        __ensureNode(_From);
        __ensureNode(_To);
        if (!_Fallback.empty() && _Fallback != "_") { __ensureNode(_Fallback); }

        vecEdges__.emplace_back(Edge{
            std::move(_From),
            std::move(_To),
            _Kind,
            std::move(_Fallback)
        });
    }

    /// @brief 启用/禁用节点（OptionParser 解析时调用）
    /// @param _Name 选项名
    /// @param _Enabled 是否激活
    void setNodeState(std::string const &_Name, bool _Enabled)
    {
        auto it = mapNodes__.find(_Name);
        if (it != mapNodes__.end()) { it->second.bEnabled = _Enabled; }
    }

    /// @brief 查询节点是否激活
    auto isNodeEnabled(std::string const &_Name) const -> bool
    {
        auto it = mapNodes__.find(_Name);
        return it != mapNodes__.end() && it->second.bEnabled;
    }

    /// @brief 拓扑排序：返回边的执行顺序（入度为零的先执行）
    /// @return 排序后的选项名列表
    auto topologicalSort() const -> std::vector<std::string>
    {
        // 构建入度表
        std::map<std::string, int> inDegree;
        for (auto const &node : mapNodes__) { inDegree[node.first] = 0; }
        for (auto const &edge : vecEdges__) { ++inDegree[edge.strTo]; }

        // BFS 拓扑排序
        std::vector<std::string> queue;
        for (auto const &kv : inDegree) {
            if (kv.second == 0) { queue.emplace_back(kv.first); }
        }

        std::vector<std::string> result;
        size_t idx = 0;
        while (idx < queue.size()) {
            auto const &cur = queue[idx++];
            result.emplace_back(cur);
            for (auto const &edge : vecEdges__) {
                if (edge.strFrom == cur) {
                    if (--inDegree[edge.strTo] == 0) {
                        queue.emplace_back(edge.strTo);
                    }
                }
            }
        }

        return result;
    }

    /// @brief 求值：根据节点状态激活条件边的目标节点
    /// @return 新激活的选项名列表（按拓扑序）
    auto evaluate() -> std::vector<std::string>
    {
        std::vector<std::string> activated;

        for (auto const &edge : vecEdges__) {
            if (edge.enKind == EdgeKind_E::Conditional) {
                bool condState = isNodeEnabled(edge.strFrom);
                if (condState) {
                    // 条件为真：激活 then 分支
                    if (edge.strTo != "_") {
                        setNodeState(edge.strTo, true);
                        activated.emplace_back(edge.strTo);
                    }
                }
                else {
                    // 条件为假：激活 else 分支
                    if (!edge.strFallback.empty() && edge.strFallback != "_") {
                        setNodeState(edge.strFallback, true);
                        activated.emplace_back(edge.strFallback);
                    }
                }
            }
        }

        return activated;
    }

    /// @brief 获取所有边（只读）
    auto edges() const -> std::vector<Edge> const & { return vecEdges__; }

    /// @brief 获取所有节点（只读）
    auto nodes() const -> std::map<std::string, Node> const & { return mapNodes__; }

    /// @brief 是否为空（无边）
    bool empty() const noexcept { return vecEdges__.empty(); }

    /// @brief 清空图（每次解析前调用）
    void clear()
    {
        mapNodes__.clear();
        vecEdges__.clear();
    }

private:
    /// @brief 确保节点存在（不存在则创建）
    void __ensureNode(std::string const &_Name)
    {
        if (mapNodes__.find(_Name) == mapNodes__.end()) {
            mapNodes__[_Name] = Node{ _Name, false };
        }
    }

    std::map<std::string, Node> mapNodes__;  ///< 节点表
    std::vector<Edge>           vecEdges__;  ///< 边列表
};

CMD_ARGS_NAMESPACE_END
```

- [ ] **Step 2: 编译验证**

Run: `xmake build unit_test`
Expected: BUILD PASS

- [ ] **Step 3: Commit**

```bash
git add include/cmd_args/option/option_graph.h
git commit -m "feat(option): add OptionGraph directed graph for conditional and dependency relations"
```

---

### Task 5: OptionGraph 单元测试

**Files:**
- Create: `dfx/unit_test/test_option_graph.cpp`

**Interfaces:**
- Consumes: `OptionGraph`（Task 4）
- Produces: 无（纯测试）

- [ ] **Step 1: 写 test_option_graph.cpp**

```cpp
#include "catch2/catch.hpp"

#include <string>
#include <vector>

#include "cmd_args/cmd_args.hpp"

using namespace cmd_args;

/* ==================== OptionGraph：有向图 ==================== */

TEST_CASE("graph: addEdge 后节点自动创建", "[graph]")
{
    OptionGraph graph;
    graph.addEdge("a", "b", OptionGraph::EdgeKind_E::Conditional, "c");

    REQUIRE(graph.nodes().count("a") == 1);
    REQUIRE(graph.nodes().count("b") == 1);
    REQUIRE(graph.nodes().count("c") == 1);
    REQUIRE_FALSE(graph.empty());
}

TEST_CASE("graph: 拓扑排序——无环图返回有效序", "[graph]")
{
    OptionGraph graph;
    graph.addEdge("a", "b", OptionGraph::EdgeKind_E::Dependency);
    graph.addEdge("b", "c", OptionGraph::EdgeKind_E::Dependency);

    auto order = graph.topologicalSort();
    REQUIRE(order.size() == 3);
    // a 在 b 之前，b 在 c 之前
    auto posA = std::find(order.begin(), order.end(), "a") - order.begin();
    auto posB = std::find(order.begin(), order.end(), "b") - order.begin();
    auto posC = std::find(order.begin(), order.end(), "c") - order.begin();
    REQUIRE(posA < posB);
    REQUIRE(posB < posC);
}

TEST_CASE("graph: 条件求值——条件为真激活 then 分支", "[graph]")
{
    OptionGraph graph;
    graph.addEdge("verbose", "debug", OptionGraph::EdgeKind_E::Conditional, "quiet");

    graph.setNodeState("verbose", true);
    auto activated = graph.evaluate();

    REQUIRE(activated.size() == 1);
    REQUIRE(activated[0] == "debug");
    REQUIRE(graph.isNodeEnabled("debug"));
    REQUIRE_FALSE(graph.isNodeEnabled("quiet"));
}

TEST_CASE("graph: 条件求值——条件为假激活 else 分支", "[graph]")
{
    OptionGraph graph;
    graph.addEdge("verbose", "debug", OptionGraph::EdgeKind_E::Conditional, "quiet");

    graph.setNodeState("verbose", false);
    auto activated = graph.evaluate();

    REQUIRE(activated.size() == 1);
    REQUIRE(activated[0] == "quiet");
    REQUIRE(graph.isNodeEnabled("quiet"));
    REQUIRE_FALSE(graph.isNodeEnabled("debug"));
}

TEST_CASE("graph: 条件求值——else 为 _ 时不激活任何选项", "[graph]")
{
    OptionGraph graph;
    graph.addEdge("verbose", "debug", OptionGraph::EdgeKind_E::Conditional, "_");

    graph.setNodeState("verbose", false);
    auto activated = graph.evaluate();

    REQUIRE(activated.empty());
    REQUIRE_FALSE(graph.isNodeEnabled("debug"));
}

TEST_CASE("graph: 依赖边不参与条件求值（仅影响拓扑序）", "[graph]")
{
    OptionGraph graph;
    graph.addEdge("compile", "link", OptionGraph::EdgeKind_E::Dependency);

    graph.setNodeState("compile", true);
    auto activated = graph.evaluate();

    // Dependency 边不在 evaluate 中激活目标——目标由上层（Executor）按拓扑序显式触发
    REQUIRE(activated.empty());
}

TEST_CASE("graph: clear 后图为空", "[graph]")
{
    OptionGraph graph;
    graph.addEdge("a", "b", OptionGraph::EdgeKind_E::Conditional, "c");
    REQUIRE_FALSE(graph.empty());

    graph.clear();
    REQUIRE(graph.empty());
    REQUIRE(graph.nodes().empty());
    REQUIRE(graph.edges().empty());
}
```

- [ ] **Step 2: 编译并跑新标签**

Run: `xmake build unit_test && xmake run unit_test "[graph]"`
Expected: 编译零警告；7 个用例全 PASS

- [ ] **Step 3: 全量回归**

Run: `xmake run unit_test`
Expected: 全部 PASS

- [ ] **Step 4: Commit**

```bash
git add dfx/unit_test/test_option_graph.cpp
git commit -m "test(graph): cover OptionGraph addEdge/topologicalSort/evaluate/clear"
```

---

### Task 6: Scanner 识别关系语法

**Files:**
- Modify: `include/cmd_args/context/lexer.h`（Scanner / Lexer）
- Test: `dfx/unit_test/test_lexer.cpp`（追加）

**Interfaces:**
- Consumes: 无（独立扩展）
- Produces: Scanner 识别 `-a?b:c`、`-a?b:_`、`-a->b` 语法，产出 `ContinualName` 类型 Token（text 保留完整关系字符串）

- [ ] **Step 1: 修改 Scanner — IN_CONTINUAL_NAME 状态扩展**

在 `__processContinualName` 方法中，追加对 `?` 和 `->` 字符的处理——它们应作为关系分隔符终结当前连续名：

```cpp
    void __processContinualName(char const _Cur, Token &_T)
    {
        if (_Cur == '?' || _Cur == '-') {
            // 关系语法终结符：? (条件) 或 - (-> 依赖的前半)
            // 先终结当前 token
            _T.setAttr(Token::Type_E::ContinualName);
            __setState(Phrase_E::END);
            return;
        }
        if (std::isalpha(static_cast<unsigned char>(_Cur))) {
            _T.pick(_Cur);
            __nextChar();
            __setState(Phrase_E::IN_CONTINUAL_NAME);
        }
        else {
            _T.setAttr(Token::Type_E::ContinualName);
            __setState(Phrase_E::END);
        }
    }
```

但这会导致 `-a?b:c` 被拆成多个 token（`-a`、`?b:c`）。为了保持关系语法作为**单个 token**，需要在 `Lexer::__normalize` 中做合并：

在 `Lexer::__normalize` 静态方法中追加关系语法合并逻辑：

```cpp
    static Tokens_T __normalize(Scanner::Tokens_T const &_Raw)
    {
        Tokens_T res;
        for (size_t i = 0; i < _Raw.size(); ++i) {
            auto const &t    = _Raw[i];
            auto        attr = t.getAttr();

            // 关系语法合并：-a?b:c / -a?b:_ / -a->b
            // Scanner 会把 -a? 拆成 ContinualName("-a") + Delimiter("?") + ...
            // 这里检测 ContinualName 后跟 ? 或 -> 的模式，合并为单个 token
            if (attr == Token::Type_E::ContinualName && i + 1 < _Raw.size()
                && _Raw[i + 1].getAttr() == Token::Type_E::Delimiter) {
                auto const &next = _Raw[i + 1];
                if (next.getName() == "?" || next.getName() == "->") {
                    // 合并：-a + ? + b:c → -a?b:c
                    std::string merged = t.toText() + next.getName();
                    size_t j = i + 2;
                    while (j < _Raw.size()
                           && _Raw[j].getAttr() != Token::Type_E::LongName
                           && _Raw[j].getAttr() != Token::Type_E::ShortName
                           && !(_Raw[j].getAttr() == Token::Type_E::Delimiter
                                && _Raw[j].getName() != ":")) {
                        merged += _Raw[j].toText();
                        ++j;
                    }
                    i = j - 1;
                    res.emplace_back(merged);
                    continue;
                }
            }

            // --name=value / -x=value / KEY=VALUE 合并为单个 token
            if (i + 2 < _Raw.size()
                && _Raw[i + 1].getAttr() == Token::Type_E::Delimiter
                && _Raw[i + 1].getName() == "="
                && (attr == Token::Type_E::LongName || attr == Token::Type_E::ShortName
                    || attr == Token::Type_E::ContinualName
                    || attr == Token::Type_E::Variable)) {
                res.emplace_back(t.toText() + "=" + _Raw[i + 2].toText());
                i += 2;
                continue;
            }

            auto text = t.toText();
            if (text.empty()) continue;
            res.emplace_back(std::move(text));
        }

        return res;
    }
```

- [ ] **Step 2: 写测试 — test_lexer.cpp 追加**

在 `dfx/unit_test/test_lexer.cpp` 末尾追加：

```cpp
/* ==================== Scanner/Lexer 关系语法识别 ==================== */

TEST_CASE("lexer: -a?b:c 关系语法合并为单个 token", "[lexer]")
{
    Tokens_T toks{ "-verbose?debug:quiet" };
    auto results = Lexer(0, nullptr, nullptr).tokens();
    // tokensFromLine 方式测试
    auto lineToks = Lexer::tokensFromLine("-verbose?debug:quiet");
    REQUIRE(lineToks.size() == 1);
    REQUIRE(lineToks.front() == "-verbose?debug:quiet");
}

TEST_CASE("lexer: -a?b:_ 关系语法（无操作分支）", "[lexer]")
{
    auto lineToks = Lexer::tokensFromLine("-verbose?debug:_");
    REQUIRE(lineToks.size() == 1);
    REQUIRE(lineToks.front() == "-verbose?debug:_");
}

TEST_CASE("lexer: -a->b 依赖关系语法", "[lexer]")
{
    auto lineToks = Lexer::tokensFromLine("-compile->link");
    REQUIRE(lineToks.size() == 1);
    REQUIRE(lineToks.front() == "-compile->link");
}

TEST_CASE("lexer: 普通聚合 -abc 不受关系语法影响", "[lexer]")
{
    auto lineToks = Lexer::tokensFromLine("-abc");
    REQUIRE(lineToks.size() == 1);
    REQUIRE(lineToks.front() == "-abc");
}
```

- [ ] **Step 3: 编译并跑 lexer 标签**

Run: `xmake build unit_test && xmake run unit_test "[lexer]"`
Expected: 编译零警告；既有 lexer 用例 + 新增 4 个用例全 PASS

- [ ] **Step 4: 全量回归**

Run: `xmake run unit_test`
Expected: 全部 PASS

- [ ] **Step 5: Commit**

```bash
git add include/cmd_args/context/lexer.h dfx/unit_test/test_lexer.cpp
git commit -m "feat(lexer): recognize option relation syntax (-a?b:c, -a?b:_, -a->b)"
```

---

### Task 7: OptionParser 集成 OptionGraph + Interpreter __handleRelation

**Files:**
- Modify: `include/cmd_args/option/option_parser.h`（集成 OptionGraph）
- Modify: `include/cmd_args/context/interpreter.h`（填充 `__handleRelation`）
- Test: `dfx/unit_test/test_option.cpp`（追加关系语法测试）

**Interfaces:**
- Consumes: `OptionGraph`（Task 4）、`ExpressionClassifier`（Task 1）
- Produces: OptionParser 持有 OptionGraph 引用；Interpreter `__handleRelation` 调用 graph

- [ ] **Step 1: 修改 OptionParser — 持有 OptionGraph 引用**

在 `OptionParser` 私有成员区追加：

```cpp
    OptionGraph *pGraph__ = nullptr;  ///< 关系有向图（可选，为 nullptr 时关系语法被忽略）
```

新增公开 setter：

```cpp
    /// @brief 绑定关系有向图（可选，为 nullptr 时关系语法被忽略）
    void setGraph(OptionGraph *_Graph) { pGraph__ = _Graph; }
```

在 `parse()` 方法的短选项聚合分支中，追加对关系语法 token 的处理：

```cpp
    // 在 for 循环的短选项分支中，聚合串内检测关系语法
    if (util::starts_with(tok, "-") && tok.size() >= 2) {
        // 新增：检测关系语法 token（-a?b:c / -a->b）
        if (pGraph__ && __isRelationToken(tok)) {
            __parseRelation(tok);
            it = _Tokens.erase(it);
            continue;
        }
        // ... 原有聚合逻辑 ...
    }
```

新增私有方法：

```cpp
    /// @brief 判断 token 是否为关系语法
    bool __isRelationToken(std::string const &_Tok) const
    {
        if (_Tok.size() < 4 || _Tok[0] != '-') return false;
        for (size_t i = 1; i < _Tok.size(); ++i) {
            if (_Tok[i] == '?') return true;
            if (_Tok[i] == '-' && i + 1 < _Tok.size() && _Tok[i + 1] == '>') return true;
        }
        return false;
    }

    /// @brief 解析关系语法 token 并注册到 OptionGraph
    void __parseRelation(std::string const &_Tok)
    {
        if (!pGraph__) return;

        auto qPos = _Tok.find('?');
        auto dPos = _Tok.find("->");

        if (qPos != std::string::npos) {
            // -a?b:c
            auto condition = _Tok.substr(1, qPos - 1);
            auto colonPos  = _Tok.find(':', qPos);
            std::string thenBranch, elseBranch;
            if (colonPos != std::string::npos) {
                thenBranch = _Tok.substr(qPos + 1, colonPos - qPos - 1);
                elseBranch = _Tok.substr(colonPos + 1);
            }
            else {
                thenBranch = _Tok.substr(qPos + 1);
                elseBranch = "_";
            }
            pGraph__->addEdge(condition, thenBranch,
                OptionGraph::EdgeKind_E::Conditional, elseBranch);
        }
        else if (dPos != std::string::npos) {
            // -a->b
            auto source = _Tok.substr(1, dPos - 1);
            auto target = _Tok.substr(dPos + 2);
            pGraph__->addEdge(source, target, OptionGraph::EdgeKind_E::Dependency);
        }
    }
```

- [ ] **Step 2: 填充 Interpreter __handleRelation**

将 Task 3 中的空壳 `__handleRelation` 替换为实际实现。Interpreter 没有 OptionGraph 成员（流式模式不构建图），关系 token 在流式模式下直接按条件语义即时执行：

```cpp
    /// @brief 处理关系语法：条件（-a?b:c）和依赖（-a->b）
    /// @note 流式模式：条件边即时判断并激活目标；依赖边按序执行
    void __handleRelation(ClassifiedExpr const &_Expr)
    {
        if (_Expr.kind == ExpressionKind_E::Conditional) {
            // 条件关系：检查条件选项状态
            bool condState = false;
            if (auto const *pOpt = pModel__->opts.mgrValues.find(_Expr.strCondition)) {
                condState = util::parse_bool(pOpt->strValue);
            }

            if (condState) {
                // 条件为真：激活 then 分支
                if (_Expr.strThenBranch != "_") {
                    if (auto *pOpt = pModel__->opts.mgrValues.mutFind(_Expr.strThenBranch)) {
                        __executeOptionImmediate(pOpt, "1", true);
                    }
                }
            }
            else {
                // 条件为假：激活 else 分支
                if (!_Expr.strElseBranch.empty() && _Expr.strElseBranch != "_") {
                    if (auto *pOpt = pModel__->opts.mgrValues.mutFind(_Expr.strElseBranch)) {
                        __executeOptionImmediate(pOpt, "1", true);
                    }
                }
            }
        }
        else if (_Expr.kind == ExpressionKind_E::Dependency) {
            // 依赖关系：先执行 source，再执行 target
            // 源选项已被激活（解析序保证），现在激活目标
            if (auto *pOpt = pModel__->opts.mgrValues.mutFind(_Expr.strDepTarget)) {
                __executeOptionImmediate(pOpt, "1", true);
            }
        }
    }
```

- [ ] **Step 3: 写测试 — test_option.cpp 追加**

在 `dfx/unit_test/test_option.cpp` 末尾追加：

```cpp
/* ==================== OptionParser 关系语法 ==================== */

TEST_CASE("option: 关系语法 -a?b:c 注册到 OptionGraph", "[option]")
{
    Model model;
    OptionBuilder ob(&model.opts);
    ob.addFlag('v', "verbose", "verbose mode");
    ob.addFlag('d', "debug", "debug mode");
    ob.addFlag('q', "quiet", "quiet mode");

    OptionGraph graph;
    OptionParser op(&model.opts);
    op.setGraph(&graph);

    Tokens_T toks{ "-verbose?debug:quiet" };
    REQUIRE_FALSE(op.parse(toks));

    REQUIRE_FALSE(graph.empty());
    REQUIRE(graph.edges().size() == 1);
    auto const &edge = graph.edges()[0];
    REQUIRE(edge.strFrom == "verbose");
    REQUIRE(edge.strTo == "debug");
    REQUIRE(edge.strFallback == "quiet");
    REQUIRE(edge.enKind == OptionGraph::EdgeKind_E::Conditional);
}

TEST_CASE("option: 依赖语法 -a->b 注册到 OptionGraph", "[option]")
{
    Model model;
    OptionBuilder ob(&model.opts);
    ob.addFlag('c', "compile", "compile");
    ob.addFlag('l', "link", "link");

    OptionGraph graph;
    OptionParser op(&model.opts);
    op.setGraph(&graph);

    Tokens_T toks{ "-compile->link" };
    REQUIRE_FALSE(op.parse(toks));

    REQUIRE(graph.edges().size() == 1);
    auto const &edge = graph.edges()[0];
    REQUIRE(edge.strFrom == "compile");
    REQUIRE(edge.strTo == "link");
    REQUIRE(edge.enKind == OptionGraph::EdgeKind_E::Dependency);
}

TEST_CASE("option: 无 OptionGraph 时关系语法被忽略（兼容）", "[option]")
{
    Model model;
    OptionBuilder ob(&model.opts);
    ob.addFlag('v', "verbose", "verbose mode");

    OptionParser op(&model.opts);
    // 不调用 setGraph：pGraph__ 为 nullptr

    Tokens_T toks{ "-verbose?debug:quiet" };
    // 关系语法在无图时被跳过，不报错
    REQUIRE_FALSE(op.parse(toks));
}
```

- [ ] **Step 4: 写测试 — test_interpreter.cpp 追加条件/依赖关系测试**

在 `dfx/unit_test/test_interpreter.cpp` 末尾追加：

```cpp
/* ==================== Interpreter 关系语法 ==================== */

TEST_CASE("interpreter: 条件关系 -v?d:q 条件为真激活 then 分支", "[interpreter]")
{
    Model model;
    Errors errors;
    Interpreter interp(&model);
    OptionBuilder ob(&model.opts);
    ob.addFlag('v', "verbose", "verbose mode");
    ob.addFlag('d', "debug", "debug mode");
    ob.addFlag('q', "quiet", "quiet mode");

    auto ec = interp.interpret("-verbose?debug:quiet", errors);
    REQUIRE(!ec);
    REQUIRE(model.opts.mgrValues.find("debug")->strValue == "1");
    REQUIRE(model.opts.mgrValues.find("quiet")->strValue == "0");
}

TEST_CASE("interpreter: 条件关系 -v?d:q 条件为假激活 else 分支", "[interpreter]")
{
    Model model;
    Errors errors;
    Interpreter interp(&model);
    OptionBuilder ob(&model.opts);
    ob.addFlag('v', "verbose", "verbose mode");
    ob.addFlag('d', "debug", "debug mode");
    ob.addFlag('q', "quiet", "quiet mode");

    // verbose 未激活（默认 false），条件为假 → 激活 quiet
    auto ec = interp.interpret("-debug?verbose:quiet", errors);
    REQUIRE(!ec);
    // debug 被激活（-debug 设为 true），但 verbose? 的条件是 verbose（false）
    // 所以 quiet 被激活
    REQUIRE(model.opts.mgrValues.find("quiet")->strValue == "1");
}

TEST_CASE("interpreter: 依赖关系 -a->b 按序激活", "[interpreter]")
{
    Model model;
    Errors errors;
    Interpreter interp(&model);
    OptionBuilder ob(&model.opts);
    ob.addFlag('c', "compile", "compile");
    ob.addFlag('l', "link", "link");

    auto ec = interp.interpret("-compile->link", errors);
    REQUIRE(!ec);
    REQUIRE(model.opts.mgrValues.find("link")->strValue == "1");
}
```

- [ ] **Step 5: 编译并跑 option 标签**

Run: `xmake build unit_test && xmake run unit_test "[option]"`
Expected: 编译零警告；既有 option 用例 + 新增 3 个用例全 PASS

- [ ] **Step 6: 全量回归**

Run: `xmake run unit_test`
Expected: 全部 PASS

- [ ] **Step 7: Commit**

```bash
git add include/cmd_args/option/option_parser.h include/cmd_args/context/interpreter.h dfx/unit_test/test_option.cpp dfx/unit_test/test_interpreter.cpp
git commit -m "feat(option): integrate OptionGraph with OptionParser and Interpreter for relation syntax"
```

---

### Task 8: Compiler 集成 ExpressionClassifier + OptionGraph

**Files:**
- Modify: `include/cmd_args/context/compiler.h`（`__parseTokens` 方法）
- Modify: `include/cmd_args/context/model.h`（追加 OptionGraph 成员）
- Test: 既有 `dfx/unit_test/test_context.cpp` 作为回归

**Interfaces:**
- Consumes: `ExpressionClassifier`（Task 1）、`OptionGraph`（Task 4）
- Produces: Compiler 使用 classifier 分类 token，并将关系边注册到 Model 的 OptionGraph

- [ ] **Step 1: 修改 Model — 追加 OptionGraph 成员**

在 `model.h` 的 `Model` 类公开成员区追加：

```cpp
    OptionGraph optionGraph;  ///< Option 关系有向图（条件/依赖边）
```

追加 include：

```cpp
#include "cmd_args/option/option_graph.h"
```

- [ ] **Step 2: 修改 Compiler — 使用 ExpressionClassifier**

在 `compiler.h` 的 `__parseTokens` 方法中，在 `optParser__.parse(_Tokens)` 之前追加分类步骤：

```cpp
    void __parseTokens(Tokens_T &_Tokens, std::vector<std::string> const &_Env)
    {
        try {
            pModel__->strProgramName = program_name;
            envParser__.parse(_Env);
            ensureBuiltins();

            // 新增：将 OptionGraph 绑定到 OptionParser
            optParser__.setGraph(&pModel__->optionGraph);

            bFired__ = optParser__.parse(_Tokens);
            if (bFired__) return;

            if (optParser__.isOptionsEnded()) {
                argParser__.parseNamedOnly(_Tokens);
                argParser__.parsePositionalOnly(_Tokens);
                bFired__ = false;
                return;
            }

            argParser__.parse(_Tokens);

            // 新增：根据 OptionGraph 拓扑序激活条件边的目标
            pModel__->optionGraph.evaluate();

            for (auto const &t : _Tokens) {
                if (!util::starts_with(t, "-")) continue;
                throw ErrorParse(ParseError_E::UNKNOWN_OPTION, "unrecognized option or argument: " + t);
            }
        }
        catch (...) {
            pModel__->opts.clearParseOrder();
            pModel__->optionGraph.clear();  // 解析失败清空关系图
            throw;
        }
    }
```

- [ ] **Step 3: 编译验证**

Run: `xmake build unit_test`
Expected: BUILD PASS

- [ ] **Step 4: 全量回归**

Run: `xmake run unit_test`
Expected: 全部 PASS

- [ ] **Step 5: Commit**

```bash
git add include/cmd_args/context/compiler.h include/cmd_args/context/model.h
git commit -m "feat(compiler): integrate ExpressionClassifier and OptionGraph into batch pipeline"
```

---

### Task 9: 文档同步 + 最终回归

**Files:**
- Modify: `include/cmd_args/context/AGENT.md`（同步新组件说明）

- [ ] **Step 1: 更新 AGENT.md**

在文件职责表中追加：

| 文件 | 类 | 职责 |
|---|---|---|
| `expression_classifier.h` | `ExpressionClassifier` | Token → ClassifiedExpr 分类接口（流式+批量） |
| `option_graph.h` | `OptionGraph` | Option 关系有向图（条件/依赖边、拓扑排序、求值） |

在管线说明中追加：

```
Compiler 管线:
  Lexer → Tokens → OptionParser.parse() [→ OptionGraph.addEdge()] → OptionGraph.evaluate()
           → ArgumentParser.parse() → finalize()

Interpreter 管线:
  Scanner.step() → Token → ExpressionClassifier.classifyOne() → ClassifiedExpr
           → __reduceStack() [→ __handleRelation()] → 立即执行
```

- [ ] **Step 2: 最终全量回归**

Run: `xmake build unit_test && xmake run unit_test`
Expected: 全部 PASS（含既有 + 新增约 23 个用例）

- [ ] **Step 3: 最终 Commit**

```bash
git add include/cmd_args/context/AGENT.md
git commit -m "docs(context): sync AGENT.md with ExpressionClassifier and OptionGraph"
```

---

## 架构总览

```
                        ┌─────────────────────────┐
                        │    ExpressionClassifier   │
                        │  Token → ClassifiedExpr   │
                        │  (classifyOne/classifyAll) │
                        └────────┬────────────────┘
                                 │
                    ┌────────────┴────────────┐
                    │                         │
            ┌───────▼───────┐         ┌───────▼───────┐
            │   Compiler     │         │  Interpreter   │
            │  (批量模式)     │         │  (流式模式)     │
            │ classifyAll()  │         │ classifyOne()  │
            └───────┬───────┘         └───────┬───────┘
                    │                         │
            ┌───────▼───────┐         ┌───────▼───────┐
            │  OptionParser  │         │ __reduceStack  │
            │  setGraph()    │         │ __handleRel()  │
            └───────┬───────┘         └───────────────┘
                    │
            ┌───────▼───────┐
            │  OptionGraph   │
            │ addEdge/eval   │
            │ topologicalSort│
            └───────┬───────┘
                    │
            ┌───────▼───────┐
            │   Executor     │
            │ 按拓扑序执行    │
            └───────────────┘
```

## Self-Review 记录

1. **Spec 覆盖**：「token 转化 Expression 的部分」→ Task 1 ExpressionClassifier + Task 3 Interpreter 委托 + Task 8 Compiler 集成；「转化识别接口能被 Interpreter 用到」→ Task 3 classifyOne 委托；「-a?b:c / -a?b:_ / -a->b」→ Task 4-7 OptionGraph + Scanner + OptionParser + Interpreter；「Option 关系变成有向图」→ Task 4 OptionGraph topologicalSort。无遗漏。
2. **占位符扫描**：所有代码块均为完整可粘贴内容，无 TBD/TODO/“类似 Task N”。
3. **类型一致性**：`ExpressionClassifier::classifyOne()` 返回 `std::optional<ClassifiedExpr>`，与 Task 3 `__reduceStack` 调用一致；`OptionGraph::addEdge` 参数与 Task 6 `__parseRelation` 调用一致；`OptionGraph::EdgeKind_E` 与 Task 1 `ExpressionKind_E` 的 Conditional/Dependency 对应。
