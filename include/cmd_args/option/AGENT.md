# option — 选项子系统

## 模块概述
命令行选项（值选项 / 标志 / 短回路选项）的数据层、构筑层、解析层与执行层。支持选项 action（system_call 子进程），执行时序分三路：内置选项解析期即时、用户 action 批量 Executor 后置（hasAction 门控）、流式 Interpreter 即时。

## 文件职责

| 文件 | 类 | 职责 |
|------|---|------|
| `option.h` | `Option` / `OptionValue` / `OptionAction` | 选项基础数据结构：短名/长名/帮助、值/类型/标记、action 模板、环境变量绑定 |
| `option_data.h` | `OptMgrItf` / `ValueOptMgr` / `ActionOptMgr` / `OptionData` | 管理器接口与实现 + 仓库（含 actionOrder 解析顺序） |
| `option_builder.h` | `OptionBuilder` | 构筑层：`addOption<T>` / `addFlag` / `addShortCircuit`，含重名/前缀/类型校验 |
| `option_parser.h` | `OptionParser` | **纯解析层**：赋值 + 标记 + 记录解析顺序；内置交互选项（--env-set 等）解析期即时触发回调 |
| `expression/executor.h` | `Executor` | **批量执行层**：hasAction 门控下按 actionOrder 顺序统一触发 fnOnSet__ + action template |
| `expression/interpreter.h` | `Interpreter` | **流式引擎**：字符泵逐 Token 分类，`__executeOptionImmediate` 即时执行 |

## 选项 action 时序

```
OptionParser（解析期）        Executor（批量后置）        Interpreter（流式即时）
┌──────────────────┐        ┌──────────────────┐        ┌──────────────────┐
│ 内置选项（--env-set│        │ hasAction() 门控  │        │ 字符泵逐 Token 分类 │
│ 等）赋值即回调     │        │ actionOrder 顺序  │        │ __executeOption  │
│ 用户选项记 actionOrder│ 批量  │ fnOnSet__/模板触发 │  流式   │ Immediate 即时执行 │
└──────────────────┘        └──────────────────┘        └──────────────────┘
```

- **批量解析**（`Context::parse`）：Compiler 编译入库 → `Model.hasAction()` 门控 → Executor 按 actionOrder 后置执行 → finalize。
- **REPL/loop**（`Interpreter::interpret`）：字符泵逐 Token 分类，选项命中即 `__executeOptionImmediate` 即时执行 → finalize。
- **内置交互选项**两条路径均在解析期即时生效（不记 actionOrder，Executor 不重复触发）。

## OptionParser 纯解析设计
- `__assign` 仅赋值 + `bExplicit__` 标记 + 记录解析顺序；**内置选项**（`bBuiltin__ && fnOnSet__`）
  内联触发回调使其影响后续解析；**用户选项**只记入 actionOrder，由 Executor 延迟执行。
- Action 记录：`OptionData::recordParseOrder`（去重，仅含 fnOnSet__ 或 strActionTemplate__ 的选项）。

## OptionData 扩展
- `actionOrder()` — 含 action 的选项解析顺序（Executor 按此顺序执行）。
- `recordParseOrder(key)` / `clearParseOrder()` — 解析前清空，解析期按需记录。

## OptionValue 标记
| 标记 | 含义 |
|------|------|
| `bExplicit__` | 被 CLI 显式赋值过（区别于默认值/环境变量兜底） |
| `bExternal__` | 值来自外部配置（YAML），env 兜底跳过 |
| `bBuiltin__` | 内置交互选项（--env-set 等），不参与 schema 导出 |
| `strEnvName__` | 绑定的环境变量名 |
| `fnOnSet__` | 自定义赋值回调钩子 |
| `strActionTemplate__` | system_call 命令模板，`{value}` 占位符替换 |

## 方法命名约定
- 构筑：`addOption` / `addFlag` / `addShortCircuit`。
- 校验：`checkAddOptionSname` / `checkAddOptionLname`。
- 解析：`parse`（OptionParser 纯解析层）。
- 执行：`Executor::execute`（post-parse），`Interpreter::interpret`（per-token）。
