# Parser 内置 Expression 识别规则重构计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use subagent-driven-development (recommended) or executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 OptionParser/ArgumentParser/EnvParser 中硬编码的 token 消费逻辑拆分为「识别规则（recognize）」与「应用（apply）」两层，使每个 Parser 内置自己的 Expression 识别规则，达到逻辑解耦。ExpressionRecognizer 组合所有 Parser 规则，Interpreter 和 Compiler 通过 ExpressionRecognizer 获得 Expression 后再 apply 到 Model。

**Architecture:** 每个 Parser 新增 `recognize()` 方法，接收 token（或 token 序列），返回 `std::optional<ClassifiedExpr>`。`parse()` 方法改为调用 `recognize()` + `apply()`。ExpressionRecognizer 持有三个 Parser 的引用，按优先级调用各 Parser 的 `recognize()` 方法。

**Tech Stack:** C++20（header-only）、xmake、Catch2

**Spec:** 用户需求原文——"ArgumentParser,OptionParser,EnvParser,应该能够识别后输出Expression, 所以他们应该内置对应Expression的规则，达到逻辑解耦的目的，这样在Interpreter继承Parser的时候，结合ExpressionRecognizer,对输入的信息能够判断出Expression，Compiler也一样，尽可能去掉对字节流的硬编码识别。"

已确认的设计决策：
1. **Parser 规则化**：每个 Parser 新增 `recognize()` 方法，返回 `std::optional<ClassifiedExpr>`。`recognize()` 只做识别，不修改 Model。
2. **Parser 应用化**：每个 Parser 新增 `apply(ClassifiedExpr const&)` 方法，将识别结果写入 Model。
3. **parse() 重构**：`parse()` 改为循环调用 `recognize()` + `apply()`，不再硬编码 token 消费。
4. **ExpressionRecognizer 委托**：ExpressionRecognizer 持有三个 Parser 引用，`feed()` 按优先级调用各 Parser 的 `recognize()`。
5. **兼容铁律**：既有测试全部通过，公开 API 行为不变。

## Global Constraints

- 语言标准 C++20；警告即错误；header-only；成员命名规范同前。
- 排除区：`dfx/internal/**` 不得触碰。
- 构建：`xmake build unit_test`；回归：`xmake run unit_test`。

---

## 文件结构总览

```
include/cmd_args/
├── option/option_parser.h       [改] 新增 recognize() + apply()，parse() 改为规则驱动
├── argument/argument_parser.h   [改] 新增 recognize() + apply()，parse() 改为规则驱动
├── env/env_parser.h             [改] 新增 recognize() + apply()，parse() 改为规则驱动
├── context/expression_classifier.h [改] ExpressionRecognizer 委托 Parser 规则
├── context/interpreter.h        [改] __reduceStack 使用 ExpressionRecognizer
└── context/compiler.h           [改] __parseTokens 使用 ExpressionRecognizer

dfx/unit_test/
├── test_option_parser_rules.cpp   [新] OptionParser 规则测试
├── test_argument_parser_rules.cpp [新] ArgumentParser 规则测试
└── test_env_parser_rules.cpp      [新] EnvParser 规则测试
```

---

### Task 1: OptionParser — recognize() + apply() + parse() 重构

**Files:**
- Modify: `include/cmd_args/option/option_parser.h`
- Test: `dfx/unit_test/test_option_parser_rules.cpp`（新建）

**Interfaces:**
- Consumes: `Tokens_T::iterator`（当前 token）、`Tokens_T`（完整流，用于消费 value）
- Produces: `std::optional<ClassifiedExpr>`（recognize 返回）、写入 OptionData（apply）

- [ ] **Step 1: 新增 recognize() 方法**

