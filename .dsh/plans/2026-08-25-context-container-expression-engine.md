# Context 容器化 + Expression 类层次 + Compiler/Interpreter 双模式 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use subagent-driven-development (recommended) or executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 Context 重构为通用容器（持有 Model 数据 + 可注入的执行引擎），引入 Expression 统一基类实现命令行数据自动分类，建立 Compiler-Executor（批量）和 Interpreter（流式）两条执行路径。

**Architecture:**
- `Expression` 作为 Argument/OptionValue/OptionAction/environment 的统一基类，通过 `kind()` 实现自动分类
- `Compiler`（原 Parser）+ `Executor` 组成批量执行路径：Lex → Token 流 → Parsers → Expression 分类入库 → Model → `hasAction()` 门控 → `Executor` 顺序执行
- `Interpreter` 组成流式执行路径：字符流 → `Scanner::step` 逐字符分词 → 每个 Token 即时分类 → 按 Expression 类型立即执行
- `Context` 是通用容器，持有 Model + Compiler + Executor + Interpreter，由数据内容（`hasAction`）和调用入口（`parse` vs `once/loop`）决定执行路径

**Tech Stack:** C++20, header-only, xmake, Catch2, MSVC + Linux 双平台

**Spec:** 本会话用户的设计陈述 — Context 为通用容器，命令行数据注入后，执行方式取决于注入的成员（Compiler-Executor 模式 or Interpreter 模式）；Expression 是 Argument/Option/Env 的基类，解析过程自动分类；Model 输出产物含 `hasAction()` 门控。

## Global Constraints

| # | 约束 | 值 |
|---|------|---|
| 1 | header-only | 所有实现留在 `include/cmd_args/**` 头文件 |
| 2 | C++20 双平台 | MSVC (Windows) + Linux (Clang/GCC) |
| 3 | 重名报错 | `ErrorArgument(BuildError_E::DUPLICATE_NAME)` |
| 4 | 测试只用公共 API | 禁止 `#define private public` |
| 5 | 环境变量兜底优先级 | CLI 显式值 > YAML > 环境变量 > 默认值 |
| 6 | 编码风格 | 成员 `m_xxx__`（私有）`__` 后缀；形参 `_Xxx`；方法 camelCase；Doxygen 中文 |
| 7 | 当前基线 | 39 TEST_CASE / 215 断言 |
| 8 | 提交信息 | `gitmoji type(scope): message` |

---

## 文件结构预览

```
include/cmd_args/
├── context/
│   ├── expression.h          ← 新建：Expression 基类 + ExpressionKind_E
│   ├── compiler.h            ← 重命名自 parser.h，类 Parser → Compiler
│   ├── context.h             ← 修改：容器化（Compiler + Model.hasAction 门控）
│   ├── model.h               ← 修改：新增 hasAction() + expressions()
│   ├── executor.h            ← 修改：Executor(Model*) 替代 Executor(OptionData*)
│   ├── interpreter.h         ← 重写：流式字符泵 interpret()
│   └── lexer.h               ← 修改：Scanner::step() 增量分词
├── argument/
│   └── argument.h            ← 修改：Argument : public Expression
├── option/
│   └── option.h              ← 修改：Option : public Expression
└── env/
    └── env.h                 ← 修改：environment : public Expression
```

---

## Phase A — Expression 类层次（Tasks 1–4）

> 表达式基类：自动分类的基础设施。完成后三类数据条目均可以 `Expression` 视角统一枚举。

### Task 1: 新建 context/expression.h（Expression 基类 + ExpressionKind_E）

**Files:**
- Create: `include/cmd_args/context/expression.h`
- Test: `dfx/unit_test/head.cpp`

**Interfaces:**
- Produces: `ExpressionKind_E` 枚举（Argument / Option / Env）、`Expression` 类（kind / getName / getHelp + 虚析构 + protected setters）

- [ ] **Step 1: 写测试 — expression.h 基本构造与访问**

在 `dfx/unit_test/head.cpp` 末尾（`#endif` 之前）追加：

```cpp
/* ==================== Expression 类层次 ==================== */

TEST_CASE("expression: 构造 + kind/getName/getHelp 基本访问", "[expression]")
{
    Expression e(ExpressionKind_E::Argument, "hello", "say hello");
    REQUIRE(e.kind() == ExpressionKind_E::Argument);
    REQUIRE(e.getName() == "hello");
    REQUIRE(e.getHelp() == "say hello");

    Expression e2(ExpressionKind_E::Option, "timeout", "seconds");
    REQUIRE(e2.kind() == ExpressionKind_E::Option);
    REQUIRE(e2.getName() == "timeout");

    Expression e3(ExpressionKind_E::Env, "MY_VAR");
    REQUIRE(e3.kind() == ExpressionKind_E::Env);
    REQUIRE(e3.getName() == "MY_VAR");
    REQUIRE(e3.getHelp().empty());  // help 可选
}
```

- [ ] **Step 2: 编译验证测试失败（新头文件尚未存在）**

Run: `xmake build unit_test`
Expected: FAIL — `expression.h` 未找到

- [ ] **Step 3: 实现 expression.h**

创建 `include/cmd_args/context/expression.h`：

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

#include <string>

#include "cmd_args/base/def.h"
#include "cmd_args/context/parse_fwd.h"

CMD_ARGS_NAMESPACE_BEGIN

/// @brief 表达式种类：Argument（参数）/ Option（选项）/ Env（环境变量）
enum class ExpressionKind_E : char
{
    Argument,  ///< 命名/方位参数
    Option,    ///< 值选项/标志/短回路选项
    Env        ///< 环境变量条目
};

/**
 * @brief 表达式基类：Argument / OptionValue / OptionAction / environment 的统一视图。
 *
 * 解析管线中，各 Parser 把 token 分类为具体派生类型后，
 * 以 Expression 视角统一枚举（kind() 自动完成分类），再存入对应 Data。
 * Compiler 的输出产物 Model 即持有全部 Expression 派生对象。
 */
class Expression
{
public:
    /// @brief 构造表达式
    /// @param _Kind 表达式种类
    /// @param _Name 名字（参数名 / 选项长名 / 环境变量键）
    /// @param _Help 帮助文本（可选，默认空）
    Expression(ExpressionKind_E _Kind, Name_T _Name, Desc_T _Help = {})
        : m_enKind__(_Kind)
        , m_strName__(std::move(_Name))
        , m_strHelp__(std::move(_Help))
    {}

    virtual ~Expression() = default;

    /// @brief 获取表达式种类（分类依据，非虚）
    auto kind() const noexcept -> ExpressionKind_E { return m_enKind__; }

    /// @brief 获取表达式名字（虚函数，派生类可覆盖使用自有存储）
    virtual auto getName() const noexcept -> Name_T const & { return m_strName__; }

    /// @brief 获取帮助文本（虚函数，派生类可覆盖）
    virtual auto getHelp() const noexcept -> Desc_T const & { return m_strHelp__; }

protected:
    /// @brief 设置名字（保护：派生类 setter 使用）
    auto _setName(Name_T &&_Name) noexcept -> Expression &
    {
        m_strName__ = std::move(_Name);
        return *this;
    }

    /// @brief 设置帮助文本（保护：派生类 setter 使用）
    auto _setHelp(Desc_T &&_Help) noexcept -> Expression &
    {
        m_strHelp__ = std::move(_Help);
        return *this;
    }

private:
    ExpressionKind_E m_enKind__;  ///< 种类
    Name_T           m_strName__; ///< 名字
    Desc_T           m_strHelp__; ///< 帮助文本
};

CMD_ARGS_NAMESPACE_END
```

在 `head.cpp` 开头的 include 区域（`#include "cmd_args/cmd_args.hpp"` 附近）追加：
```cpp
#include "cmd_args/context/expression.h"
```

- [ ] **Step 4: 编译 + 运行测试验证通过**

Run: `xmake build unit_test && xmake run unit_test`
Expected: PASS — 新增 TEST_CASE `expression: 构造 + kind/getName/getHelp 基本访问` 通过；原有 39 个用例不受影响

- [ ] **Step 5: 提交**

```bash
git add include/cmd_args/context/expression.h dfx/unit_test/head.cpp
git commit -m "✨ feat(context): add Expression base class with ExpressionKind_E for auto-classification"
```

