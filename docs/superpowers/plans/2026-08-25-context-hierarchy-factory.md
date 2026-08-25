# Context 类层次重构（ContextBase / ContextStep / ContextRuntime + 工厂注入）实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use subagent-driven-development (recommended) or executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将单一大类 `Context` 重构为「抽象基类 `ContextBase`（纯虚 once/loop）+ 三个派生（兼容版 `Context`、批量引擎 `ContextStep`、流式引擎 `ContextRuntime`）」，并提供枚举工厂 `make_context(EngineKind_E, Name_T)` 按引擎种类注入派生上下文；同时全库清理成员命名的 `m_` 前缀滥用。

**Architecture:** 新建抽象基类承载 Model 与全部引擎无关 API；解析入口 parse 的公共包装（异常/错误码双通道）留在基类，引擎相关管线下沉为纯虚钩子 `_doRunPipeline(Tokens_T&, Envs_T const&)`。`ContextStep` 注入 Compiler+Executor（批量：先纯解析入库，hasAction 门控后按序执行）；`ContextRuntime` 注入 Interpreter（流式：token 流拼回一行即时解释执行，parse 本身也走流式）。旧 `Context` 改为继承基类的完整兼容实现，行为逐字节不变。Compiler 内置选项注册与 usage/help/env 描述函数提取为共享头。

**Tech Stack:** C++20（header-only）、xmake、Catch2（dfx/unit_test）

**Spec:** 用户需求原文——"Context是基类，有once,loop两个虚函数接口，分别派生ContextStep, 以及ContextRuntime, 通过Compiler+executor这边在工厂函数注入为ContextStep, 通过Interpreter在工厂函数注入为ContextRuntime, 工厂函数的入参是枚举E_STEP,E_RUN_TIME"。已确认的六项决策：
1. Context 保留旧名做完整实现（兼容期），另建抽象基类 `ContextBase`；
2. 枚举类型名 `EngineKind_E`，枚举值 `E_STEP` / `E_RUN_TIME`；
3. 工厂为自由函数 `make_context(EngineKind_E _Kind, Name_T _Name) -> std::unique_ptr<ContextBase>`；
4. `ContextRuntime::parse()` 也走 Interpreter 流式路径；
5. **引擎不共用**：ContextStep 内部只有 Compiler+Executor（零 Interpreter 引用）；ContextRuntime 内部只有 Interpreter（零 Compiler/Executor 引用）；兼容版 Context 是唯一同时持有三件套的过渡产物，冻结不动；
6. **成员命名铁律**：`m_` 前缀仅允许用于 `mutable` 成员（样板：exception_handler.h 的 `mutable std::string m_what`）；非 mutable 成员一律不带 `m_`，且本次**全库清理**（含测试文件中的引用）。
7. once 维持 exit 进程语义：两派生类的 `once()` 一致——未解析抛 `ErrorInterp(STOPPED)`，已解析则 `std::exit(exit_code)`。loop 与 once 的关系：**loop = once 的循环化**——每轮输入等价一次 once 执行（独立解析、绝不把多段输入合并成一次命令），外加「识别 exit/quit/exit() 等退出信号→正常返回」与「自动等待下一段输入」。对 Runtime 而言每轮=一行流式解释；对 Step 而言每轮=一整行走批量管线。

## Global Constraints

- 语言标准 C++20（`xmake.lua: set_languages("c++20")`）；MSVC 下 `/utf-8` 已全局强制。
- 警告即错误（`set_warnings("all", "error")`）：所有新代码必须零警告。
- 库为 header-only：所有新增代码一律 inline 放在 `include/cmd_args/` 下头文件内，不新建 .cpp。
- **成员命名**：私有成员＝小写类型前缀+`__` 后缀（如 `compiler__ / pModel__ / ec__`，bool 用 `b` 前缀、指针用 `p` 前缀、枚举用 `en` 前缀）；保护成员＝小写类型前缀+`_` 后缀（如 `strPrjName_ / instData_ / bParsed_`）；公开成员＝小写类型前缀无后缀（如 Model 的 `args / opts / envs / strProgramName`）；**`m_` 前缀仅限 mutable 成员**。
- 命名空间只用宏 `CMD_ARGS_NAMESPACE_BEGIN / CMD_ARGS_NAMESPACE_END`；内部实现入 `namespace xxx_detail {}` 并以 `using xxx_detail::X;` 导出。
- 其余风格遵循 argument.h 规范：类型别名 PascalCase+`_T`；形参 `_` 前缀+PascalCase；虚动词加 `do` 前缀；返回类型非 4 字符用 `auto ... -> T` 后置（`void`/`bool` 前置）；Doxygen 全中文注释；Apache 头格式照抄 interpreter.h 第 1-17 行（Copyright[2026][ValenciaFly] + Email 行）。
- **兼容期铁律**：现有 `example/once/main.cpp`、`example/config/main.cpp` 零修改必须继续编译并通过；既有 `test_*.cpp` 除 Task 1 的机械改名外零逻辑改动。
- 不引入任何第三方依赖；只允许标准库与既有工程头。
- 排除区：`dfx/internal/**` 为 vendored 第三方（FakeIt/catch2/tlp），一切任务不得触碰。
- 测试目标构建命令：`xmake build unit_test`（target 在 dfx/unit_test/xmake.lua，`set_default(false)`）；运行全量 `xmake run unit_test`；按标签过滤 `xmake run unit_test "[ctx_step]"`（引号必须带）。

---

## 文件结构总览

```
include/cmd_args/
├── cmd_args.hpp                      [改] 追加 include factory.h 等
├── context/
│   ├── describe.h                    [新] usage/help/env 描述自由函数（自 Compiler 私有静态提取）
│   ├── builtin_options.h             [新] registerBuiltinOptions(Model*) + resetModelData(Model*)
│   ├── context_base.h                [新] 抽象基类：Model + 共享 API + parse 包装 + 纯虚 once/loop/_doRunPipeline
│   ├── context.h                     [改] Context : public ContextBase，删上移代码，保留三引擎与 override
│   ├── context_step.h                [新] ContextStep：Compiler+Executor 批量引擎
│   ├── context_runtime.h             [新] ContextRuntime：Interpreter 流式引擎
│   ├── factory.h                     [新] make_context(EngineKind_E, Name_T)
│   ├── compiler.h                    [改] 内置注册/desc/reset 委托共享头
│   └── parse_fwd.h                   [改] 增加 enum class EngineKind_E : char { E_STEP, E_RUN_TIME }
dfx/unit_test/
├── test_context_step.cpp             [新]
├── test_context_runtime.cpp          [新]
└── test_context_factory.cpp          [新]
```

继承关系与引擎注入（**不变量：两派生类引擎互不混用**）：

```
Errors (私有基)
 └── ContextBase  (抽象: once()/loop() 纯虚; Model instData_; parse×4 包装 → _doRunPipeline 纯虚钩子)
      ├── Context         兼容完整实现【过渡产物，冻结】: Compiler+Executor+Interpreter 三件套
      ├── ContextStep     批量步进引擎: 仅 Compiler+Executor，无任何 Interpreter 引用
      └── ContextRuntime  流式运行时引擎: 仅 Interpreter，无任何 Compiler/Executor 引用

make_context(E_STEP)     -> unique_ptr<ContextBase> → new ContextStep
make_context(E_RUN_TIME) -> unique_ptr<ContextBase> → new ContextRuntime
```

once/loop 语义对照表：

| 能力 | Context (兼容) | ContextStep | ContextRuntime |
|---|---|---|---|
| once() | 未 parse 抛 STOPPED；否则 `std::exit(nExitCode_)` | 同左 | 同左（「一次性执行完命令行参数」即结束进程） |
| 每轮输入的执行单元 | ——（loop 整体委托 Interpreter REPL） | 一整行 token 流走「Lexer 分词 → Compiler 纯解析 → Executor 门控 → finalize」 | 一行字符流走「Scanner 分词 → Expression 分类 → 即时执行」 |
| loop() | ensureBuiltins + Interpreter 流式 REPL | 批量 REPL：每轮先 reset（独立解析）→ 批量管线 → 错误打印 → 自动等待下一行 | 流式 REPL：每轮 interpret 一行（内部自带 reset），识别 exit/quit/exit() 后正常返回 |
| 多段输入关系 | 每轮独立，不合并 | 每轮独立，不合并 | 每轮独立，不合并 |
| action 触发时机 | 解析完成后按 actionOrder 统一 | 同左 | 解释到该 token 时立即 |