```cpp
/// @brief 识别当前迭代器指向的 token 是否匹配选项规则
/// @param _It 当前 token 迭代器（识别成功后不消费，由 parse/apply 消费）
/// @param _Tokens 完整 token 流（用于查看 next token 是否为 value）
/// @return 识别成功返回 ClassifiedExpr，否则 nullopt
auto recognize(Tokens_T::iterator _It, Tokens_T const &_Tokens) const -> std::optional<ClassifiedExpr>
{
    if (_It == _Tokens.end()) return std::nullopt;
    std::string const &tok = *_It;

    // -- 终止选项区
    if (tok == "--") return std::nullopt;  // 由 parse 处理

    // --name[=value] 长选项
    if (util::starts_with(tok, "--") && tok.size() > 2) {
        auto body = tok.substr(2);
        auto eq   = body.find('=');
        ClassifiedExpr expr;
        expr.kind      = ExpressionKind_E::Option;
        expr.bExplicit = true;
        expr.name      = (eq == std::string::npos) ? body : body.substr(0, eq);
        if (eq != std::string::npos) { expr.value = body.substr(eq + 1); }
        return expr;
    }

    // -x / -abc 短选项
    if (util::starts_with(tok, "-") && tok.size() >= 2) {
        // 关系语法检测
        if (__isRelationToken(tok)) {
            ClassifiedExpr expr;
            expr.bExplicit = true;
            // 解析关系语法
            auto qPos = tok.find('?');
            auto dPos = tok.find("->");
            if (qPos != std::string::npos) {
                expr.kind         = ExpressionKind_E::Conditional;
                expr.strCondition = tok.substr(1, qPos - 1);
                auto colonPos     = tok.find(':', qPos);
                if (colonPos != std::string::npos) {
                    expr.strThenBranch = tok.substr(qPos + 1, colonPos - qPos - 1);
                    expr.strElseBranch = tok.substr(colonPos + 1);
                } else {
                    expr.strThenBranch = tok.substr(qPos + 1);
                    expr.strElseBranch = "_";
                }
            } else if (dPos != std::string::npos) {
                expr.kind         = ExpressionKind_E::Dependency;
                expr.strDepSource = tok.substr(1, dPos - 1);
                expr.strDepTarget = tok.substr(dPos + 2);
            }
            return expr;
        }

        ClassifiedExpr expr;
        expr.kind       = ExpressionKind_E::Option;
        expr.cShortName = tok[1];
        expr.name       = std::string(1, tok[1]);
        expr.bExplicit  = true;
        expr.bIsBool    = true;
        return expr;
    }

    return std::nullopt;  // 不匹配选项规则
}
```

- [ ] **Step 2: 新增 apply() 方法**

```cpp
/// @brief 将识别结果应用到 OptionData（赋值 + 标记 + 回调触发）
void apply(ClassifiedExpr const &_Expr, Tokens_T::iterator &_It, Tokens_T &_Tokens)
{
    if (_Expr.kind == ExpressionKind_E::Conditional || _Expr.kind == ExpressionKind_E::Dependency) {
        // 关系语法：注册到 OptionGraph
        if (pGraph__) __parseRelation(*_It);
        _It = _Tokens.erase(_It);
        return;
    }

    // 普通选项：查找注册的选项
    auto *pOpt = pData__->mgrValues.mutFind(_Expr.name);
    if (!pOpt && _Expr.cShortName != '\0') {
        pOpt = pData__->mgrValues.mutFind(_Expr.cShortName);
    }
    if (!pOpt) return;  // 未注册，留给上层

    std::string value = _Expr.value;

    // 非 bool 且无内联值：消费下一个 token
    if (value.empty() && pOpt->strType != "bool") {
        auto nx = std::next(_It);
        if (nx != _Tokens.end() && !util::starts_with(*nx, "-")) {
            value = *nx;
            _Tokens.erase(nx);
        }
    }

    __assign(pOpt, value, pOpt->strType == "bool");
    _It = _Tokens.erase(_It);
}
```

- [ ] **Step 3: 重构 parse() 使用 recognize + apply**

```cpp
bool parse(Tokens_T &_Tokens)
{
    bFired__  = false;
    bOptEnd__ = false;
    if (pData__) { pData__->clearParseOrder(); }

    for (auto it = _Tokens.begin(); it != _Tokens.end();) {
        // -- 终止选项区
        if (*it == "--") {
            _Tokens.erase(it);
            bOptEnd__ = true;
            break;
        }

        // 识别：调用内置规则
        auto expr = recognize(it, _Tokens);
        if (!expr) {
            ++it;  // 不匹配选项规则，留给参数解析
            continue;
        }

        // 短回路检测
        if (__fireOnLong(expr->name, it, _Tokens)) return true;

        // 应用：写入 OptionData
        apply(*expr, it, _Tokens);
        if (bFired__) return true;
    }

    return bFired__;
}
```

- [ ] **Step 4: 写测试 test_option_parser_rules.cpp**