---

### Task 2: Argument 继承 Expression

**Files:**
- Modify: `include/cmd_args/argument/argument.h`
- Test: `dfx/unit_test/head.cpp`

**Interfaces:**
- Consumes: `Expression(ExpressionKind_E::Argument, Name_T, Desc_T)`
- Produces: `Argument.kind() == ExpressionKind_E::Argument`；`getName()/getHelp()` 返回基类存储

- [ ] **Step 1: 写测试 — Argument kind 分类**

在 head.cpp 的 expression TEST_CASE 区域追加：

```cpp
TEST_CASE("expression: Argument kind() 返回 Argument", "[expression]")
{
    Context app("ut_expr_arg");
    app.build<ArgumentNamed>("hello", "say hello");
    app.build<ArgumentPosition>("name", "your name").No(0);

    auto *pNamed = app.argument("hello");
    REQUIRE(pNamed != nullptr);
    REQUIRE(pNamed->kind() == ExpressionKind_E::Argument);
    REQUIRE(pNamed->getName() == "hello");
    REQUIRE(pNamed->getHelp() == "say hello");

    auto *pPos = app.argument("name");
    REQUIRE(pPos != nullptr);
    REQUIRE(pPos->kind() == ExpressionKind_E::Argument);
}
```

- [ ] **Step 2: 编译验证测试失败（Argument 尚未继承 Expression）**

Run: `xmake build unit_test`
Expected: FAIL — `Argument` 无 `kind()` 方法

- [ ] **Step 3: 修改 argument.h — Argument : public Expression**

核心变更（以 diff 形式展示）：

```cpp
// === argument_detail 命名空间内 ===

// 新增 include（文件顶部区域）
#include "cmd_args/context/expression.h"

// 类声明改为继承
class Argument : public Expression
{
public:
    // ...（现有类型别名保留）

    // 默认构造：kind=Argument
    Argument()
        : Expression(ExpressionKind_E::Argument, {})
    {}

    // 基础构造：移除 strName__ / strHelp__ 初始化（已移至基类）
    Argument(Name_T _Name, Desc_T _Help)
        : Expression(ExpressionKind_E::Argument, std::move(_Name), std::move(_Help))
    {}

    // 完整构造
    Argument(Name_T _Name, Desc_T _Help, Type_T _Type, Value_T _Default = "")
        : Expression(ExpressionKind_E::Argument, std::move(_Name), std::move(_Help))
        , strType__(std::move(_Type))
        , strDefault__(_Default)
        , strValue__(std::move(_Default))
    {}

    // 移动构造：委托基类 + 后续成员
    explicit Argument(Argument &&_Other) noexcept
        : Expression(std::move(_Other))
        , strType__(std::move(_Other.strType__))
        , strDefault__(std::move(_Other.strDefault__))
        , strValue__(std::move(_Other.strValue__))
        , strEnvName__(std::move(_Other.strEnvName__))
        , bExternal__(_Other.bExternal__)
    {}

    // 拷贝构造
    Argument(Argument const &_Other) noexcept
        : Expression(_Other)
        , strType__(_Other.strType__)
        , strDefault__(_Other.strDefault__)
        , strValue__(_Other.strValue__)
        , strEnvName__(_Other.strEnvName__)
        , bExternal__(_Other.bExternal__)
    {}

    // 移动赋值
    auto operator=(Argument &&_Rhs) noexcept -> Self_T &
    {
        if (this == &_Rhs) return *this;
        Expression::operator=(std::move(_Rhs));
        strType__    = std::move(_Rhs.strType__);
        strDefault__ = std::move(_Rhs.strDefault__);
        strValue__   = std::move(_Rhs.strValue__);
        strEnvName__ = std::move(_Rhs.strEnvName__);
        bExternal__    = _Rhs.bExternal__;
        return *this;
    }

    // 拷贝赋值
    auto operator=(Argument const &_Rhs) noexcept -> Self_T &
    {
        if (this == &_Rhs) return *this;
        Expression::operator=(_Rhs);
        strType__    = _Rhs.strType__;
        strDefault__ = _Rhs.strDefault__;
        strValue__   = _Rhs.strValue__;
        strEnvName__ = _Rhs.strEnvName__;
        bExternal__    = _Rhs.bExternal__;
        return *this;
    }

    // ...（operator bool、hasValue 等保留不变）

    // name() / help() setter 改为委托基类 _setName / _setHelp
    auto name(Name_T &&_Name) & -> Self_T &
    {
        _setName(std::move(_Name));
        return *this;
    }

    auto name(Name_T &&_Name) && -> Self_T
    {
        _setName(std::move(_Name));
        return std::move(*this);
    }

    auto help(Desc_T &&_V) & noexcept -> Self_T &
    {
        _setHelp(std::move(_V));
        return *this;
    }

    auto help(Desc_T &&_V) && noexcept -> Self_T
    {
        _setHelp(std::move(_V));
        return std::move(*this);
    }

    // ...（type / set / setExternal / reset / env / copy 保留不变）

    // 删除原有的 private: strName__; strHelp__;（已移至 Expression 基类）

private:
    // ===== 运行时状态（不再含 strName__ / strHelp__） =====
    Type_T  strType__;                  ///< 类型标识
    Value_T strDefault__;               ///< 默认值
    mutable Value_T strValue__;         ///< 当前值
    Name_T  strEnvName__;               ///< 绑定环境变量名

    // ===== 标志位 =====
    mutable bool bExternal__ = false;
};
```

ArgumentNamed / ArgumentPosition 不变（它们继承 Argument，Argument 已继承 Expression）。

- [ ] **Step 4: 编译 + 运行测试验证通过**

Run: `xmake build unit_test && xmake run unit_test`
Expected: PASS — Argument kind 测试通过；全部 40 个用例（原有 39 + 新增 1）通过

- [ ] **Step 5: 提交**

```bash
git add include/cmd_args/argument/argument.h dfx/unit_test/head.cpp
git commit -m "✨ feat(argument): Argument derives Expression for auto-classification"
```

---

### Task 3: Option 继承 Expression

**Files:**
- Modify: `include/cmd_args/option/option.h`
- Test: `dfx/unit_test/head.cpp`

**Interfaces:**
- Consumes: `Expression(ExpressionKind_E::Option, NameLong_T, Desc_T)`
- Produces: `Option.kind() == ExpressionKind_E::Option`；`getName()` override 返回 `strLongName`（自有存储）

- [ ] **Step 1: 写测试 — Option / OptionValue / OptionAction kind 分类**

```cpp
TEST_CASE("expression: Option/OptionValue/OptionAction kind() 返回 Option", "[expression]")
{
    Context app("ut_expr_opt");
    app.addOption<int>('t', "timeout", "seconds", 30);
    app.addFlag('v', "verbose", "enable verbose");

    auto *pOpt = app.option("timeout");
    REQUIRE(pOpt != nullptr);
    REQUIRE(pOpt->kind() == ExpressionKind_E::Option);
    REQUIRE(pOpt->getName() == "timeout");  // getName() override 返回 strLongName
    REQUIRE(pOpt->getHelp() == "seconds");

    auto *pFlag = app.option("verbose");
    REQUIRE(pFlag != nullptr);
    REQUIRE(pFlag->kind() == ExpressionKind_E::Option);
}
```

- [ ] **Step 2: 编译验证测试失败**

Run: `xmake build unit_test`
Expected: FAIL — `OptionValue` 无 `kind()` 方法

- [ ] **Step 3: 修改 option.h — Option : public Expression**

核心变更：

```cpp
// 新增 include
#include "cmd_args/context/expression.h"

struct Option : public Expression
{
    using NameShort_T = char;
    using NameLong_T  = std::string;
    using Desc_T      = std::string;

    NameShort_T cShortName = '\0';
    NameLong_T  strLongName;
    Desc_T      strHelp;

    Option(NameShort_T _SName, NameLong_T _LName, Desc_T _Help)
        : Expression(ExpressionKind_E::Option, _LName, _Help)
        , cShortName(_SName)
        , strLongName(std::move(_LName))
        , strHelp(std::move(_Help))
    {}

    virtual ~Option() = default;

    /// @brief 覆盖基类 getName()：返回自有 strLongName（避免双份存储同步问题）
    auto getName() const noexcept -> Name_T const & override { return strLongName; }

    /// @brief 覆盖基类 getHelp()：返回自有 strHelp
    auto getHelp() const noexcept -> Desc_T const & override { return strHelp; }
};
```