---

### Task 1: 全库成员命名清理（去 m_ 前缀，仅 mutable 保留）

纯机械重命名，行为零变化；以全量回归验收。**本任务先行**，后续所有任务的代码块均基于清理后的命名。

**Files:**
- Modify: `include/cmd_args/error/errors.h`、`include/cmd_args/error/parser_error.h`、`include/cmd_args/error/lexer_error.h`、`include/cmd_args/error/interpreter_error.h`、`include/cmd_args/error/builder_error.h`、`include/cmd_args/error/functional_util.h`、`include/cmd_args/error/exception_handler.h`
- Modify: `include/cmd_args/base/defer.hpp`
- Modify: `include/cmd_args/config/yaml.h`、`include/cmd_args/config/config_store.h`
- Modify: `include/cmd_args/option/option_data.h`
- Modify: `include/cmd_args/context/model.h`、`lexer.h`、`builder.h`、`interpreter.h`、`expression.h`、`executor.h`、`compiler.h`、`context.h`
- Modify: `dfx/unit_test/test_model.cpp`、`test_interpreter.cpp`、`test_expression.cpp`、`test_context.cpp`

**Interfaces:**
- Consumes: 无
- Produces: 全库统一的新成员名（后续任务的代码直接使用）：`Model::{args, opts, envs, strProgramName, bBuiltinOptions, bBuiltinsRegistered, strSaveConfigPath}`（public 无后缀）、`Errors::{ecDefault__, stkErros__}`、各错误类 `ec__`、`Expression::{enKind__, strName__, strHelp__}`、`Token::{enAttr__, strName__, locLine__, bInlineEq__}`、`Scanner::{source__, enState__, itPc__, cntLoc__, tResult__, tCur__, tblScanInput__}`、`Lexer::{srcInput__, srcEnvs__}`、`Interpreter::{pModel__, bParsed__, nExitCode__}`、`Compiler::{argParser__, optParser__, envBuilder__, envParser__, pModel__, bFired__}`、`Executor::pModel__`、`OptionData::actionOrder__`、`yaml doc::entries__`、`UniqueDefer/SharedDefer::{fnRun__, bCancel__}`、`Context::{strPrjName__, instData__, compiler__, executor__, interp__, bFired__}`。

- [ ] **Step 1: 按映射表逐文件替换**

映射总表（访问级别决定后缀；`m_` 一律剥除；已有 `__` 后缀的保留）：

| 文件 | 旧名 | 新名 | 说明 |
|---|---|---|---|
| model.h | `m_args__ / m_opts__ / m_envs__` | `args / opts / envs` | **public 成员无后缀** |
| model.h | `m_strProgramName__ / m_bBuiltinOptions__ / m_bBuiltinsRegistered__ / m_strSaveConfigPath__` | `strProgramName / bBuiltinOptions / bBuiltinsRegistered / strSaveConfigPath` | 同上 |
| builder.h / config_store.h / interpreter.h / compiler.h / context.h / describe 相关调用点 | `_Data->m_args__` 等对 Model 公有成员的全部引用 | 跟随上表（如 `_Data->args`） | 编译器会兜底揪出漏改 |
| errors.h | `m_ecDefault__ / m_stkErros__` | `ecDefault__ / stkErros__` | private |
| parser_error.h / lexer_error.h / interpreter_error.h / builder_error.h / errors.h(ErrorGeneral) | `m_ec__` | `ec__` | private |
| expression.h | `m_enKind__ / m_strName__ / m_strHelp__` | `enKind__ / strName__ / strHelp__` | private |
| lexer.h | Token 四成员、Scanner 七成员、Lexer 两成员（见 Interfaces） | 剥 `m_` 保 `__` | private |
| interpreter.h | `m_pModel__ / m_bParsed__ / m_nExitCode__` | `pModel__ / bParsed__ / nExitCode__` | private |
| compiler.h | `m_argParser__ / m_optParser__ / m_envBuilder__ / m_envParser__ / m_pModel__ / m_bFired__` | 去 `m_` 保 `__` | private |
| executor.h | `m_pModel__` | `pModel__` | private |
| option_data.h | `m_actionOrder__` | `actionOrder__` | private |
| yaml.h | `m_entries__` | `entries__` | private |
| defer.hpp | `m_fnRun__ / m_bCancel__` | `fnRun__ / bCancel__` | private |
| functional_util.h | `m_bHasInit__ / m_Data__ / m_func / m_value` | `bHasInit__ / data__ / func__ / value__` | 注意源码存在 `m_Data____`（多下划线）变体，用正则 `\bm_Data__+\b` 类匹配整体归一为 `data__`；`m_func/m_value` 补齐 `__` 后缀 |
| exception_handler.h | `m_strMessage__` | `strMessage__` | private |
| exception_handler.h | `mutable std::string m_what` 及其两处赋值 | **保持不动** | ✔ mutable 成员合法保留 `m_` 前缀（规则样板） |
| context.h | `m_strPrjName__ / m_instData__ / m_compiler__ / m_executor__ / m_interp__ / m_bFired__` | `strPrjName__ / instData__ / compiler__ / executor__ / interp__ / bFired__` | private（Task 5 迁入基类时再改保护后缀 `_`） |
| test_model.cpp / test_interpreter.cpp / test_expression.cpp / test_context.cpp | `model.m_opts__ / model.m_args__ / model.m_envs__ / model.m_strProgramName__ / model2.*` 等 35 处 | `model.opts / model.args / model.envs / model.strProgramName / ...` | 跟随 Model 公有成员新名 |

排除区（不得触碰）：`dfx/internal/**`。`program_name / description`（compiler.h 私有成员，本就无 m_）不在本任务范围。

- [ ] **Step 2: 零残留验证**

Run: `Get-ChildItem include, dfx\unit_test, example -Recurse -Include *.h,*.hpp,*.cpp | Select-String -Pattern '\bm_[A-Za-z]' | Select-Object Path, LineNumber, Line`
Expected: 仅命中 `include\cmd_args\error\exception_handler.h` 中 `mutable std::string m_what` 相关的 3 行（声明 + 两处赋值）。出现其他任何命中即为漏改。

- [ ] **Step 3: 全量构建**

Run: `xmake`
Expected: BUILD PASS（含两个 example——它们不经成员名访问，天然不受影响，作编译级冒烟）。

- [ ] **Step 4: 全量测试回归**

Run: `xmake run unit_test`
Expected: 全部 TEST_CASE 通过，零失败（行为不变的证明）。

- [ ] **Step 5: Commit**

```bash
git add -A include/cmd_args dfx/unit_test
git commit -m "refactor(style): drop m_ prefix from all non-mutable members library-wide"
```

---

### Task 2: 提取 describe.h（usage/help/env 描述函数）

**Files:**
- Create: `include/cmd_args/context/describe.h`
- Modify: `include/cmd_args/context/compiler.h:300-355`（私有静态 desc 函数区）

**Interfaces:**
- Consumes: `Model`（Task 1 清理后的公有成员 `args/opts/envs/strProgramName`），`EnvParser::envString`
- Produces: `cmd_args::describe_detail::usageOf(Model const&, Name_T const&) -> std::string`、`helpOf(Model const&) -> std::string`、`envOf(Model&) -> std::string`，并在 `CMD_ARGS_NAMESPACE_END` 前经 `using` 导出。Task 3 的内置选项回调与 Task 6 的 Runtime 复用这三个名字。

- [ ] **Step 1: 创建 describe.h**

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

#include <sstream>
#include <string>

#include "cmd_args/base/def.h"
#include "cmd_args/env/env_parser.h"

#include "model.h"

CMD_ARGS_NAMESPACE_BEGIN