```cpp
#include "catch2/catch.hpp"
#include "cmd_args/cmd_args.hpp"
#include "cmd_args/context/expression_classifier.h"

using namespace cmd_args;

TEST_CASE("option rules: recognize 长选项", "[option_rules]")
{
    Model model;
    OptionBuilder ob(&model.opts);
    ob.addOption<int>("timeout", "t", 30);
    OptionParser op(&model.opts);

    Tokens_T toks{ "--timeout", "9" };
    auto expr = op.recognize(toks.begin(), toks);
    REQUIRE(expr.has_value());
    REQUIRE(expr->kind == ExpressionKind_E::Option);
    REQUIRE(expr->name == "timeout");
}

TEST_CASE("option rules: recognize 短选项", "[option_rules]")
{
    Model model;
    OptionBuilder ob(&model.opts);
    ob.addOption<int>("timeout", "t", 30);
    OptionParser op(&model.opts);

    Tokens_T toks{ "-t", "9" };
    auto expr = op.recognize(toks.begin(), toks);
    REQUIRE(expr.has_value());
    REQUIRE(expr->cShortName == 't');
}

TEST_CASE("option rules: recognize 关系语法", "[option_rules]")
{
    Model model;
    OptionBuilder ob(&model.opts);
    ob.addFlag('v', "verbose", "verbose");
    OptionParser op(&model.opts);

    Tokens_T toks{ "-verbose?debug:quiet" };
    auto expr = op.recognize(toks.begin(), toks);
    REQUIRE(expr.has_value());
    REQUIRE(expr->kind == ExpressionKind_E::Conditional);
}

TEST_CASE("option rules: parse 仍正常工作（回归）", "[option_rules]")
{
    Model model;
    OptionBuilder ob(&model.opts);
    ob.addOption<int>("timeout", "t", 30);
    OptionParser op(&model.opts);

    Tokens_T toks{ "--timeout", "9" };
    REQUIRE_FALSE(op.parse(toks));
    REQUIRE(model.opts.mgrValues.find("timeout")->strValue == "9");
}
```

- [ ] **Step 5: 编译并跑 option_rules 标签**
- [ ] **Step 6: 全量回归**
- [ ] **Step 7: Commit**

---

### Task 2: ArgumentParser — recognize() + apply() + parse() 重构

**Files:**
- Modify: `include/cmd_args/argument/argument_parser.h`
- Test: `dfx/unit_test/test_argument_parser_rules.cpp`（新建）

- [ ] **Step 1: 新增 recognize() 方法**

```cpp
/// @brief 识别当前迭代器指向的 token 是否匹配参数规则
auto recognize(Tokens_T::iterator _It, Tokens_T const &_Tokens) const -> std::optional<ClassifiedExpr>
{
    if (_It == _Tokens.end()) return std::nullopt;
    std::string const &tok = *_It;

    // --name=value / --name value 命名参数
    if (util::starts_with(tok, "--") && tok.size() > 2) {
        auto body = tok.substr(2);
        auto eq   = body.find('=');
        std::string name = (eq == std::string::npos) ? body : body.substr(0, eq);
        // 检查是否为已注册的命名参数
        if (pData__->mgrName.doGet(name)) {
            ClassifiedExpr expr;
            expr.kind      = ExpressionKind_E::Argument;
            expr.name      = name;
            expr.bExplicit = true;
            if (eq != std::string::npos) { expr.value = body.substr(eq + 1); }
            return expr;
        }
    }

    // 普通 token（非 - 开头）→ 方位参数
    if (!util::starts_with(tok, "-")) {
        ClassifiedExpr expr;
        expr.kind      = ExpressionKind_E::Argument;
        expr.name      = tok;
        expr.value     = tok;
        expr.bExplicit = true;
        return expr;
    }

    return std::nullopt;  // 不匹配参数规则（可能是选项）
}
```

- [ ] **Step 2: 新增 apply() 方法**

```cpp
/// @brief 将识别结果应用到 ArgumentData
void apply(ClassifiedExpr const &_Expr)
{
    if (_Expr.kind == ExpressionKind_E::Argument) {
        // 命名参数
        if (auto *p = pData__->mgrName.mutGet(_Expr.name)) {
            p->set(_Expr.value);
        }
    }
}
```

- [ ] **Step 3: 重构 parse() 使用 recognize + apply**

- [ ] **Step 4: 写测试**
- [ ] **Step 5: 编译并跑 argument_rules 标签**
- [ ] **Step 6: 全量回归**
- [ ] **Step 7: Commit**

---

### Task 3: EnvParser — recognize() + apply() + parse() 重构

**Files:**
- Modify: `include/cmd_args/env/env_parser.h`
- Test: `dfx/unit_test/test_env_parser_rules.cpp`（新建）