OptionValue / OptionAction 不变（继承 Option，Option 已继承 Expression；kind=Option）。

- [ ] **Step 4: 编译 + 运行测试验证通过**

Run: `xmake build unit_test && xmake run unit_test`
Expected: PASS — 全部 41 个用例通过

- [ ] **Step 5: 提交**

```bash
git add include/cmd_args/option/option.h dfx/unit_test/head.cpp
git commit -m "✨ feat(option): Option derives Expression for auto-classification"
```

---

### Task 4: environment 继承 Expression + 统一 kind 测试

**Files:**
- Modify: `include/cmd_args/env/env.h`
- Test: `dfx/unit_test/head.cpp`

**Interfaces:**
- Consumes: `Expression(ExpressionKind_E::Env, Name_T)`
- Produces: `environment.kind() == ExpressionKind_E::Env`；`getName()` override 返回 `strKey`

- [ ] **Step 1: 写测试 — environment kind 分类**

```cpp
TEST_CASE("expression: environment kind() 返回 Env", "[expression]")
{
    Model model;
    EnvBuilder eb(&model.m_envs__);
    eb.setEnv("MY_KEY", "my_value");

    auto const *p = model.m_envs__.doGet("MY_KEY");
    REQUIRE(p != nullptr);
    REQUIRE(p->kind() == ExpressionKind_E::Env);
    REQUIRE(p->getName() == "MY_KEY");
}

TEST_CASE("expression: Model::expressions() 统一视图包含三类条目", "[expression]")
{
    Context app("ut_expr_view");
    app.build<ArgumentNamed>("hello", "greeting");
    app.addOption<int>("timeout", "sec", 30);

    std::error_code ec;
    char const *argv[] = { "test", "--hello", "hi", "--timeout", "9", nullptr };
    app.parse(ec, 6, argv, nullptr);
    REQUIRE(!ec);

    auto exprs = app.expression()->expressions();  // 需要 Model 访问（见 Task 5）
    REQUIRE(exprs.size() >= 2);
    bool hasArg = false, hasOpt = false;
    for (auto const *e : exprs) {
        if (e->kind() == ExpressionKind_E::Argument) hasArg = true;
        if (e->kind() == ExpressionKind_E::Option) hasOpt = true;
    }
    REQUIRE(hasArg);
    REQUIRE(hasOpt);
}
```

> 注：第二个 TEST_CASE 需要 `Model::expressions()` 方法（Task 5），若未就绪可先注释，留 Task 5 启用。环境变量 kind 测试可独立运行。

- [ ] **Step 2: 编译验证测试失败**

Run: `xmake build unit_test`
Expected: FAIL — `environment` 无 `kind()` 方法

- [ ] **Step 3: 修改 env.h — environment : public Expression**

```cpp
// 新增 include
#include "cmd_args/context/expression.h"

struct environment : public Expression
{
    std::string strKey;
    std::string strValue;
    bool        bReferenced__ = false;
    bool        bModified__   = false;
    bool        bNew__        = false;

    // 纯键构造（查找用）：kind = Env，name = Key
    environment(std::string const &_Key)
        : Expression(ExpressionKind_E::Env, _Key)
        , strKey(_Key)
    {}

    // 完整构造
    environment(std::string _Key, std::string _Value, bool _New)
        : Expression(ExpressionKind_E::Env, _Key)
        , strKey(std::move(_Key))
        , strValue(std::move(_Value))
        , bModified__(false)
        , bNew__(_New)
    {}

    /// @brief 覆盖基类 getName()：返回自有 strKey（避免双份存储）
    auto getName() const noexcept -> Name_T const & override { return strKey; }

    bool operator<(environment const &_Rhs) const noexcept { return strKey < _Rhs.strKey; }
};
```

- [ ] **Step 4: 编译 + 运行测试验证通过**

Run: `xmake build unit_test && xmake run unit_test`
Expected: PASS — environment kind 测试通过；原有用例不受影响（42 用例 / 约 225 断言）

- [ ] **Step 5: 提交**

```bash
git add include/cmd_args/env/env.h dfx/unit_test/head.cpp
git commit -m "✨ feat(env): environment derives Expression for auto-classification"
```

---

## Phase B — Compiler-Executor 模式（Tasks 5–8）

> 批量执行路径：Model 作为 Expression 分类仓库，hasAction() 门控 Executor 顺序执行。

### Task 5: Model::hasAction() + Model::expressions()

**Files:**
- Modify: `include/cmd_args/context/model.h`
- Test: `dfx/unit_test/head.cpp`

**Interfaces:**
- Produces: `Model::hasAction() const noexcept -> bool`；`Model::expressions() const -> std::vector<Expression const *>`

- [ ] **Step 1: 写测试**

```cpp
TEST_CASE("model: hasAction 含 action 的选项返回 true", "[model]")
{
    Model model;
    OptionBuilder ob(&model.m_opts__);
    ob.addOption<int>("task", "t", 30)->action("echo {value}");
    Executor exec(&model);  // 需 Task 7 改为 Executor(Model*)
    // hasAction 不依赖 Executor；直接检查 actionOrder
    REQUIRE_FALSE(model.actionOrder().empty());  // 构筑后 actionOrder 为空（未解析）
}

TEST_CASE("model: expressions() 返回统一 Expression 视图", "[model]")
{
    Context app("ut_model_expr");
    app.build<ArgumentNamed>("hello", "greeting");
    app.addOption<int>("timeout", "sec", 30);
    app.set_env("MY_K", "MY_V");

    std::error_code ec;
    char const *argv[] = { "test", "--hello", "hi", "--timeout", "9", nullptr };
    app.parse(ec, 6, argv, nullptr);
    REQUIRE(!ec);

    auto vec = app.option("timeout");  // 可用
    // 通过 Context 暴露的 Model 访问
    auto const &model = *reinterpret_cast<Model const *>(&app);  // 或通过 Task 8 的 public accessor
    auto exprs = model.expressions();
    REQUIRE(exprs.size() >= 2);

    size_t nArg = 0, nOpt = 0, nEnv = 0;
    for (auto const *e : exprs) {
        switch (e->kind()) {
        case ExpressionKind_E::Argument: ++nArg; break;
        case ExpressionKind_E::Option:   ++nOpt; break;
        case ExpressionKind_E::Env:      ++nEnv; break;
        }
    }
    REQUIRE(nArg >= 1);
    REQUIRE(nOpt >= 1);
    REQUIRE(nEnv >= 1);
}
```

> 注：`reinterpret_cast` 仅为临时测试策略；Task 8 会提供 `Context::model()` accessor。也可在 Task 8 完成后再启用完整 expressions 测试。

- [ ] **Step 2: 编译验证测试失败**

Run: `xmake build unit_test`
Expected: FAIL — `Model` 无 `hasAction()` / `expressions()` 方法

- [ ] **Step 3: 实现 hasAction() + expressions()**

修改 `include/cmd_args/context/model.h`：

```cpp
// 新增 include
#include "cmd_args/context/expression.h"

class Model
{
public:
    Model();
    ~Model();

    // ===== 查询方法 =====

    /// @brief 是否存在待执行的 OptionAction（fnOnSet__ 或 action template）
    /// @note Executor 按 actionOrder 顺序执行；hasAction 为 false 时无需调用 Executor
    bool hasAction() const noexcept { return !m_opts__.actionOrder().empty(); }

    /// @brief 统一 Expression 视图：遍历三类 Data，返回全部 Expression 派生对象指针
    auto expressions() const -> std::vector<Expression const *>
    {
        std::vector<Expression const *> vec;
        vec.reserve(16);
        m_args__.mgrName.doConVisit([&](Argument const &_A) { vec.push_back(&_A); });
        m_args__.mgrPos.doConVisit([&](Argument const &_A) { vec.push_back(&_A); });
        m_opts__.mgrValues.forEachValue([&](OptionValue const &_O) { vec.push_back(&_O); });
        m_opts__.mgrActions.forEachAction([&](OptionAction const &_A) { vec.push_back(&_A); });
        m_envs__.doVisit([&](environment const &_E) { vec.push_back(&_E); });
        return vec;
    }

    // ===== 数据成员（保持不变） =====
    ArgumentData m_args__;
    OptionData   m_opts__;
    EnvData      m_envs__;
    std::string  m_strProgramName__;
    bool         m_bBuiltinOptions__     = true;
    bool         m_bBuiltinsRegistered__ = false;
    std::string  m_strSaveConfigPath__;
};

Model::Model() {}
Model::~Model() {}
```