namespace describe_detail
{

/// @brief usage 行：程序名 + 参数概览 + 选项概览
/// @param _M 底层模型
/// @param _Program 程序名（空串时回落 Model::strProgramName）
inline auto usageOf(Model const &_M, Name_T const &_Program) -> std::string
{
    std::ostringstream oss;
    oss << "usage: " << (_Program.empty() ? _M.strProgramName : _Program) << " [arguments]";
    oss << _M.args.mgrName.doGetUsage();
    oss << _M.args.mgrPos.doGetUsage();
    oss << _M.opts.usageString();
    oss << "\n";
    return oss.str();
}

/// @brief 完整帮助文本：usage + 命名参数帮助 + 方位参数帮助 + 选项帮助
inline auto helpOf(Model const &_M) -> std::string
{
    std::ostringstream oss;
    oss << usageOf(_M, _M.strProgramName);
    _M.args.mgrName.doGetHelp(oss, 0);
    _M.args.mgrPos.doGetHelp(oss, 0);
    if (!_M.opts.empty()) {
        oss << "\nOptions:\n";
        size_t maxLen = _M.opts.calcDescMaxLength();
        _M.opts.appendHelp(oss, maxLen);
    }
    oss << "\n";
    return oss.str();
}

/// @brief 环境变量快照文本（含回写标记）
inline auto envOf(Model &_M) -> std::string
{
    return EnvParser(&_M.envs).envString(_M.strProgramName);
}

}  // namespace describe_detail

using describe_detail::usageOf;  ///< 导出：usage 描述
using describe_detail::helpOf;   ///< 导出：完整帮助描述
using describe_detail::envOf;    ///< 导出：环境快照描述

CMD_ARGS_NAMESPACE_END
```

- [ ] **Step 2: Compiler 委托共享实现**

修改 `compiler.h`：
(a) include 区追加：

```cpp
#include "describe.h"
```

(b) 将 `__descUsageOf / __descHelpOf / __descEnvOf` 三个私有静态函数体替换为转发（保留原函数名与签名，减少 diff 面）：

```cpp
    static std::string __descUsageOf(Model const &_M, Name_T const &_Program)
    {
        return describe_detail::usageOf(_M, _Program);
    }

    static std::string __descHelpOf(Model const &_M)
    {
        return describe_detail::helpOf(_M);
    }

    static std::string __descEnvOf(Model &_M)
    {
        return describe_detail::envOf(_M);
    }
```

注意：`__descHelp()`（实例版，带 description 行与 environments 提示行）、`__descUsage()`、`__descEnv()` 保持原样不动——它们已调用上述三个静态函数。

- [ ] **Step 3: 全量构建 + 全量测试（行为不变回归）**

Run: `xmake && xmake run unit_test`
Expected: 构建成功（含 example 目标）；全部 TEST_CASE 通过。

- [ ] **Step 4: Commit**

```bash
git add include/cmd_args/context/describe.h include/cmd_args/context/compiler.h
git commit -m "refactor(context): extract usage/help/env describers into shared describe.h"
```

---

### Task 3: 提取 builtin_options.h（内置选项注册 + 数据还原）

**Files:**
- Create: `include/cmd_args/context/builtin_options.h`
- Modify: `include/cmd_args/context/compiler.h:143-204`（resetData / ensureBuiltins 函数体）
- Test: 无新测试——Task 5/6 的新测试将直接消费本头文件；本任务以既有全量回归为准

**Interfaces:**
- Consumes: `OptionBuilder`、`EnvBuilder`、`EnvParser`、`config::read_file/from_yaml/to_yaml/write_file`、`describe_detail::helpOf/envOf`（Task 2）
- Produces:
  - `void cmd_args::builtin_detail::registerBuiltinOptions(Model *_M)`（幂等：受 `bBuiltinOptions / bBuiltinsRegistered` 双标记门控）
  - `void cmd_args::builtin_detail::resetModelData(Model *_M)`
  - 二者经 `using` 导出至 `cmd_args`。Task 4 基类 `reset()` 与 Task 6 Runtime 的 `loop()/_doRunPipeline()` 直接调用。

- [ ] **Step 1: 创建 builtin_options.h**

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

#include <iostream>
#include <string>

#include "cmd_args/base/util.h"
#include "cmd_args/config/config_store.h"
#include "cmd_args/env/env_builder.h"
#include "cmd_args/env/env_parser.h"
#include "cmd_args/option/option_builder.h"

#include "describe.h"
#include "model.h"

CMD_ARGS_NAMESPACE_BEGIN

namespace builtin_detail
{

/// @brief 注册内置选项（--help/-?、--env/-!、--env-apply、--env-set、--env-unset、--load-config、--save-config）
/// @param _M 底层模型
/// @note 幂等：未开启内置选项或已注册过则直接返回（懒注册一次）
inline void registerBuiltinOptions(Model *_M)
{
    if (!_M->bBuiltinOptions || !_M->bBuiltinsRegistered == false) {}  // ← 不要这行！占位提醒：门控条件见下
}
}  // namespace builtin_detail
CMD_ARGS_NAMESPACE_END
```

实际完整实现（以上残缺块仅为防抄错警示，落盘以下面为准）：

```cpp
#pragma once

#include <iostream>
#include <string>

#include "cmd_args/base/util.h"
#include "cmd_args/config/config_store.h"
#include "cmd_args/env/env_builder.h"
#include "cmd_args/env/env_parser.h"
#include "cmd_args/option/option_builder.h"

#include "describe.h"
#include "model.h"

CMD_ARGS_NAMESPACE_BEGIN

namespace builtin_detail
{

inline void registerBuiltinOptions(Model *_M)
{
    if (!_M->bBuiltinOptions || _M->bBuiltinsRegistered) return;
    _M->bBuiltinsRegistered = true;

    Model        *m = _M;
    OptionBuilder ob(&_M->opts);
    ob.addShortCircuit('?', "help", "show this help message", [m] {
        std::cout << describe_detail::helpOf(*m);
    });
    ob.addShortCircuit('!', "env", "show environments of this program", [m] {
        std::cout << describe_detail::envOf(*m);
    });
    ob.addShortCircuit('\0', "env-apply", "write changed environments back to the OS", [m] {
        auto applied = EnvParser(&m->envs).apply();
        for (auto const &a : applied) { std::cout << "applied " << a << "\n"; }
        std::cout << describe_detail::envOf(*m);
    });
    ob.addOption<std::string>("env-set", "set an environment variable, KEY=VALUE", "")
        ->bBuiltin__ = true;
    _M->opts.mgrValues.mutFind("env-set")->onSet([m](std::string const &_V) {
        EnvBuilder b(&m->envs);
        auto eq = _V.find('=');
        if (eq == std::string::npos) {
            b.setEnv(_V, "");
        }
        else {
            b.setEnv(_V.substr(0, eq), _V.substr(eq + 1));
        }
    });
    ob.addOption<std::string>("env-unset", "unset an environment variable, KEY", "")
        ->bBuiltin__ = true;
    _M->opts.mgrValues.mutFind("env-unset")->onSet(
        [m](std::string const &_V) { EnvBuilder(&m->envs).unsetEnv(_V); });

    ob.addOption<std::string>(
          "load-config", "load a yaml config file before later args, PATH", "")
        ->bBuiltin__ = true;
    _M->opts.mgrValues.mutFind("load-config")->onSet(
        [m](std::string const &_Path) {
            auto text = config::read_file(_Path);
            if (!text) {
                std::cerr << "(load-config) cannot read file: " << _Path << "\n";
                return;
            }
            config::from_yaml(*m, *text);
        });
    ob.addOption<std::string>(
          "save-config", "write the resolved parameter/env state to a yaml file, PATH", "")
        ->bBuiltin__ = true;
    _M->opts.mgrValues.mutFind("save-config")->onSet(
        [m](std::string const &_Path) { m->strSaveConfigPath = _Path; });
}

/// @brief 还原 ArgumentData/OptionData 到内置默认值（Env 会话态保留）
/// @param _M 底层模型
inline void resetModelData(Model *_M)
{
    _M->args.mgrName.doMutVisit([](Argument &a) { a.reset(); });
    _M->args.mgrPos.doMutVisit([](Argument &a) { a.reset(); });
    _M->opts.mgrValues.forEachValue([](OptionValue &o) { o.reset(); });
}

}  // namespace builtin_detail

using builtin_detail::registerBuiltinOptions;  ///< 导出：内置选项懒注册
using builtin_detail::resetModelData;          ///< 导出：参数/选项数据还原

CMD_ARGS_NAMESPACE_END
```

注意两点：(1) 头部 Apache 许可注释照 Task 2 格式补全；(2) 文件内只允许一份实现——上面第一个残缺代码块绝不能同时存在于最终文件。

- [ ] **Step 2: Compiler 委托共享实现**

