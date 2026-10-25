# unit_test — cmd_args 单元测试（Catch2 单测目标）

> **摘要**：`dfx/unit_test/` 是 cmd_args 的核心回归测试集，使用 vendored Catch2 2.13.10，仅通过公共 API（`#include "cmd_args/cmd_args.hpp"` 与少量子模块头）驱动。当前 19 个 `test_*.cpp` 共 **175 个 TEST_CASE / 913 条断言**（2026-10-25 实测，全部通过）；`main.cpp` 只提供 `CATCH_CONFIG_MAIN`。xmake target 名 `unit_test`，默认不参与全量构建。
> **联系**：上级 [dfx/README.md](../README.md)、[include/cmd_args/cmd_args.md](../../include/cmd_args/cmd_args.md)；被测模块文档 [argument](../../include/cmd_args/argument/argument.md)、[option](../../include/cmd_args/option/option.md)、[env](../../include/cmd_args/env/env.md)、[property](../../include/cmd_args/property/property.md)、[context](../../include/cmd_args/context/context.md)、[expression](../../include/cmd_args/expression/expression.md)、[base](../../include/cmd_args/base/base.md)、[config](../../include/cmd_args/config/config.md)、[error](../../include/cmd_args/error/error.md)；同级样例 [example/example.md](../../example/example.md)；工程守则 [.dsh/Agent.md §9.4](../../.dsh/Agent.md)。

## truth

### 入口与构建

- `main.cpp`：`#define CATCH_CONFIG_MAIN` + `#include "catch2/catch.hpp"` + `#include "cmd_args/cmd_args.hpp"` + `using namespace cmd_args;`，无其它代码。
- `xmake.lua`（2026-10-25 核对）：

```lua
target("unit_test")
    set_kind("binary")
    set_default(false)
    add_files("*.cpp")
    add_includedirs("$(projectdir)/dfx/internal", {public = true})
    -- MSVC 多 TU 并行编译共用一个 PDB，需要 /FS
    if is_plat("windows") then
        add_cxflags("/FS", {force = true})
    end
```

- 运行：`xmake build unit_test` 后 `xmake run unit_test`；常用过滤器 `xmake run unit_test "[lexer]"`（Catch2 tag）。
- 断言库位于 `dfx/internal/catch2`（2.13.10，单头文件）；同目录还有 `FakeIt` 与 `tlp`，**unit_test 当前未使用**（`grep -l 'fakeit\|tlp' *.cpp` 无命中）。

### 测试文件清单（2026-10-25 实测计数）

| 文件 | TEST_CASE | 断言数 | 覆盖内容 |
| --- | --- | --- | --- |
| `test_lexer.cpp` | 27 | 119 | 词法：Lexer / Scanner（批量与流式、错误码） |
| `test_validation.cpp` | 23 | 190 | 统一校验：env spec / 必选 / 值校验（含 validator 语义） |
| `test_declarations.cpp` | 17 | 59 | `declare()` 折叠声明与 `OptionDef`/`FlagDef` |
| `test_env.cpp` | 14 | 83 | EnvData / EnvBuilder / EnvParser + Context 层联动 |
| `test_interpreter.cpp` | 11 | 61 | Interpreter 流式解释执行 |
| `test_parser.cpp` | 10 | 48 | OptionParser：token → Option(Expression) |
| `test_context.cpp` | 9 | 53 | Context 解析、错误码、短回路、loop、容器化 |
| `test_property.cpp` | 9 | 79 | `Property<T>` 写/读/变更钩子、默认值与版本号；Argument/OptionValue 读写拦截、数据源旁路 |
| `test_config.cpp` | 8 | 61 | YAML 配置 / schema / 往返 / load-save e2e |
| `test_expression_classifier.cpp` | 8 | 40 | ExpressionRecognizer：Token → Expression 归约 |
| `test_option_graph.cpp` | 7 | 22 | OptionGraph 条件/依赖边与拓扑执行 |
| `test_token.cpp` | 6 | 14 | 公共 Token 类型与字符串化 |
| `test_context_runtime.cpp` | 5 | 10 | ContextRuntime：流式 parse 引擎 |
| `test_context_step.cpp` | 4 | 9 | ContextStep：批量 step 引擎 |
| `test_expression.cpp` | 4 | 28 | Expression 类层次与 kind() 分类 |
| `test_model.cpp` | 4 | 12 | Model 查询 + Executor 批量执行 |
| `test_option.cpp` | 4 | 15 | 选项 action（system_call 子进程真实执行） |
| `test_context_factory.cpp` | 3 | 7 | `make_context(EngineKind_E)` 工厂注入 |
| `test_builder.cpp` | 2 | 3 | 构筑期重名检查 |
| **合计** | **175** | **913** | 19 个文件（全部通过） |

### 测试组织约定

- 每个 `test_*.cpp` 以 `/* ====== 主题 ====== */` 注释块开头，声明该文件覆盖范围。
- 用例按被测子系统拆分文件，而不是按被测类；文件内再用局部辅助函数/匿名命名空间隔离夹具。
- 断言以 `REQUIRE` 为主，仅在"可继续观察"处用 `CHECK`。
- 大量测试通过 `Context` 公共 API 端到端驱动（`build<>()` → `parse()` → `getOption<T>()`），而非直接构造内部对象；少数白盒测试直接 include 子模块头（`expression/token.h`、`option/option_graph.h`、`base/validator.h`、`expression/expression_recognizer.h`）。
- `test_option.cpp` 会把命令写成文件的可观测副作用，验证 action 真实执行（`system_call`）。