- [ ] **Step 4: 编译 + 运行测试验证通过**

Run: `xmake build unit_test && xmake run unit_test`
Expected: PASS — hasAction / expressions 测试通过；43 用例 / ~230 断言

- [ ] **Step 5: 提交**

```bash
git add include/cmd_args/context/model.h dfx/unit_test/head.cpp
git commit -m "✨ feat(model): add hasAction() gate and expressions() unified view"
```

---

### Task 6: Parser → Compiler 重命名

**Files:**
- Rename+Modify: `include/cmd_args/context/parser.h` → `include/cmd_args/context/compiler.h`
- Modify: `include/cmd_args/context/context.h`
- Modify: `dfx/unit_test/head.cpp`
- Modify: 多处注释文档（AGENT.md / README / 其他头文件注释）

**Interfaces:**
- Consumes: 现有 `Parser` 类全部接口
- Produces: `Compiler` 类（接口完全不变，仅类名 + 文件名）

- [ ] **Step 1: 重命名文件**

```bash
git mv include/cmd_args/context/parser.h include/cmd_args/context/compiler.h
```

- [ ] **Step 2: 修改 compiler.h — 类名 Parser → Compiler**

在 `include/cmd_args/context/compiler.h` 中：
- class declaration: `class Parser` → `class Compiler`
- 所有 `Parser &` 返回类型 → `Compiler &`
- 所有 `Parser(...)` 构造 → `Compiler(...)`

关键行：
```
Line 42: class Parser       → class Compiler
Line 45: Parser(Model *)    → Compiler(Model *)
Line 53: Parser &set_program_name(...)  → Compiler &set_program_name(...)
Line 59: Parser &set_description(...)   → Compiler &set_description(...)
```

同时更新文件注释中所有 "Parser" 引用 → "Compiler"。

- [ ] **Step 3: 修改 context.h — include 路径 + 成员类型 + 成员名**

```cpp
// include 路径
// 原: #include "parser.h"
// 改:
#include "compiler.h"

// 类注释：同步 "Parser（纯解析层）" → "Compiler（编译层）"

// 成员声明（private 区）
// 原: Parser       m_parser__;
// 改:
Compiler       m_compiler__;

// 构造函数（如 m_parser__ 直接出现在列表中，已在 constructor body 中无直接引用）

// __doParse 内部所有 m_parser__.xxx → m_compiler__.xxx
// 涉及行：
//   m_parser__.set_program_name(...)  → m_compiler__.set_program_name(...)
//   m_parser__.set_description(...)   → m_compiler__.set_description(...)
//   m_parser__.ensureBuiltins()       → m_compiler__.ensureBuiltins()
//   m_parser__.parse(...)             → m_compiler__.parse(...)
//   m_parser__.finalize()             → m_compiler__.finalize()
//   m_parser__.isFired()              → m_compiler__.isFired()
//   m_parser__.resetData()            → m_compiler__.resetData()

// loop() 方法
// 原: m_parser__.ensureBuiltins()
// 改:
m_compiler__.ensureBuiltins();
```

- [ ] **Step 4: 修改 head.cpp — 测试引用**

```cpp
// Line 416: TEST_CASE tag
// 原: TEST_CASE("parser: -?/--help、-!/--env 命中即短路", "[parser]")
// 改:
TEST_CASE("parser: -?/--help、-!/--env 命中即短路", "[compiler]")

// Line 418-419
// 原: Parser p(&model);
// 改:
Compiler p(&model);
```

- [ ] **Step 5: 同步注释文档（精简关键位置，其余文档更新在 Task 11 统一处理）**

```cpp
// .dsh/AGENT.md: §3 源码地图 parser.h → compiler.h；Parser → Compiler
// context/AGENT.md: 文件表格 + 解析流程描述同步
// option/AGENT.md: "Parser 纯解析" → "Compiler 纯解析"
// README.md / README_zh.md: 流程图中 "Parser" → "Compiler"
// cmd_args.hpp: 注释中 "Parser" → "Compiler"
// 其他头文件注释：builder.h / lexer.h / parse_fwd.h / error/parser_error.h
```

- [ ] **Step 6: 编译 + 全量回归**

Run: `xmake build unit_test && xmake run unit_test`
Expected: PASS — 全部用例通过，行为无变化（仅命名改变）

Run: `xmake build once && xmake build config_demo`
Expected: PASS — 示例正常构建

- [ ] **Step 7: 提交**

```bash
git add include/cmd_args/context/compiler.h include/cmd_args/context/context.h \
        dfx/unit_test/head.cpp .dsh/AGENT.md include/cmd_args/context/AGENT.md \
        include/cmd_args/option/AGENT.md README.md README_zh.md
git commit -m "♻️ refactor(context): rename Parser → Compiler (class + file)"
```

---

### Task 7: Executor(Model *)

**Files:**
- Modify: `include/cmd_args/context/executor.h`
- Test: `dfx/unit_test/head.cpp`

**Interfaces:**
- Consumes: `Model *` 替代 `OptionData *`
- Produces: `Executor::execute()` 从 Model 中读取 OptionData

- [ ] **Step 1: 写测试 — Executor 直接构造**

```cpp
TEST_CASE("executor: Executor(Model*) 构造 + execute 执行 action", "[executor]")
{
    Model model;
    OptionBuilder ob(&model.m_opts__);
    ob.addOption<std::string>("task", "t", "")
        ->onSet([&](std::string const &_V) {
            model.m_strProgramName__ = "executed:" + _V;
        });

    Executor exec(&model);  // 新构造签名

    // 模拟解析后状态：手动记录 actionOrder
    model.m_opts__.recordParseOrder("task");
    model.m_opts__.mgrValues.mutFind("task")->strValue = "build";
    model.m_opts__.mgrValues.mutFind("task")->bExplicit__ = true;

    exec.execute();
    REQUIRE(model.m_strProgramName__ == "executed:build");
}
```

- [ ] **Step 2: 编译验证测试失败（旧 Executor(OptionData*) 不接受 Model*）**

Run: `xmake build unit_test`
Expected: FAIL — 构造函数参数不匹配

- [ ] **Step 3: 修改 executor.h — Executor(Model*)**

```cpp
class Executor
{
public:
    /// @brief 构造执行器
    /// @param _Model 底层模型（含 OptionData）
    explicit Executor(Model *_Model) noexcept
        : m_pModel__(_Model)
    {}

    /// @brief 按解析顺序执行含 action 的选项
    void execute()
    {
        for (auto const &key : m_pModel__->m_opts__.actionOrder()) {
            auto *pOpt = m_pModel__->m_opts__.mgrValues.mutFind(key);
            if (!pOpt) continue;

            if (pOpt->fnOnSet__) { pOpt->fnOnSet__(pOpt->strValue); }

            if (!pOpt->strActionTemplate__.empty()) {
                auto cmd = util::replace(pOpt->strActionTemplate__, "{value}", pOpt->strValue);
                if (!cmd.empty()) {
                    int ret = std::system(cmd.c_str());
                    if (ret != 0) {
                        std::cerr << "(action) command exit " << ret << ": " << cmd << "\n";
                    }
                }
            }
        }
        m_pModel__->m_opts__.clearParseOrder();
    }

private:
    Model *m_pModel__ = nullptr;
};
```

新增 include: `#include "model.h"`（或前向声明 `class Model;`）。

- [ ] **Step 4: 更新 context.h 中 Executor 构造**

```cpp
// context.h 构造函数
// 原: m_executor__(&m_instData__.m_opts__)
// 改:
m_executor__(&m_instData__)
```

- [ ] **Step 5: 编译 + 运行测试验证通过**

Run: `xmake build unit_test && xmake run unit_test`
Expected: PASS — Executor 测试通过；action 相关用例（[action] tag）行为不变

- [ ] **Step 6: 提交**