修改 `compiler.h`：
(a) include 区追加 `#include "builtin_options.h"`；
(b) `ensureBuiltins()` 函数体替换为一行委托：

```cpp
    /// 内置选项注册（--help/--env/--env-set 等，懒注册一次）
    void ensureBuiltins()
    {
        builtin_detail::registerBuiltinOptions(pModel__);
    }
```

(c) `resetData()` 函数体替换为委托：

```cpp
    /// 还原 ArgumentData/OptionData 到内置默认值（Env 会话态保留）
    void resetData()
    {
        builtin_detail::resetModelData(pModel__);
    }
```

(d) 因搬移而可能空置的标准库 include 保留不动（最小化改动）。

- [ ] **Step 3: 全量构建 + 全量测试**

Run: `xmake && xmake run unit_test`
Expected: 全部通过。重点观察 test_env.cpp（内置 --env-set/--env-unset/--env-apply）、test_config.cpp（--load-config/--save-config）、test_option.cpp——覆盖被搬移的全部回调路径。

- [ ] **Step 4: Commit**

```bash
git add include/cmd_args/context/builtin_options.h include/cmd_args/context/compiler.h
git commit -m "refactor(compiler): extract builtin option registration and model data reset into builtin_options.h"
```

---

### Task 4: ContextBase 抽象基类 + Context 改继承（兼容性核心任务）

**Files:**
- Create: `include/cmd_args/context/context_base.h`
- Modify: `include/cmd_args/context/context.h`（重写为薄派生）
- Test: 既有全部 test_*.cpp【零修改】作为回归验收

**Interfaces:**
- Consumes: Task 2/3 的 `describe.h`、`builtin_options.h`；`builder.h` 的 `BuildHelper{Model*}`；`Lexer(int, char const*[], nullptr).tokens()`；`Lexer::tokensFromLine(std::string const&)`
- Produces（后续所有任务依赖的确切签名）:

```cpp
class ContextBase : private Errors
{
public:
    explicit ContextBase(std::string _Name);
    virtual ~ContextBase() = default;

    // 引擎无关 API（自 Context 上移，签名不变；链式返回类型 Context & → ContextBase &）：
    // build<>×2 / build() / addOption<T>×2 / addFlag×2 /
    // model() / argument×2 / option×2 / getArgument<T> / getOption<T> / hasOption / isParsed×2 /
    // has_env / get_env / get_env_or / set_env / unset_env / apply_env / expand_env / env_string /
    // print_env / set_builtin_options / schema / schema_string / snapshot / apply_snapshot /
    // to_yaml / save_config / from_yaml / load_config / env_to_yaml / yaml_to_env

    ContextBase &reset();                      // builtin_detail::resetModelData(&instData_) + clearLast()
    ContextBase &set_exit_code(int _Code) noexcept;
    int exit_code() const noexcept;

    ContextBase &parse(int _Argc, char const *_Argv[], char *_Envp[]);
    ContextBase &parse(std::error_code &_Ec, int _Argc, char const *_Argv[], char *_Envp[]) noexcept;
    ContextBase &parse(Tokens_T const &_Tokens, Envs_T const &_Env);
    ContextBase &parse(std::error_code &_Ec, Tokens_T const &_Tokens, Envs_T const &_Env) noexcept;

    virtual void once() = 0;                   // 纯虚接口一
    virtual void loop() = 0;                   // 纯虚接口二

    using Errors::dump;
    using Errors::getLast;
    using Errors::getLastMsg;
    using Errors::getLastVal;
    using Errors::isOk;

protected:
    void _markParsed() noexcept;               // bParsed_ = true
    auto _errors() noexcept -> Errors &;       // 派生类访问错误栈的唯一通道

    virtual void _doRunPipeline(Tokens_T &_Toks, Envs_T const &_Envs) = 0;  // 引擎钩子

protected:
    std::string strPrjName_;       ///< 工程名
    Model       instData_;         ///< 核心数据仓库
    bool        bParsed_   = false;///< 已完成一次解析
    int         nExitCode_ = 0;    ///< once() 退出码
};
```

- 派生类构造初始化引擎成员时直接取 `&instData_`（保护成员，合法）。
- 派生类访问 Errors 一律经 `_errors()`（Errors 是基类私有基，派生类不能自行转换）。
- 保护成员后缀 `_`（非 mutable，无 `m_` 前缀）。

- [ ] **Step 1: 创建 context_base.h**

把 context.h 当前类体按下述规则搬移改造（方法体除注明外逐字照抄，机械替换成员名：`strPrjName__ → strPrjName_`、`instData__ → instData_`，以及 Task 1 已生效的 Model 新名）：