## 约束

- **只测公共 API 优先**：新增用例应优先经 `cmd_args.hpp` 驱动；直接 include 内部头仅限白盒必需，并需在文件头注释说明。
- 禁止修改 `dfx/internal/catch2` 等 vendored 代码；升级 Catch2 走整体替换。
- 一个 `test_*.cpp` 对应一个子系统主题，命名 `test_<主题>.cpp`；不得把无关用例塞进已有文件。
- 用例必须独立、可重复：不依赖外部网络、不依赖用户主目录状态；涉及文件的用例一律使用临时目录并自清理。
- 每次改动库行为必须同步保持或增加覆盖，**禁止回退测试数**（见 `.dsh/Agent.md §9.4`）。
- Windows 下 MSVC 需要 `/FS`，`xmake.lua` 已处理，不要删除该分支。
- 不提交测试产生的临时文件（如 `test_option` 的可执行文件/输出）。

## 索引

| 文件 | 关键符号 | 职责 |
| --- | --- | --- |
| `main.cpp` | `CATCH_CONFIG_MAIN` | 仅提供 Catch2 main |
| `xmake.lua` | `target("unit_test")` | 单测构建配置（默认关闭、/FS） |
| `test_lexer.cpp` | Lexer / Scanner | 词法与流式分词 |
| `test_token.cpp` | `Token` / `KeyWord_E` | Token 类型 |
| `test_parser.cpp` | `OptionParser` | token → Option |
| `test_builder.cpp` | `Builder` 重名检查 | 构筑期约束 |
| `test_declarations.cpp` | `declare()` / `OptionDef` / `FlagDef` | 声明式接口 |
| `test_expression.cpp` | `Expression` / `ExpressionKind_E` | 类层次 |
| `test_expression_classifier.cpp` | `ExpressionRecognizer` | Token → Expression |
| `test_interpreter.cpp` | `Interpreter` | 流式解释 |
| `test_model.cpp` | `Model` / `Executor` | 仓库查询与批量执行 |
| `test_context.cpp` | `Context` | 解析主流程 |
| `test_context_step.cpp` | `ContextStep` | 批量 step |
| `test_context_runtime.cpp` | `ContextRuntime` | 流式 parse |
| `test_context_factory.cpp` | `make_context` | 工厂 |
| `test_env.cpp` | `EnvData`/`EnvBuilder`/`EnvParser` | 环境变量 |
| `test_property.cpp` | `Property<T>` / 写读钩子 | 属性读写拦截、默认值、版本号 |
| `test_config.cpp` | `ConfigStore` / YAML | 配置往返 |
| `test_validation.cpp` | `Validator` / `Result` | 统一校验 |
| `test_option.cpp` | action / `system_call` | 选项 action |
| `test_option_graph.cpp` | `OptionGraph` | 条件/依赖图 |

## 决策

- **按子系统拆文件**：单文件会膨胀到不可导航；按 `test_*.cpp` 拆分使失败能直接定位子系统。
- **Catch2 2.13.10 单头 vendored**：避免包管理依赖，符合"零运行时依赖"定位；代价是版本升级需手工替换。
- **不引入 gmock/FakeIt**：`FakeIt` 虽已 vendored，但当前测试都用真实对象 + 文件副作用验证，mock 只在需要时再引入，避免维护双套夹具。
- **保留 `main.cpp` 仅一行宏**：与 Catch2 官方推荐一致，避免每个测试 TU 重复定义 main。

## 教训

- **`main.cpp` 的注释清单已过时**：其注释只列 10 类测试，而目录实有 19 个 `test_*.cpp`（缺 `test_token`、`test_declarations`、`test_option_graph`、`test_context_step`、`test_context_runtime`、`test_context_factory`、`test_expression_classifier`、`test_validation`、`test_property`）。加测试文件时同步该注释或直接依赖本表。
- **数量漂移**：`.dsh/Agent.md` 曾写 111 TEST_CASE / 523 断言，2026-10-25 引入 Property 后实测 175 / 913（全部通过）；引用数字必须注明实测日期。
- **失败用例会截断断言计数**：Catch2 在首个 `REQUIRE` 失败处中止该用例，其后断言不计；因此修复失败用例会同时抬高总数。2026-10-25 修复 `test_lexer`（EnvLine 值位置标志）与 `test_expression_classifier`（过期期望）后，两文件断言从 115/34 变为 119/40，总数 903→913。逐文件断言数用互斥 tag 测得（`[lexer]`、`[classifier]`；其余 tag 跨文件复用不可用），恒等式 `754 + 119 + 40 = 913` 与全量直跑互相校验；引用数字必须注明实测日期。
- **action 测试依赖真实子进程**：`test_option.cpp` 通过写文件观测，跨平台命令差异（`cmd`/`sh`）易失败；改 action 执行层时必须同时验证 Windows 与 Linux。
- **不要用 `std::filesystem` 相对当前目录做断言**：测试运行目录由 xmake 决定，一律用临时目录绝对路径。
- **`strValue` 公有字段已删除**：`OptionValue::strValue` 被 `property()` 取代后，测试改用 `getValue()` 读、`setValue()` 写；断言直接引用 `strValue` 会编译失败，不能用 `#define private public` 绕过（违反「只测公共 API」）。