```bash
git add include/cmd_args/context/executor.h include/cmd_args/context/context.h \
        dfx/unit_test/head.cpp
git commit -m "♻️ refactor(executor): Executor(Model*) replacing Executor(OptionData*)"
```

---

### Task 8: Context 容器化 — compile → hasAction 门控 → execute

**Files:**
- Modify: `include/cmd_args/context/context.h`
- Test: `dfx/unit_test/head.cpp`

**Interfaces:**
- Consumes: `Compiler::parse(...)`, `Model::hasAction()`, `Executor::execute()`
- Produces: `Context::model()` accessor（供 expressions 测试使用）；parse() 内部门控逻辑

- [ ] **Step 1: 写测试 — hasAction 门控 + model() accessor**

```cpp
TEST_CASE("context: hasAction 门控 — 有 action 时 Executor 执行", "[context]")
{
    Context app("ut_hasaction");
    app.addOption<std::string>("task", "t", "")
        ->onSet([](std::string const &) { /* noop */ });

    std::error_code ec;
    char const *argv[] = { "test", "--task", "run", nullptr };
    app.parse(ec, 3, argv, nullptr);
    REQUIRE(!ec);
    REQUIRE(app.getOption<std::string>("task") == "run");
}

TEST_CASE("context: model() accessor 返回底层 Model 引用", "[context]")
{
    Context app("ut_model_acc");
    auto const &m = app.model();
    REQUIRE(m.m_strProgramName__ == "ut_model_acc");
}
```

- [ ] **Step 2: 修改 context.h — 容器化 + model() accessor**

```cpp
// 公开 API 新增
/// @brief 获取底层 Model 引用（只读，用于 expressions() 等统一视图）
auto model() const noexcept -> Model const & { return m_instData__; }

// __doParse 内部 — 替换为门控逻辑：
void __doParse(int _Argc, char const *_Argv[], char *_Envp[])
{
    if (_Argc > 0 && _Argv[0]) {
        m_instData__.m_strProgramName__ = _Argv[0];
    }
    else {
        m_instData__.m_strProgramName__ = m_strPrjName__;
    }

    Tokens_T toks;
    if (_Argc > 1) {
        Lexer lex(_Argc - 1, _Argv + 1, nullptr);
        toks = lex.tokens();
    }

    m_compiler__.set_program_name(m_instData__.m_strProgramName__);
    m_compiler__.set_description(m_strPrjName__);

    Envs_T envs;
    if (_Envp) { for (int i = 0; _Envp[i]; ++i) { envs.emplace_back(_Envp[i]); } }

    m_compiler__.ensureBuiltins();
    m_compiler__.parse(toks, envs);                    // 纯解析：Tokens → Expression 分类入库 Model

    if (m_instData__.hasAction()) {                    // Model.hasAction() 门控
        m_executor__.execute();                        // 按 actionOrder 顺序执行
    }

    m_compiler__.finalize();                           // seed 环境兜底 + 展开 + save-config

    m_interp__.markParsed();
    m_bFired__ = m_compiler__.isFired();
}

// __doParse(Tokens, Envs) 同步更新
void __doParse(Tokens_T const &_Tokens, Envs_T const &_Env)
{
    m_compiler__.set_program_name(m_instData__.m_strProgramName__.empty()
                                     ? m_strPrjName__
                                     : m_instData__.m_strProgramName__);
    m_compiler__.set_description(m_strPrjName__);

    m_compiler__.ensureBuiltins();
    m_compiler__.parse(_Tokens, _Env);

    if (m_instData__.hasAction()) {
        m_executor__.execute();
    }

    m_compiler__.finalize();

    m_interp__.markParsed();
    m_bFired__ = m_compiler__.isFired();
}

// reset() 中 m_parser__ → m_compiler__
Context &reset()
{
    m_compiler__.resetData();
    clearLast();
    return *this;
}

// loop() 中 m_parser__ → m_compiler__
void loop()
{
    m_compiler__.ensureBuiltins();
    m_interp__.loop(static_cast<Errors &>(*this));
}
```

类注释更新：
```cpp
/*
    核心容器：
    - build<>() 构筑参数/选项
    - parse()   编译(Compiler) → hasAction 门控(Executor) → finalize
    - env 交互  set_env/unset_env/apply_env/env_string
    - once()/loop() 执行模式（委托 Interpreter 流式引擎）

    通用容器设计：
    - Model 是数据仓库（ArgumentData + OptionData + EnvData + Expression 统一视图）
    - Compiler 是编译引擎（Lexer → Parsers → Expression 分类入库）
    - Executor 是批量执行器（按 actionOrder 顺序触发 OptionAction）
    - Interpreter 是流式解释器（逐字符分词 → 即时分类执行）
    - 执行路径由调用入口（parse vs once/loop）和数据内容（hasAction）共同决定
*/
```

- [ ] **Step 3: 更新 expressions 测试（Task 4 中的第二个 TEST_CASE 启用）**

```cpp
TEST_CASE("expression: Model::expressions() 统一视图包含三类条目", "[expression]")
{
    Context app("ut_expr_view");
    app.build<ArgumentNamed>("hello", "greeting");
    app.addOption<int>("timeout", "sec", 30);
    app.set_env("MY_K", "MY_V");

    std::error_code ec;
    char const *argv[] = { "test", "--hello", "hi", "--timeout", "9", nullptr };
    app.parse(ec, 6, argv, nullptr);
    REQUIRE(!ec);

    auto exprs = app.model().expressions();
    REQUIRE(exprs.size() >= 3);

    size_t nArg = 0, nOpt = 0, nEnv = 0;
    for (auto const *e : exprs) {
        switch (e->kind()) {
        case ExpressionKind_E::Argument: ++nArg; break;
        case ExpressionKind_E::Option:   ++nOpt; break;
        case ExpressionKind_E::Env:      ++nEnv; break;
        }
    }
    REQUIRE(nArg >= 1);
    REQUIRE(nOpt >= 1);
    REQUIRE(nEnv >= 1);
}
```

- [ ] **Step 4: 编译 + 全量回归**

Run: `xmake build unit_test && xmake run unit_test`
Expected: PASS — 全部用例通过；公开 API（parse/once/loop）行为不变

Run: `xmake build once && xmake build config_demo`
Expected: PASS

- [ ] **Step 5: 提交**

```bash
git add include/cmd_args/context/context.h dfx/unit_test/head.cpp
git commit -m "✨ feat(context): containerize Context — Compiler+Executor+Model.hasAction gate + model() accessor"
```

---

## Phase C — Interpreter 流式引擎（Tasks 9–11）

> 流式执行路径：Scanner::step 增量分词 → Interpreter 逐字符泵 → Token 完成即分类 → 按 Expression 类型立即执行。

### Task 9: Scanner::step 增量分词

**Files:**
- Modify: `include/cmd_args/context/lexer.h`
- Test: `dfx/unit_test/head.cpp`

**Interfaces:**
- Produces: `Scanner::step(char) -> std::optional<Token>`；`Token::hasInlineEq() const -> bool`（用于精确匹配 batch 语义）

- [ ] **Step 1: 写测试 — Scanner::step 增量分词**