```cpp
/* Apache 头（Copyright[2023] 保留原样亦可，新文件建议 [2026]，Email: pengwanring@live.com） */
#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <type_traits>

#include "cmd_args/base/def.h"
#include "cmd_args/base/util.h"

#include "cmd_args/config/config_store.h"
#include "cmd_args/env/env_builder.h"
#include "cmd_args/env/env_parser.h"
#include "cmd_args/error/errors.h"
#include "cmd_args/error/interpreter_error.h"
#include "cmd_args/error/lexer_error.h"
#include "cmd_args/error/parser_error.h"

#include "builtin_options.h"
#include "builder.h"
#include "lexer.h"
#include "model.h"

CMD_ARGS_NAMESPACE_BEGIN

/*
    上下文抽象基类：
    - 持有 Model 数据仓库与全部引擎无关能力（构筑/查询/环境变量/YAML 配置/schema）
    - parse() 公共包装负责 argv 归一化（程序名探测 + Lexer 分词 + envp 收集）
      与双通道错误处理（抛异常 / 错误码），引擎管线下沉给纯虚 _doRunPipeline
    - once()/loop() 为执行模式纯虚接口，由 ContextStep / ContextRuntime / 兼容版 Context 各自实现
*/
class ContextBase : private Errors
{
public:
    explicit ContextBase(std::string _Name)
        : strPrjName_(std::move(_Name))
    {}
    virtual ~ContextBase() = default;

    /* ---------- 构筑 ---------- */
    // （context.h:71-111 的 build<>/build()/addOption<>/addFlag 全部上移，
    //   返回类型 Context & 改为 ContextBase &，函数体中 Builder(&instData_) 不变）

    /* ---------- 数据访问 ---------- */
    // （model()/argument()/option()/getArgument/getOption/hasOption/isParsed 全部上移，
    //   函数体引用 instData_.args / instData_.opts）

    /* ---------- 环境变量交互 ---------- */
    // （has_env/get_env/get_env_or/set_env/unset_env/apply_env/expand_env/env_string/
    //   print_env/set_builtin_options 上移，返回 Context & 改为 ContextBase &，
    //   函数体引用 instData_.envs / instData_.bBuiltinOptions / strPrjName_）

    ContextBase &set_exit_code(int _Code) noexcept
    {
        nExitCode_ = _Code;
        return *this;
    }
    int exit_code() const noexcept { return nExitCode_; }

    /* ---------- 参数格式 / 快照 ---------- */   // schema/schema_string/snapshot/apply_snapshot/
                                                // to_yaml/save_config/from_yaml/load_config/
                                                // env_to_yaml/yaml_to_env 上移（引用 instData_）

    /* ---------- 执行状态 ---------- */

    ContextBase &reset()
    {
        builtin_detail::resetModelData(&instData_);
        clearLast();
        return *this;
    }

    /* ---------- 解析（公共包装：argv/tokens 归一化 + 双通道错误处理） ---------- */

    ContextBase &parse(int _Argc, char const *_Argv[], char *_Envp[])
    {
        try {
            Tokens_T toks;
            Envs_T   envs;
            __preambleArgv(_Argc, _Argv, _Envp, toks, envs);
            _doRunPipeline(toks, envs);
        }
        catch (ErrorParse const &e) { setLast(e.code()); throw; }
        catch (ErrorLexer const &e) { setLast(e.code()); throw; }
        return *this;
    }

    ContextBase &parse(std::error_code &_Ec, int _Argc, char const *_Argv[], char *_Envp[]) noexcept
    {
        try {
            Tokens_T toks;
            Envs_T   envs;
            __preambleArgv(_Argc, _Argv, _Envp, toks, envs);
            _doRunPipeline(toks, envs);
            _Ec = make_error_code(ParseError_E::OK);
        }
        catch (ErrorParse const &e) { _Ec = e.code(); setLast(e.code()); }
        catch (ErrorLexer const &e) { _Ec = e.code(); setLast(e.code()); }
        catch (std::exception const &ex) { setMsg(ex.what()); _Ec = make_error_code(ParseError_E::UNKNWON_ARGS); }
        return *this;
    }

    ContextBase &parse(Tokens_T const &_Tokens, Envs_T const &_Env)
    {
        try {
            Tokens_T real = _Tokens;             // OptionParser 按值消费，防外层 list 被吃掉
            __preambleTokens();
            _doRunPipeline(real, _Env);
        }
        catch (ErrorParse const &e) { setLast(e.code()); throw; }
        catch (ErrorLexer const &e) { setLast(e.code()); throw; }
        return *this;
    }

    ContextBase &parse(std::error_code &_Ec, Tokens_T const &_Tokens, Envs_T const &_Env) noexcept
    {
        try {
            Tokens_T real = _Tokens;
            __preambleTokens();
            _doRunPipeline(real, _Env);
            _Ec = make_error_code(ParseError_E::OK);
        }
        catch (ErrorParse const &e) { _Ec = e.code(); setLast(e.code()); }
        catch (ErrorLexer const &e) { _Ec = e.code(); setLast(e.code()); }
        catch (std::exception const &ex) { setMsg(ex.what()); _Ec = make_error_code(ParseError_E::UNKNWON_ARGS); }
        return *this;
    }

    /* ---------- 执行模式（纯虚接口，派生类按引擎注入实现） ---------- */

    virtual void once() = 0;
    virtual void loop() = 0;

    using Errors::dump;
    using Errors::getLast;
    using Errors::getLastMsg;
    using Errors::getLastVal;
    using Errors::isOk;

protected:
    /// @brief 标记「已完成一次解析」（once() 门控依据）
    void _markParsed() noexcept { bParsed_ = true; }

    /// @brief 派生类访问错误栈的唯一通道（Errors 为本类私有基）
    auto _errors() noexcept -> Errors & { return static_cast<Errors &>(*this); }

    /// @brief 引擎钩子：消费归一化后的 token 流与环境串（纯虚，派生类注入）
    virtual void _doRunPipeline(Tokens_T &_Toks, Envs_T const &_Envs) = 0;

protected:
    std::string strPrjName_;         ///< 工程名
    Model       instData_;           ///< 核心数据仓库
    bool        bParsed_   = false;  ///< 已完成一次解析
    int         nExitCode_ = 0;      ///< once() 退出码

private:
    EnvData *__envsMut() const
    {
        return &const_cast<Model &>(instData_).envs;
    }

    /// @brief argv 路径前置：程序名探测 + Lexer 分词 + envp 收集（语义与旧 __doParse(int,...) 前半段一致）
    void __preambleArgv(int _Argc, char const *_Argv[], char *_Envp[], Tokens_T &_Toks, Envs_T &_Envs)
    {
        if (_Argc > 0 && _Argv[0]) {
            instData_.strProgramName = _Argv[0];
        }
        else {
            instData_.strProgramName = strPrjName_;
        }

        if (_Argc > 1) {
            Lexer lex(_Argc - 1, _Argv + 1, nullptr);
            _Toks = lex.tokens();
        }

        if (_Envp) { for (int i = 0; _Envp[i]; ++i) { _Envs.emplace_back(_Envp[i]); } }
    }

    /// @brief tokens 路径前置：程序名为空时回落工程名（语义与旧 __doParse(Tokens,...) 前半段一致）
    void __preambleTokens()
    {
        if (instData_.strProgramName.empty()) {
            instData_.strProgramName = strPrjName_;
        }
    }
};

CMD_ARGS_NAMESPACE_END
```

搬移时的三条硬规则：
1. 上移方法的链式返回类型 `Context &` 一律改为 `ContextBase &`；
2. `set_exit_code/exit_code` 直写基类字段 `nExitCode_`（不再转发 Interpreter）；
3. 除上述与 `reset()` 外，其余方法体逐字保留（含注释），仅做成员名机械替换。

- [ ] **Step 2: 重写 context.h 为薄派生**

```cpp
/* Apache 头保留原样 */
#pragma once

#include "cmd_args/base/def.h"

#include "cmd_args/context/builtin_options.h"
#include "cmd_args/context/context_base.h"

#include "compiler.h"
#include "executor.h"
#include "interpreter.h"

CMD_ARGS_NAMESPACE_BEGIN

/*
    Context（兼容完整实现）：
    - 继承 ContextBase 全部引擎无关能力，自身持有三件套引擎（Compiler + Executor + Interpreter）
    - parse 管线：Compiler 纯解析入库 → hasAction 门控 Executor → finalize（批量语义）
    - once()/loop()：沿用历史行为——once 以退出码退出进程；loop 委托 Interpreter 流式 REPL
    - 兼容期保留：新代码请优先使用 make_context(E_STEP / E_RUN_TIME) 注入对应派生上下文
*/
class Context : public ContextBase
{
public:
    Context(std::string _Name)
        : ContextBase(std::move(_Name))
        , compiler__(&instData_)
        , executor__(&instData_)
        , interp__(&instData_)
    {}

    /* ---------- 执行模式（兼容语义） ---------- */

    void once() override
    {
        if (!bParsed_) { throw ErrorInterp(InterpError_E::STOPPED); }
        std::exit(nExitCode_);
    }

    void loop() override
    {
        compiler__.ensureBuiltins();
        interp__.loop(_errors());
    }

protected:
    /// @brief 批量管线：Compiler 纯解析 → hasAction 门控 Executor → finalize
    void _doRunPipeline(Tokens_T &_Toks, Envs_T const &_Envs) override
    {
        compiler__.set_program_name(instData_.strProgramName);
        compiler__.set_description(strPrjName_);

        compiler__.ensureBuiltins();
        compiler__.parse(_Toks, _Envs);      // 编译：Tokens → Expression 分类入库 Model

        if (instData_.hasAction())           // Model.hasAction() 门控
        {
            executor__.execute();            // 按 actionOrder 顺序执行
        }

        compiler__.finalize();               // seed 环境兜底 + 展开 + save-config

        _markParsed();
    }

private:
    Compiler    compiler__;
    Executor    executor__;
    Interpreter interp__;
};

CMD_ARGS_NAMESPACE_END
```

行为等价性核对清单（执行者自查，逐条打勾后再进 Step 3）：
- [ ] 旧 `once()` 门控读 Interpreter 的 bParsed__，新门控读基类 bParsed_——两者都只在 parse 管线尾部被置位，旧路径无其他写点 ⇒ 等价。
- [ ] 旧 `exit_code` 存于 Interpreter 字段、新存于基类字段；`set_exit_code` 是唯一写入口 ⇒ 等价。
- [ ] 旧 `loop()` 先 ensureBuiltins 再 interp.loop(errors)，errors 即 *this 向上转换；新经 `_errors()` 取同一子对象 ⇒ 等价。
- [ ] 旧 `__doParse` 两版本的程序名处理分别落入 `__preambleArgv/__preambleTokens`，管线主体逐行一致 ⇒ 等价。
- [ ] 旧 `bFired__` 只写不读，新版删除该成员不影响任何可见行为。

- [ ] **Step 3: 全量构建（example 也参与编译，即兼容性编译验证）**

Run: `xmake`
Expected: BUILD PASS。example/once、example/config 对 `Context demo("once_demo")` / `app.once()` 的既有用法不改一行即可编译——这就是「兼容期」的编译级证明。

- [ ] **Step 4: 全量测试回归**

Run: `xmake run unit_test`
Expected: 全部既有 TEST_CASE 通过，零失败。特别注意 test_interpreter.cpp 中 `app.once()` 抛 STOPPED、`app.loop()` REPL 两组用例（走的就是新 override）。

- [ ] **Step 5: Commit**

```bash
git add include/cmd_args/context/context_base.h include/cmd_args/context/context.h
git commit -m "refactor(context): introduce abstract ContextBase and rewire legacy Context onto it"
```

---

### Task 5: ContextStep（批量步进引擎派生类）

**Files:**
- Create: `include/cmd_args/context/context_step.h`
- Test: `dfx/unit_test/test_context_step.cpp`（新建）