- [ ] **Step 1: 新增 recognize() 方法**

```cpp
/// @brief 识别环境变量串是否匹配 Env 规则
auto recognize(std::string const &_EnvStr) const -> std::optional<ClassifiedExpr>
{
    auto eq = _EnvStr.find('=');
    if (eq == std::string::npos) return std::nullopt;  // 无 '=' 不是合法环境项

    ClassifiedExpr expr;
    expr.kind      = ExpressionKind_E::Env;
    expr.name      = _EnvStr.substr(0, eq);
    expr.value     = _EnvStr.substr(eq + 1);
    expr.bExplicit = true;
    return expr;
}
```

- [ ] **Step 2: 新增 apply() 方法**

```cpp
/// @brief 将识别结果应用到 EnvData
void apply(ClassifiedExpr const &_Expr)
{
    pData__->doPush(_Expr.name, _Expr.value, false);
}
```

- [ ] **Step 3: 重构 parse() 使用 recognize + apply**

- [ ] **Step 4: 写测试**
- [ ] **Step 5: 编译并跑 env_rules 标签**
- [ ] **Step 6: 全量回归**
- [ ] **Step 7: Commit**

---

### Task 4: ExpressionRecognizer 委托 Parser 规则

**Files:**
- Modify: `include/cmd_args/context/expression_classifier.h`

- [ ] **Step 1: ExpressionRecognizer 持有三个 Parser 引用**

```cpp
class ExpressionRecognizer
{
public:
    ExpressionRecognizer(OptionParser *_Opt, ArgumentParser *_Arg, EnvParser *_Env)
        : pOptParser__(_Opt)
        , pArgParser__(_Arg)
        , pEnvParser__(_Env)
    {}

    // feed() 内部调用各 Parser 的 recognize()
    auto feed(detail::Token const &_Tok) -> std::optional<ClassifiedExpr>
    {
        // 按优先级尝试各 Parser 规则
        if (pOptParser__) {
            // 构造临时 token 列表供 recognize 使用
            Tokens_T tmp{ _Tok.toText() };
            auto expr = pOptParser__->recognize(tmp.begin(), tmp);
            if (expr) return expr;
        }
        // ... 类似调用 pArgParser__, pEnvParser__
    }

private:
    OptionParser  *pOptParser__ = nullptr;
    ArgumentParser *pArgParser__ = nullptr;
    EnvParser     *pEnvParser__ = nullptr;
};
```

- [ ] **Step 2: 移除 ExpressionRecognizer 中硬编码的识别逻辑**
- [ ] **Step 3: 全量回归**
- [ ] **Step 4: Commit**

---

### Task 5: Interpreter + Compiler 使用新架构

**Files:**
- Modify: `include/cmd_args/context/interpreter.h`
- Modify: `include/cmd_args/context/compiler.h`

- [ ] **Step 1: Interpreter 构造 ExpressionRecognizer 并注入 Parser**
- [ ] **Step 2: Compiler 构造 ExpressionRecognizer 并注入 Parser**
- [ ] **Step 3: 全量回归**
- [ ] **Step 4: Commit**

---

### Task 6: 文档同步 + 最终回归

- [ ] **Step 1: 更新 AGENT.md**
- [ ] **Step 2: 最终全量回归**
- [ ] **Step 3: Commit**

---

## 架构总览

```
                    ┌─────────────────────────────┐
                    │     ExpressionRecognizer      │
                    │  持有三个 Parser 引用          │
                    │  feed() → 调用各 Parser 规则  │
                    └─────────────┬───────────────┘
                                  │
                    ┌─────────────┴───────────────┐
                    │                             │
        ┌───────────▼──────────┐     ┌────────────▼──────────┐
        │   OptionParser       │     │   ArgumentParser       │
        │ recognize() → Opt    │     │ recognize() → Arg      │
        │ apply() → OptionData │     │ apply() → ArgumentData │
        └──────────────────────┘     └────────────────────────┘
                    │
        ┌───────────▼──────────┐
        │   EnvParser          │
        │ recognize() → Env    │
        │ apply() → EnvData    │
        └──────────────────────┘
```

**关键设计：**
- 每个 Parser **内置自己的识别规则**（recognize 方法）
- 每个 Parser **内置自己的应用逻辑**（apply 方法）
- ExpressionRecognizer **组合所有 Parser 规则**
- Interpreter/Compiler **通过 ExpressionRecognizer 获得 Expression**
- **零硬编码**：识别逻辑全部在 Parser 内部