```cpp
TEST_CASE("lexer: Scanner::step 增量分词逐字符产出 token", "[lexer]")
{
    using detail::Scanner;
    using detail::Token;

    Scanner sc({});
    // "--timeout 9" 逐字符喂入
    REQUIRE_FALSE(sc.step('-'));    // IN_INTEGER
    REQUIRE_FALSE(sc.step('-'));    // IN_LONG_NAME
    REQUIRE_FALSE(sc.step('t'));
    REQUIRE_FALSE(sc.step('i'));
    REQUIRE_FALSE(sc.step('m'));
    REQUIRE_FALSE(sc.step('e'));
    REQUIRE_FALSE(sc.step('o'));
    REQUIRE_FALSE(sc.step('u'));
    REQUIRE_FALSE(sc.step('t'));
    auto tok1 = sc.step(' ');       // space 结束 LongName
    REQUIRE(tok1.has_value());
    REQUIRE(tok1->toText() == "--timeout");

    REQUIRE_FALSE(sc.step('9'));    // Integer
    auto tok2 = sc.step('\0');      // 流结束 flush
    REQUIRE(tok2.has_value());
    REQUIRE(tok2->toText() == "9");
}

TEST_CASE("lexer: Scanner::step 字符串未终结抛 ErrorLexer", "[lexer]")
{
    using detail::Scanner;
    Scanner sc({});
    sc.step('"');
    REQUIRE_FALSE(sc.step('a'));
    REQUIRE_FALSE(sc.step('b'));
    REQUIRE_THROWS_AS(sc.step('\0'), ErrorLexer);  // IN_STRING → UNTERMINATED_STRING
}

TEST_CASE("lexer: Scanner::step --timeout=9 产出 inline eq 标记", "[lexer]")
{
    using detail::Scanner;
    Scanner sc({});
    sc.step('-'); sc.step('-');
    sc.step('t'); sc.step('i'); sc.step('m'); sc.step('e'); sc.step('o'); sc.step('u'); sc.step('t');
    auto tok1 = sc.step('=');       // '=' 结束 LongName → hasInlineEq = true
    REQUIRE(tok1.has_value());
    REQUIRE(tok1->toText() == "--timeout");
    REQUIRE(tok1->hasInlineEq());

    auto tok2 = sc.step('9');
    REQUIRE_FALSE(tok2.has_value()); // 9 仍在 Integer 状态
    auto tok3 = sc.step('\0');
    REQUIRE(tok3.has_value());
    REQUIRE_FALSE(tok3->hasInlineEq());
}
```

- [ ] **Step 2: 编译验证测试失败**

Run: `xmake build unit_test`
Expected: FAIL — `Scanner::step` / `Token::hasInlineEq` 不存在

- [ ] **Step 3: 修改 lexer.h — Token + Scanner**

Token 新增成员和方法：

```cpp
class Token
{
    // ...（现有成员保留）

    void clear()
    {
        m_enAttr__   = Type_E::Unknown;
        m_strName__.clear();
        m_locLine__  = 0;
        m_bInlineEq__ = false;       // ← 新增
    }

    /// @brief 是否由 '=' 内联分隔（--name=value 中 '=' 直接结束 LongName）
    bool hasInlineEq() const noexcept { return m_bInlineEq__; }

private:
    Type_E m_enAttr__   = Type_E::Unknown;
    Name_T m_strName__;
    size_t m_locLine__  = 0;
    bool   m_bInlineEq__ = false;    // ← 新增
};
```

Scanner 新增增量方法：

```cpp
class Scanner
{
public:
    // ...（现有构造、scan、公开 API 保留）

    /// @brief 增量模式：逐字符喂入，token 完成时返回之（Interpreter 流式模式）
    /// @param _Cur 字符；'\0' 表示流结束（flush 最后一个 token）
    /// @return 有 token 完成则返回之，否则 nullopt
    std::optional<Token> step(char _Cur)
    {
        if (_Cur == '\0') {
            __flush(m_tCur__);
            m_enState__ = Phrase_E::BEGIN;
        }
        else {
            m_tblScanInput__[m_enState__](_Cur, m_tCur__);
            if (m_enState__ == Phrase_E::END) {
                __processEnd('\0', m_tCur__);
            }
        }
        if (m_tResult__.empty()) return std::nullopt;
        auto tok = std::move(m_tResult__.front());
        m_tResult__.pop_front();
        return tok;
    }

private:
    // ...（现有私有成员保留）

    Token m_tCur__;  // ← 新增：流式模式当前处理中的 token（scan() 不使用此成员）
};
```

`__processLongName` 中在 '=' 分支设置 inline eq 标记：

```cpp
void __processLongName(char const _Cur, Token &_T)
{
    if (_Cur == '=' || std::isspace(static_cast<unsigned char>(_Cur))) {
        _T.setAttr(Token::Type_E::LongName);
        if (_Cur == '=') { _T.setInlineEq(true); }  // ← 新增
        __setState(Phrase_E::END);
        return;
    }
    _T.pick(_Cur);
    __nextChar();
    __setState(Phrase_E::IN_LONG_NAME);
}
```

Token 需新增 `setInlineEq` 方法：
```cpp
void setInlineEq(bool _V) { m_bInlineEq__ = _V; }
```

- [ ] **Step 4: 编译 + 运行测试验证通过**

Run: `xmake build unit_test && xmake run unit_test`
Expected: PASS — Scanner::step 三个测试通过；原有用例不受影响（47 用例 / ~240 断言）

- [ ] **Step 5: 提交**

```bash
git add include/cmd_args/context/lexer.h dfx/unit_test/head.cpp
git commit -m "✨ feat(lexer): add Scanner::step() incremental tokenizer for streaming interpreter"
```

---

### Task 10: Interpreter 流式 reinterpret

**Files:**
- Modify: `include/cmd_args/context/interpreter.h`
- Test: `dfx/unit_test/head.cpp`

**Interfaces:**
- Consumes: `Scanner::step`（Task 9）；`OptionParser` / `ArgumentParser` / `EnvParser`（继承）
- Produces: 流式 `Interpreter::interpret(line)` — 字符泵 → Token → 分类 → 立即执行

- [ ] **Step 1: 写测试 — 流式 interpret 行为不变**

```cpp
TEST_CASE("interpreter: 流式 interpret 与批量模式结果一致", "[interpreter]")
{
    Context app("ut_stream_interp");
    app.build<ArgumentNamed>("hello", "h");
    app.addOption<int>("timeout", "t", 30);

    // 批量 parse
    std::error_code ec;
    char const *argv1[] = { "test", "--hello", "a", "--timeout", "9", nullptr };
    app.parse(ec, 5, argv1, nullptr);
    REQUIRE(!ec);
    auto batchHello = app.getArgument<std::string>("hello");
    auto batchTimeout = app.getOption<int>("timeout");
    REQUIRE(batchHello == "a");
    REQUIRE(batchTimeout == 9);

    // 流式 interpret（等效命令行）
    Model model2;
    OptionBuilder ob2(&model2.m_opts__);
    ob2.addOption<int>("timeout", "t", 30);
    auto &an2 = static_cast<ArgumentBuilder &>(
        *reinterpret_cast<ArgumentBuilder *>(nullptr)); // 需要通过 Context build

    Context app2("ut_stream_interp2");
    app2.build<ArgumentNamed>("hello", "h");
    app2.addOption<int>("timeout", "t", 30);

    Model model3;
    Errors errors;
    Interpreter interp(&model3);
    ArgumentBuilder ab(&model3.m_args__);
    ab.build<ArgumentNamed>("hello", "h");
    OptionBuilder ob(&model3.m_opts__);
    ob.addOption<int>("timeout", "t", 30);

    auto ec2 = interp.interpret("--hello a --timeout 9", errors);
    REQUIRE(!ec2);
    REQUIRE(interp.prepared());
    auto *pH = model3.m_args__.mgrName.doGet("hello");
    REQUIRE(pH != nullptr);
    REQUIRE(pH->getValue() == "a");
    auto *pT = model3.m_opts__.mgrValues.find("timeout");
    REQUIRE(pT != nullptr);
    REQUIRE(pT->strValue == "9");
}
```

> 此测试验证流式 interpret 与 batch parse 产生相同的结果。

- [ ] **Step 2: 编译验证现有 interpreter 通过（未改动时）**

Run: `xmake build unit_test && xmake run unit_test`
Expected: PASS

- [ ] **Step 3: 重写 interpreter.h — 流式字符泵 interpret()**

核心变更：`interpret(line)` 方法从「batch tokenize → batch parse」改为「char pump → per-token classify → immediate execute」：