**Interfaces:**
- Consumes: Task 4 的 `ContextBase`（`&instData_`、`_markParsed()`、`_errors()`、`_doRunPipeline` 钩子、`bParsed_/nExitCode_`）、Task 3 的 `builtin_detail::resetModelData`、`Lexer::tokensFromLine`、`util::trim`
- Produces:

```cpp
class ContextStep : public ContextBase
{
public:
    explicit ContextStep(std::string _Name);
    void once() override;    // 未 parse 抛 ErrorInterp(STOPPED)；否则 std::exit(nExitCode_)
    void loop() override;    // 批量 REPL：prompt→getline→行间 reset→tokensFromLine→_doRunPipeline→错误打印
protected:
    void _doRunPipeline(Tokens_T &_Toks, Envs_T const &_Envs) override;  // Compiler+Executor 批量管线
};
// 不变量：类内不存在任何 Interpreter / EnvParser 解释路径引用
```

- [ ] **Step 1: 写失败测试 test_context_step.cpp**

```cpp
#include "catch2/catch.hpp"

#include <iostream>
#include <sstream>
#include <string>

#include "cmd_args/cmd_args.hpp"

using namespace cmd_args;

/* ==================== ContextStep：批量步进引擎 ==================== */

TEST_CASE("step: 未 parse 直接 once() 抛 ErrorInterp(STOPPED)", "[ctx_step]")
{
    ContextStep app("ut_step_stopped");
    app.addOption<int>("timeout", "timeout in seconds", 30);

    REQUIRE_THROWS_AS(app.once(), ErrorInterp);
}

TEST_CASE("step: parse 批量管线与旧 Context 结果一致", "[ctx_step]")
{
    ContextStep app("ut_step_parse");
    app.build<ArgumentNamed>("hello", "Say hello");
    app.addOption<int>("timeout", "timeout in seconds", 30);

    std::error_code ec;
    char const *argv[] = { "test", "--hello", "a", "--timeout", "9", nullptr };
    app.parse(ec, 5, argv, nullptr);
    REQUIRE(!ec);
    REQUIRE(app.getArgument<std::string>("hello") == "a");
    REQUIRE(app.getOption<int>("timeout") == 9);
}

TEST_CASE("step: 内置 -? 短回路打印 usage 且中止后续解析", "[ctx_step]")
{
    ContextStep app("ut_step_help");
    app.addOption<int>("timeout", "timeout in seconds", 30);

    std::error_code ec;
    char const *argv[] = { "test", "-?", nullptr };

    std::ostringstream out;
    std::streambuf *oldOut = std::cout.rdbuf(out.rdbuf());
    app.parse(ec, 2, argv, nullptr);
    std::cout.rdbuf(oldOut);

    REQUIRE(!ec);
    REQUIRE(out.str().find("usage:") != std::string::npos);
}

TEST_CASE("step: loop 批量 REPL——行间独立、错误行展示后继续、exit 退出", "[ctx_step]")
{
    ContextStep app("ut_step_loop");
    app.build<ArgumentNamed>("hello", "h");
    app.addOption<int>("timeout", "t", 30);

    std::istringstream in("--hello a --timeout 9\n--nope x\n--hello b\nexit\n");
    std::ostringstream out;
    std::streambuf *oldIn  = std::cin.rdbuf(in.rdbuf());
    std::streambuf *oldOut = std::cout.rdbuf(out.rdbuf());

    app.loop();

    std::cin.rdbuf(oldIn);
    std::cout.rdbuf(oldOut);

    // 错误行被展示
    REQUIRE(out.str().find("(parse error)") != std::string::npos);
    // 行间独立（每轮等价一次 once 输入，不合并）：最后一次成功输入生效，未给项回默认
    REQUIRE(app.getArgument<std::string>("hello") == "b");
    REQUIRE(app.getOption<int>("timeout") == 30);
}
```

- [ ] **Step 2: 运行测试验证失败（红）**

Run: `xmake build unit_test`
Expected: FAIL——`ContextStep` 未定义（编译错误 'ContextStep': undeclared identifier）。

- [ ] **Step 3: 实现 context_step.h**

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

#include <iostream>
#include <string>

#include "cmd_args/base/def.h"
#include "cmd_args/base/util.h"
#include "cmd_args/error/errors.h"
#include "cmd_args/error/interpreter_error.h"
#include "cmd_args/error/lexer_error.h"
#include "cmd_args/error/parser_error.h"

#include "builtin_options.h"
#include "context_base.h"
#include "compiler.h"
#include "executor.h"

CMD_ARGS_NAMESPACE_BEGIN

/*
    ContextStep（批量步进上下文）：
    - 引擎注入：Compiler（纯解析入库）+ Executor（hasAction 门控后按 actionOrder 统一触发）
      【不变量：本类不含任何 Interpreter 引用】
    - parse：与兼容版 Context 相同的批量语义（短回路命中即中止本次解析）
    - once()：一次性执行完命令行参数后以退出码退出进程（未解析先调抛 STOPPED）
    - loop()：once 的循环化——每轮独立 reset → Lexer 分词 → 批量管线，识别 exit/quit/exit()
      后正常返回，否则自动等待下一段输入
*/
class ContextStep : public ContextBase
{
public:
    explicit ContextStep(std::string _Name)
        : ContextBase(std::move(_Name))
        , compiler__(&instData_)
        , executor__(&instData_)
    {}

    /// @brief 解析完成后以退出码退出进程
    /// @throw ErrorInterp(STOPPED) 未解析先调用
    void once() override
    {
        if (!bParsed_) { throw ErrorInterp(InterpError_E::STOPPED); }
        std::exit(nExitCode_);
    }

    /// @brief 批量 REPL：prompt → getline → 行间 reset → 批量管线 → 错误展示，exit/quit 退出
    void loop() override
    {
        std::cout << "cmd_args> " << std::flush;
        std::string line;
        while (std::getline(std::cin, line)) {
            auto cmd = util::trim(line);
            if (cmd.empty()) {
                std::cout << "cmd_args> " << std::flush;
                continue;
            }
            if (cmd == "exit" || cmd == "quit" || cmd == "exit()") break;

            _errors().clearLast();
            builtin_detail::resetModelData(&instData_);

            Tokens_T toks = Lexer::tokensFromLine(cmd);
            Envs_T   envs;
            try {
                _doRunPipeline(toks, envs);
            }
            catch (ErrorParse const &e) {
                _errors().setLast(e.code());
                std::cout << "(parse error) " << e.code().message() << "\n" << _errors().dump();
            }
            catch (ErrorLexer const &e) {
                _errors().setLast(e.code());
                std::cout << "(parse error) " << e.code().message() << "\n" << _errors().dump();
            }

            std::cout << "cmd_args> " << std::flush;
        }
        std::cout << "\n";
    }

protected:
    /// @brief 批量管线：Compiler 纯解析入库 → hasAction 门控 Executor → finalize
    void _doRunPipeline(Tokens_T &_Toks, Envs_T const &_Envs) override
    {
        compiler__.set_program_name(instData_.strProgramName);
        compiler__.set_description(strPrjName_);

        compiler__.ensureBuiltins();
        compiler__.parse(_Toks, _Envs);

        if (instData_.hasAction()) {
            executor__.execute();
        }

        compiler__.finalize();

        _markParsed();
    }

private:
    Compiler compiler__;   ///< 批量编译引擎
    Executor executor__;   ///< 批量动作执行器
};