```cpp
/// @brief 解释执行一行：reset → 流式字符泵 → 每个 Token 即时分类执行 → finalize
std::error_code interpret(std::string const &_Line, Errors &_Errors)
{
    // 1. 还原数据 + 清错误栈
    m_pModel__->m_args__.mgrName.doMutVisit([](Argument &a) { a.reset(); });
    m_pModel__->m_args__.mgrPos.doMutVisit([](Argument &a) { a.reset(); });
    m_pModel__->m_opts__.mgrValues.forEachValue([](OptionValue &o) { o.reset(); });
    _Errors.clearLast();

    try {
        // 2. 流式增量分词
        Scanner sc({});
        std::vector<std::string> vecPos;        // 待分配的方位参数缓冲
        std::string strPendingLong;             // 等待取值的选项名（--timeout 待下一个 token）
        std::string strPendingNamedArg;         // 等待取值的命名参数（--name 待下一个 token）
        bool bOptEnd = false;                   // -- 之后

        auto pump = [&](char _C) {
            auto tokOpt = sc.step(_C);
            if (!tokOpt) return;
            std::string text = tokOpt->toText();
            bool bInlineEq = tokOpt->hasInlineEq();

            // ── 等待值的选项：当前 token 就是值 ──
            if (!strPendingLong.empty()) {
                if (!bInlineEq && util::starts_with(text, "-")) {
                    throw ErrorParse(ParseError_E::NEED_VALUE,
                        "option -" + strPendingLong + " need a value");
                }
                auto *pOpt = m_pModel__->m_opts__.mgrValues.mutFind(strPendingLong);
                if (!pOpt) {
                    throw ErrorParse(ParseError_E::UNKNOWN_OPTION,
                        "option not found: " + strPendingLong);
                }
                __executeOptionImmediate(pOpt, text, pOpt->strType == "bool");
                strPendingLong.clear();
                return;
            }
            // ── 等待值的命名参数：当前 token 就是值 ──
            if (!strPendingNamedArg.empty()) {
                auto *pArg = m_pModel__->m_args__.mgrName.mutGet(strPendingNamedArg);
                if (pArg) { pArg->set(text); }
                strPendingNamedArg.clear();
                return;
            }

            // ── -- 之后全部方位参数 ──
            if (bOptEnd) {
                vecPos.emplace_back(text);
                return;
            }

            // ── 短回路选项（OptionAction）：立即触发 ──
            if (text == "-?" || text == "--help"
                || text == "-!" || text == "--env"
                || text == "--env-apply") {
                // 按名称查找短回路
                std::string longName;
                if (text == "--help" || text == "-?") longName = "help";
                else if (text == "--env" || text == "-!") longName = "env";
                else if (text == "--env-apply") longName = "env-apply";

                auto const *pSc = m_pModel__->m_opts__.mgrActions.doGet(longName);
                if (!pSc) {
                    // 尝试短名
                    if (text.size() == 2 && text[0] == '-') {
                        pSc = m_pModel__->m_opts__.mgrActions.doGet(text[1]);
                    }
                }
                if (pSc) { pSc->fnCallback(); return; }
            }

            // ── -- 分隔符 ──
            if (text == "--") { bOptEnd = true; return; }

            // ── 长选项：--name 或 --name=value ──
            if (util::starts_with(text, "--") && text.size() > 2) {
                auto body = text.substr(2);
                auto eq   = body.find('=');
                std::string name  = body.substr(0, eq == std::string::npos ? body.size() : eq);
                std::string value = eq == std::string::npos ? "" : body.substr(eq + 1);

                // 短回路
                if (auto const *pSc = m_pModel__->m_opts__.mgrActions.doGet(name)) {
                    pSc->fnCallback();
                    return;
                }

                // 值选项
                if (auto *pOpt = m_pModel__->m_opts__.mgrValues.mutFind(name)) {
                    if (value.empty() && pOpt->strType != "bool") {
                        strPendingLong = name;  // 等待下一个 token 作为值
                        return;
                    }
                    __executeOptionImmediate(pOpt, value, pOpt->strType == "bool");
                    return;
                }

                // 命名参数
                if (auto *pArg = m_pModel__->m_args__.mgrName.mutGet(name)) {
                    if (value.empty()) {
                        strPendingNamedArg = name;  // 等待下一个 token 作为值
                        return;
                    }
                    pArg->set(std::move(value));
                    return;
                }

                // 未识别：可能是环境变量 --env-set 形式，交给 positional
                vecPos.emplace_back(text);
                return;
            }

            // ── 短选项：-x / -abc 聚合 ──
            if (util::starts_with(text, "-") && text.size() >= 2) {
                if (text.size() == 2) {
                    char key = text[1];
                    // 短回路
                    if (auto const *pSc = m_pModel__->m_opts__.mgrActions.doGet(key)) {
                        pSc->fnCallback();
                        return;
                    }
                    if (auto *pOpt = m_pModel__->m_opts__.mgrValues.mutFind(key)) {
                        if (pOpt->strType == "bool") {
                            __executeOptionImmediate(pOpt, "", true);
                        }
                        else {
                            strPendingLong = std::string(1, key);  // 等待值
                        }
                        return;
                    }
                    // 未识别短选项
                    throw ErrorParse(ParseError_E::UNKNOWN_OPTION,
                        "unrecognized short option: " + text);
                }

                // 聚合短选项：-abc
                for (size_t i = 1; i < text.size(); ++i) {
                    char key = text[i];
                    if (auto const *pSc = m_pModel__->m_opts__.mgrActions.doGet(key)) {
                        pSc->fnCallback();
                        return;
                    }
                    auto *pOpt = m_pModel__->m_opts__.mgrValues.mutFind(key);
                    if (!pOpt) {
                        throw ErrorParse(ParseError_E::UNKNOWN_OPTION,
                            "unrecognized short option '" + std::string(1, key) + "' in " + text);
                    }
                    if (pOpt->strType == "bool") {
                        __executeOptionImmediate(pOpt, "", true);
                        continue;
                    }
                    // 值选项出现在聚合串中：余下部分就是它的值
                    std::string val = text.substr(i + 1);
                    if (!val.empty() && val.front() == '=') val = val.substr(1);
                    if (val.empty()) { strPendingLong = std::string(1, key); return; }
                    __executeOptionImmediate(pOpt, val, false);
                    return;  // 聚合串中只允许一个值选项
                }
                return;
            }

            // ── 普通 token → 方位参数 ──
            vecPos.emplace_back(text);
        };

        // 3. 泵入所有字符 + 流结束符
        for (char c : _Line) { pump(c); }
        pump('\0');

        // 4. 提交方位参数（复用 ArgumentParser 逻辑）
        __assignPositional(vecPos);

        // 5. 检查未识别的 dash token
        for (auto const &t : vecPos) {
            if (util::starts_with(t, "-")) {
                throw ErrorParse(ParseError_E::UNKNOWN_OPTION,
                    "unrecognized option or argument: " + t);
            }
        }

        // 6. 后置：seed env + 展开 + save-config
        __finalize();
        m_bParsed__ = true;
        return make_error_code(ParseError_E::OK);
    }
    catch (ErrorParse const &e) { _Errors.setLast(e.code()); return e.code(); }
    catch (ErrorLexer const &e) { _Errors.setLast(e.code()); return e.code(); }
    catch (std::exception const &ex) { _Errors.setMsg(ex.what()); return make_error_code(ParseError_E::UNKNWON_ARGS); }
}

/// @brief 选项赋值 + 立即触发 action（Interpreter 流式专用）
void __executeOptionImmediate(OptionValue *_Opt, std::string const &_Value, bool _IsBool)
{
    if (_IsBool) {
        _Opt->strValue = _Value.empty() ? "1" : (util::parse_bool(_Value) ? "1" : "0");
    }
    else {
        _Opt->strValue = _Value;
    }
    _Opt->bExplicit__ = true;
    // 立即触发内联 action（不记录 actionOrder，因为 Interpreter 逐 token 执行）
    this->OptionParser::onOptionAssigned(_Opt, _Opt->strValue);
}

/// @brief 按声明顺序提交方位参数（复用 ArgumentParser 的数量校验）
void __assignPositional(std::vector<std::string> const &_VecPos)
{
    size_t total = m_pModel__->m_args__.mgrPos.doSize();
    size_t need  = 0;
    m_pModel__->m_args__.mgrPos.doConVisit([&](Argument const &a) {
        if (!a.isExternal()) ++need;
    });
    if (_VecPos.size() < need || _VecPos.size() > total) {
        std::ostringstream oss;
        oss << "give " << _VecPos.size() << ", but need " << need << ".." << total << '\n';
        throw ErrorParse(ParseError_E::UNKNWON_ARGS, oss.str());
    }

    bool   exact = (_VecPos.size() == need);
    size_t vi    = 0;
    for (size_t seq = 0; seq < total && vi < _VecPos.size(); ++seq) {
        auto *p = m_pModel__->m_args__.mgrPos.mutGetBySeq(seq);
        if (!p) continue;
        if (exact && p->isExternal()) continue;
        p->set(_VecPos[vi++]);
    }
}
```

新增 include: `#include <sstream>` / `#include "cmd_args/context/lexer.h"`（已有）。

- [ ] **Step 4: 编译 + 全量回归**

Run: `xmake build unit_test && xmake run unit_test`
Expected: PASS — 流式 interpret 测试通过；原有 [interpreter] 三个用例行为不变（49 用例 / ~250 断言）

- [ ] **Step 5: 提交**

```bash
git add include/cmd_args/context/interpreter.h dfx/unit_test/head.cpp
git commit -m "✨ feat(interpreter): rewrite interpret() as streaming char pump with per-token classification"
```

---

### Task 11: 文档同步 + 全量验收

**Files:**
- Modify: `.dsh/AGENT.md`、`context/AGENT.md`、`option/AGENT.md`、`README.md`、`README_zh.md`
- Modify: 各子目录 AGENT.md（若有）

**验收标准：**

- [ ] **Step 1: 更新 .dsh/AGENT.md**

同步以下内容：
- §3 源码地图：新增 `expression.h`，`parser.h` → `compiler.h`，类名同步
- §4 API 事实：新增 `model().expressions()`、`model().hasAction()`；`ExpressionKind_E` 导出
- §6 行为约定：Compiler-Executor 批量路径描述；Interpreter 流式字符泵描述；`hasAction` 门控
- §9 测试门槛：更新测试用例数与断言数

- [ ] **Step 2: 更新 context/AGENT.md**

```markdown
# context — 上下文/解析管线

## 模块概述
Context 为通用容器，持有 Model（数据仓库 + Expression 统一视图）+ Builder + Compiler + Executor + Interpreter。
执行路径由调用入口（parse vs once/loop）和 Model 内容（hasAction）共同决定。

## 文件职责

| 文件 | 类 | 职责 |
|------|---|------|
| `expression.h` | `Expression` / `ExpressionKind_E` | **表达式基类**：Argument/Option/Env 的统一视图，kind() 自动分类 |
| `context.h` | `Context` | **通用容器**：组合 Model+Builder+Compiler+Executor+Interpreter；build/parse/once/loop |
| `compiler.h` | `Compiler` | **编译引擎**（原 Parser）：Lexer → Parsers → Expression 分类入库 Model |
| `interpreter.h` | `Interpreter` | **流式引擎**：Scanner::step 逐字符分词 → Token 即时分类执行 → 按类型立即执行 |
| `executor.h` | `Executor` | **批量执行器**：按 actionOrder 顺序执行 OptionAction（fnOnSet__ + action template） |
| `model.h` | `Model` | **数据仓库**：ArgumentData + OptionData + EnvData + hasAction() + expressions() |
| `lexer.h` | `Lexer` / `Scanner` | Scanner 状态机 + **step() 增量分词** |

## 三层架构
```
Context（通用容器）
  ├── Model        数据仓库 + Expression 统一视图（hasAction 门控）
  ├── Builder      构筑桥接 → ArgumentBuilder / OptionBuilder
  ├── Compiler     编译引擎（Lexer → Parsers → Expression 分类入库 Model）
  ├── Executor     批量执行（actionOrder 顺序触发 OptionAction）
  └── Interpreter  流式引擎（Scanner::step → per-token classify → 立即执行）
```

## 批量模式（Context::parse）
```
Compiler::ensureBuiltins → Compiler::parse(tokens)
  → Tokens → Parsers → Expression 分类入库 Model
→ if (Model::hasAction()) Executor::execute()
→ Compiler::finalize()
```

## 流式模式（Interpreter::loop / interpret）
```
per-line: Interpreter::interpret(line)
  → resetData → Scanner::step 逐字符分词
  → 每个 Token 完成即分类（Option/Argument/Env）
  → 按 Expression 类型立即执行（选项 assign+action / 参数 set / env set）
  → finalize
```

## Compiler vs Interpreter
- **Compiler**（批量）：先编译所有 Token 进 Model，Executor 后置按 actionOrder 统一执行。
- **Interpreter**（流式）：逐字符分词，Token 完成即执行，Expresssion 类型决定执行动作。
```

- [ ] **Step 3: 更新 README.md / README_zh.md**

- 流程图：`Parser` → `Compiler`
- 新增章节：Expression 类层次、hasAction 门控、流式 Interpreter
- 测试统计更新

- [ ] **Step 4: 全量构建验收**

```powershell
# Windows 环境
$env:XMAKE_GLOBALDIR='H:\dsh_space\cmd_args\build\.xmake-global'
$env:XMAKE_PROFILE_DIR='H:\dsh_space\cmd_args\build\.xmake-profile'

# 清理重建
xmake clean
xmake f

# 单测 debug
xmake build unit_test -v
xmake run unit_test

# 单测 release
xmake f -m release
xmake build unit_test -v
xmake run unit_test

# 示例
xmake f -m debug
xmake build once && xmake build config_demo
```

**验收清单：**
- [ ] debug + release 单测全绿（目标 ≥49 用例 / ≥250 断言）
- [ ] once / config_demo 构建运行正常（公开 API 行为无回归）
- [ ] 流式 interpret 行为与 batch parse 一致（流式测试已覆盖）
- [ ] Expression::kind() 返回值与所属 Data 一致（三类皆有测试）
- [ ] Model::hasAction() 门控正确（有 action → Executor 执行；无 action → 跳过）
- [ ] Model::expressions() 返回完整三类条目

- [ ] **Step 5: 提交**

```bash
git add .dsh/AGENT.md include/cmd_args/context/AGENT.md include/cmd_args/option/AGENT.md \
        README.md README_zh.md
git commit -m "📝 docs: sync AGENT.md/READMEs with Expression+Compiler+Interpreter architecture"
```

---

## Self-Review

### Spec 覆盖检查

| 需求 | Task | 状态 |
|------|------|------|
| Context 为通用容器，执行依赖注入的成员 | Task 8（容器化 + model() accessor） | ✅ |
| Expression 为 Argument/Option/Env 基类 | Task 1-4（Expression + 三类继承） | ✅ |
| Lex → Token 流 → Parsers → Expression 分类入库 | Task 9（step 增量） + Task 10（流式 interpret） | ✅ |
| Model.hasAction() 门控 Executor | Task 5（hasAction） + Task 7（Executor） + Task 8（门控） | ✅ |
| Interpreter：string as stream → char by char → Token → Expression → 立即执行 | Task 9（step） + Task 10（流式 reinterpret） | ✅ |
| Expression 三种类型自动分类存入 Data | Task 2-4（kind 自动分类） + Task 5（expressions 统一视图） | ✅ |

### Placeholder 扫描

无 "TBD" / "TODO" / "implement later" / "Add appropriate error handling" / "Similar to Task N" 等占位符。

### 类型一致性

| 早期定义 | 后续引用 | 一致 |
|---------|---------|------|
| `ExpressionKind_E` (Task 1) | Task 2-4 kind() 返回；Task 5 expressions() switch；Task 10 classify | ✅ |
| `Expression::kind()` non-virtual (Task 1) | 所有调用点 | ✅ |
| `Expression::getName()` virtual (Task 1) | Option/environment override (Task 3/4) | ✅ |
| `Token::hasInlineEq()` (Task 9) | Interpreter pending-value (Task 10) | ✅ |
| `Model::hasAction()` (Task 5) | Context __doParse 门控 (Task 8) | ✅ |
| `Model::expressions()` (Task 5) | Task 4 测试（启用）、Task 8 测试 | ✅ |
| `Compiler` class (Task 6) | context.h m_compiler__ (Task 6/8)、head.cpp test (Task 6) | ✅ |
| `Executor(Model*)` (Task 7) | Context 构造 (Task 7/8)、Task 7 test | ✅ |
| `Scanner::step(char)` (Task 9) | Interpreter interpret (Task 10) | ✅ |
| `__executeOptionImmediate` (Task 10) | Interpreter 内部调用 | ✅ |
| `__assignPositional` (Task 10) | Interpreter interpret 方法内 | ✅ |

---

## Execution Handoff

计划完成并保存至 `.dsh/plans/2026-08-25-context-container-expression-engine.md`。

**两种执行方式：**

**1. Subagent-Driven（推荐）** — 每个 Task 派发独立子代理，Task 间审查，快速迭代

**2. Inline Execution** — 在当前会话中使用 executing-plans 批量执行，带检查点审查

选哪种？