CMD_ARGS_NAMESPACE_END
```

同时在 `cmd_args.hpp` include 区追加：

```cpp
#include "cmd_args/context/context_step.h"
```

- [ ] **Step 4: 运行测试验证通过（绿）**

Run: `xmake build unit_test && xmake run unit_test "[ctx_step]"`
Expected: 4 个 ctx_step 用例全部 PASS。

- [ ] **Step 5: 全量回归**

Run: `xmake run unit_test`
Expected: 全部 PASS（含既有用例）。

- [ ] **Step 6: Commit**

```bash
git add include/cmd_args/context/context_step.h include/cmd_args/cmd_args.hpp dfx/unit_test/test_context_step.cpp
git commit -m "feat(context): add ContextStep batch-engine context derived from ContextBase"
```

---

### Task 6: ContextRuntime（流式运行时引擎派生类，parse 也走流式）

**Files:**
- Create: `include/cmd_args/context/context_runtime.h`
- Test: `dfx/unit_test/test_context_runtime.cpp`（新建）

**Interfaces:**
- Consumes: `ContextBase`（Task 4）、`Interpreter(Model*)`（interpret/loop 接口）、Task 3 的 `registerBuiltinOptions`、`EnvParser(Model*)::parse(Envs_T const&)`
- Produces:

```cpp
class ContextRuntime : public ContextBase
{
public:
    explicit ContextRuntime(std::string _Name);
    void once() override;    // 同 ContextStep：STOPPED 门控 + exit(nExitCode_)
    void loop() override;    // registerBuiltinOptions + Interpreter 流式 REPL（比 once 多识别 exit 等退出信号）
protected:
    void _doRunPipeline(Tokens_T &_Toks, Envs_T const &_Envs) override;
    // 流式管线：envs 先行入库 → token 流拼回一行（' ' 连接）→ interpret 即时执行
    // 错误通道：interpret 返回的 ec 经 throw ErrorParse(ec) 透传原始错误码域
};
// 不变量：类内不存在任何 Compiler / Executor 引用
```

- [ ] **Step 1: 写失败测试 test_context_runtime.cpp**

```cpp
#include "catch2/catch.hpp"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "cmd_args/cmd_args.hpp"

using namespace cmd_args;

/* ==================== ContextRuntime：流式运行时引擎 ==================== */

TEST_CASE("runtime: 未 parse 直接 once() 抛 ErrorInterp(STOPPED)", "[ctx_runtime]")
{
    ContextRuntime app("ut_rt_stopped");
    REQUIRE_THROWS_AS(app.once(), ErrorInterp);
}

TEST_CASE("runtime: parse 走流式解释，结果与批量语义等价", "[ctx_runtime]")
{
    ContextRuntime app("ut_rt_parse");
    app.build<ArgumentNamed>("hello", "h");
    app.addOption<int>('t', "timeout", "timeout in seconds", 30);

    std::error_code ec;
    char const *argv[] = { "test", "--hello", "a", "-t", "9", nullptr };
    app.parse(ec, 5, argv, nullptr);
    REQUIRE(!ec);
    REQUIRE(app.getArgument<std::string>("hello") == "a");
    REQUIRE(app.getOption<int>("timeout") == 9);
}

TEST_CASE("runtime: parse 错误码透传（NEED_VALUE / UNKNOWN_OPTION）", "[ctx_runtime]")
{
    ContextRuntime app1("ut_rt_need");
    app1.addOption<int>("timeout", "t", 30);
    std::error_code ec1;
    char const *argv1[] = { "test", "--timeout", nullptr };
    app1.parse(ec1, 2, argv1, nullptr);
    REQUIRE(ec1 == make_error_code(ParseError_E::NEED_VALUE));

    ContextRuntime app2("ut_rt_unknown");
    std::error_code ec2;
    char const *argv2[] = { "test", "--nope", "x", nullptr };
    app2.parse(ec2, 3, argv2, nullptr);
    REQUIRE(ec2 == make_error_code(ParseError_E::UNKNOWN_OPTION));
}

TEST_CASE("runtime: 解析即执行——onSet 回调在 parse 期间即时触发", "[ctx_runtime]")
{
    ContextRuntime app("ut_rt_onset");
    app.addOption<std::string>("step", "streaming step value", "");

    std::vector<std::string> fired;
    app.option("step")->onSet([&](std::string const &_V) { fired.emplace_back(_V); });

    std::error_code ec;
    char const *argv[] = { "test", "--step", "one", nullptr };
    app.parse(ec, 3, argv, nullptr);
    REQUIRE(!ec);
    REQUIRE(fired == std::vector<std::string>{ "one" });   // 流式：赋值即刻回调
}

TEST_CASE("runtime: loop 流式 REPL——识别退出信号、行间独立、与旧 Context.loop 一致", "[ctx_runtime]")
{
    ContextRuntime app("ut_rt_loop");
    app.build<ArgumentNamed>("hello", "h");

    std::istringstream in("--hello a\nexit\n");
    std::ostringstream out;
    std::streambuf *oldIn  = std::cin.rdbuf(in.rdbuf());
    std::streambuf *oldOut = std::cout.rdbuf(out.rdbuf());

    app.loop();

    std::cin.rdbuf(oldIn);
    std::cout.rdbuf(oldOut);

    REQUIRE(out.str().find("(parse error)") == std::string::npos);
    REQUIRE(app.getArgument<std::string>("hello") == "a");   // 每轮独立解释，最后一轮生效
}
```

- [ ] **Step 2: 运行测试验证失败（红）**

Run: `xmake build unit_test`
Expected: FAIL——`ContextRuntime` 未定义。

- [ ] **Step 3: 实现 context_runtime.h**

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

#include <iostream>
#include <string>

#include "cmd_args/base/def.h"
#include "cmd_args/env/env_parser.h"
#include "cmd_args/error/errors.h"
#include "cmd_args/error/interpreter_error.h"
#include "cmd_args/error/lexer_error.h"
#include "cmd_args/error/parser_error.h"

#include "builtin_options.h"
#include "context_base.h"
#include "interpreter.h"

CMD_ARGS_NAMESPACE_BEGIN

/*
    ContextRuntime（流式运行时上下文）：
    - 引擎注入：Interpreter（逐字符分词 → Expression 分类 → 即时执行，无需 Executor/actionOrder）
      【不变量：本类不含任何 Compiler / Executor 引用】
    - parse：同样走流式——环境串先行入库，token 流以空格拼回一行交 interpret 即时解释
    - once()：执行一行输入即结束（以退出码退出进程；未解析先调抛 STOPPED）
    - loop()：once 的循环化——每轮 interpret 一行，比 once 多识别 exit/quit/exit() 退出信号
      （命中即正常返回而非退进程），否则自动等待下一段输入
*/
class ContextRuntime : public ContextBase
{
public:
    explicit ContextRuntime(std::string _Name)
        : ContextBase(std::move(_Name))
        , interp__(&instData_)
    {}

    /// @brief 解析完成后以退出码退出进程
    /// @throw ErrorInterp(STOPPED) 未解析先调用
    void once() override
    {
        if (!bParsed_) { throw ErrorInterp(InterpError_E::STOPPED); }
        std::exit(nExitCode_);
    }

    /// @brief 流式 REPL：委托 Interpreter（prompt / exit / 错误展示均由其内聚处理）
    void loop() override
    {
        builtin_detail::registerBuiltinOptions(&instData_);
        interp__.loop(_errors());
    }

protected:
    /// @brief 流式管线：envs 先行入库 → token 流拼行 → interpret 即时执行
    /// @note interpret 返回的错误码经 ErrorParse(ec) 原样透传（保留原始错误域与码值）
    void _doRunPipeline(Tokens_T &_Toks, Envs_T const &_Envs) override
    {
        builtin_detail::registerBuiltinOptions(&instData_);
        EnvParser(&instData_.envs).parse(_Envs);

        std::string line;
        for (auto const &t : _Toks) {
            line += t;
            line += ' ';
        }

        auto ec = interp__.interpret(line, _errors());
        if (ec) { throw ErrorParse(ec); }
        _markParsed();
    }

private:
    Interpreter interp__;   ///< 流式解释引擎
};

CMD_ARGS_NAMESPACE_END
```

`cmd_args.hpp` include 区追加：

```cpp
#include "cmd_args/context/context_runtime.h"
```

- [ ] **Step 4: 运行测试验证通过（绿）**

Run: `xmake build unit_test && xmake run unit_test "[ctx_runtime]"`
Expected: 5 个 ctx_runtime 用例全部 PASS。

- [ ] **Step 5: 全量回归**

Run: `xmake run unit_test`
Expected: 全部 PASS。

- [ ] **Step 6: Commit**

```bash
git add include/cmd_args/context/context_runtime.h include/cmd_args/cmd_args.hpp dfx/unit_test/test_context_runtime.cpp
git commit -m "feat(context): add ContextRuntime streaming-engine context with streaming parse"
```

---

### Task 7: EngineKind_E 枚举 + make_context 工厂 + 入口聚合

**Files:**
- Modify: `include/cmd_args/context/parse_fwd.h:36`（Envs_T 别名之后追加枚举）
- Create: `include/cmd_args/context/factory.h`
- Modify: `include/cmd_args/cmd_args.hpp`（追加 factory.h）
- Test: `dfx/unit_test/test_context_factory.cpp`（新建）

**Interfaces:**
- Consumes: `ContextStep`（Task 5）、`ContextRuntime`（Task 6）、`std::make_unique`
- Produces:

```cpp
// parse_fwd.h
enum class EngineKind_E : char
{
    E_STEP,      ///< 批量步进引擎（Compiler + Executor → ContextStep）
    E_RUN_TIME   ///< 流式运行时引擎（Interpreter → ContextRuntime）
};

// factory.h
auto make_context(EngineKind_E _Kind, Name_T _Name) -> std::unique_ptr<ContextBase>;
// @throw std::invalid_argument 未知种类
```

- [ ] **Step 1: 写失败测试 test_context_factory.cpp**

```cpp
#include "catch2/catch.hpp"

#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

#include "cmd_args/cmd_args.hpp"

using namespace cmd_args;

/* ==================== make_context 工厂注入 ==================== */

TEST_CASE("factory: 按 EngineKind 注入对应派生类型", "[ctx_factory]")
{
    auto step = make_context(EngineKind_E::E_STEP, "ut_f_step");
    REQUIRE(dynamic_cast<ContextStep *>(step.get()) != nullptr);
    REQUIRE(dynamic_cast<ContextRuntime *>(step.get()) == nullptr);

    auto rt = make_context(EngineKind_E::E_RUN_TIME, "ut_f_rt");
    REQUIRE(dynamic_cast<ContextRuntime *>(rt.get()) != nullptr);
    REQUIRE(dynamic_cast<ContextStep *>(rt.get()) == nullptr);
}

TEST_CASE("factory: 未知枚举值抛 invalid_argument", "[ctx_factory]")
{
    auto bad = static_cast<EngineKind_E>(99);
    REQUIRE_THROWS_AS(make_context(bad, "ut_f_bad"), std::invalid_argument);
}

TEST_CASE("factory: 基类指针多态分发 once/loop", "[ctx_factory]")
{
    std::unique_ptr<ContextBase> step = make_context(EngineKind_E::E_STEP, "ut_f_poly1");
    step->addOption<int>("timeout", "t", 30);
    REQUIRE_THROWS_AS(step->once(), ErrorInterp);   // 分发到 ContextStep::once

    std::unique_ptr<ContextBase> rt = make_context(EngineKind_E::E_RUN_TIME, "ut_f_poly2");
    rt->build<ArgumentNamed>("hello", "h");

    std::istringstream in("--hello a\nexit\n");
    std::ostringstream out;
    std::streambuf *oldIn  = std::cin.rdbuf(in.rdbuf());
    std::streambuf *oldOut = std::cout.rdbuf(out.rdbuf());

    rt->loop();                                     // 分发到 ContextRuntime::loop

    std::cin.rdbuf(oldIn);
    std::cout.rdbuf(oldOut);

    REQUIRE(rt->getArgument<std::string>("hello") == "a");
}
```

- [ ] **Step 2: 运行测试验证失败（红）**

Run: `xmake build unit_test`
Expected: FAIL——`EngineKind_E` / `make_context` 未定义。

- [ ] **Step 3: 实现——parse_fwd.h 追加枚举**

在 `parse_fwd.h` 第 36 行 `using Envs_T = ...;` 之后追加：

```cpp
/* 执行引擎种类：make_context 工厂据此注入对应派生上下文 */
enum class EngineKind_E : char
{
    E_STEP,      ///< 批量步进引擎（Compiler + Executor → ContextStep）
    E_RUN_TIME   ///< 流式运行时引擎（Interpreter → ContextRuntime）
};
```

- [ ] **Step 4: 实现——创建 factory.h**

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

#include <memory>
#include <stdexcept>
#include <string>

#include "cmd_args/base/def.h"
#include "cmd_args/context/parse_fwd.h"

#include "context_base.h"
#include "context_runtime.h"
#include "context_step.h"

CMD_ARGS_NAMESPACE_BEGIN

/**
 * @brief 上下文工厂：按引擎种类注入对应派生上下文
 * @param _Kind 引擎种类（E_STEP=批量步进 / E_RUN_TIME=流式运行时）
 * @param _Name 工程名
 * @return std::unique_ptr<ContextBase> 多态上下文（调用方持有所有权）
 * @throw std::invalid_argument 未知引擎种类
 */
inline auto make_context(EngineKind_E _Kind, Name_T _Name) -> std::unique_ptr<ContextBase>
{
    switch (_Kind) {
    case EngineKind_E::E_STEP:
        return std::make_unique<ContextStep>(std::move(_Name));
    case EngineKind_E::E_RUN_TIME:
        return std::make_unique<ContextRuntime>(std::move(_Name));
    default: break;
    }
    throw std::invalid_argument("unknown engine kind");
}

CMD_ARGS_NAMESPACE_END
```

- [ ] **Step 5: cmd_args.hpp 聚合入口**

include 区追加（放在现有 `#include "cmd_args/context/context.h"` 之后）：

```cpp
#include "cmd_args/context/context_step.h"
#include "cmd_args/context/context_runtime.h"
#include "cmd_args/context/factory.h"
```

- [ ] **Step 6: 运行测试验证通过（绿）**

Run: `xmake build unit_test && xmake run unit_test "[ctx_factory]"`
Expected: 3 个 ctx_factory 用例全部 PASS。

- [ ] **Step 7: 全量最终回归（含 example 编译）**

Run: `xmake && xmake run unit_test`
Expected: 全 workspace 构建成功（含 example/once、example/config）；unit_test 全部 PASS，无 FAILED。

- [ ] **Step 8: Commit**

```bash
git add include/cmd_args/context/parse_fwd.h include/cmd_args/context/factory.h include/cmd_args/cmd_args.hpp dfx/unit_test/test_context_factory.cpp
git commit -m "feat(factory): add EngineKind_E enum and make_context factory injecting step/runtime contexts"
```

---

## 明确不做（Out of Scope）

- 不修改 `example/once/main.cpp`、`example/config/main.cpp`——它们保持旧 `Context` 用法原样通过，正是兼容期的活证明；工厂用法示例可作后续独立任务。
- 不动 `Interpreter` 内部的 `bParsed__/nExitCode__` 字段（Task 1 已去 m_；独立使用 Interpreter 的测试仍依赖其 setExitCode/exitCode/prepared 接口）。
- 不更新 README 架构说明（待层次稳定后另行任务）。
- 不清理 compiler.h 因搬移而可能空置的标准库 include（最小化改动原则）。
- `program_name / description`（compiler.h 私有成员）虽缺 `__` 后缀属另一类风格问题，不在本次范围。

## Self-Review 记录

1. **Spec 覆盖**：once/loop 双纯虚接口 → Task 4 ✓；ContextStep 由 Compiler+Executor 注入且零 Interpreter → Task 5 ✓；ContextRuntime 由 Interpreter 注入、零 Compiler/Executor、parse 走流式 → Task 6 ✓；工厂入参枚举 E_STEP/E_RUN_TIME → Task 7 ✓；Context 保留兼容 → Task 4 重写为零行为差异薄派生并以既有全套用例验收 ✓；全库去 m_ 前缀（仅 mutable 保留）→ Task 1 含零残留验证步骤 ✓；once 维持 exit 语义 + loop⊃once 关系 → 语义对照表与两派生类注释 ✓。
2. **占位符扫描**：全文无 TBD/TODO；Task 3 Step 1 开头的残缺代码块是刻意标注的「防抄错警示」，并已明示「落盘以下面完整块为准、最终文件只允许一份实现」——执行者若不确定应只保留完整块。其余代码均为可直接落盘内容 ✓。
3. **类型一致性**：`EngineKind_E`（parse_fwd.h）↔ factory.h switch ↔ 测试断言同名；`_doRunPipeline(Tokens_T&, Envs_T const&)` 在基类与三个派生签名逐一比对一致；基类保护成员 `strPrjName_/instData_/bParsed_/nExitCode_` 与 Task 5/6 及兼容 Context 的引用拼写一致；`_markParsed/_errors` 名称全篇统一；Model 公有新名 `args/opts/envs/strProgramName/bBuiltinOptions/bBuiltinsRegistered/strSaveConfigPath` 在 describe.h/builtin_options.h/基类/派生类全部一致 ✓。
