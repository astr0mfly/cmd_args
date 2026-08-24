# option — 选项子系统

## 模块概述
命令行选项（值选项 / 标志 / 短回路选项）的数据层、构筑层、解析层与执行层。支持选项 action（system_call 子进程），采用**三层架构**：纯解析 → 执行 → 实时引擎。

## 文件职责

| 文件 | 类 | 职责 |
|------|---|------|
| `option.h` | `Option` / `OptionValue` / `OptionAction` | 选项基础数据结构：短名/长名/帮助、值/类型/标记、action 模板、环境变量绑定 |
| `option_data.h` | `OptMgrItf` / `ValueOptMgr` / `ActionOptMgr` / `OptionData` | 管理器接口与实现 + 仓库（含 actionOrder 解析顺序） |
| `option_builder.h` | `OptionBuilder` | 构筑层：`addOption<T>` / `addFlag` / `addShortCircuit`，含重名/前缀/类型校验 |
| `option_parser.h` | `OptionParser` | **纯解析层**：赋值 + 标记 + 记录解析顺序（`onOptionAssigned` virtual hook = no-op） |
| `context/executor.h` | `Executor` | **执行层**：按 actionOrder 顺序统一触发 fnOnSet__ + action template |
| `context/interpreter.h` | `Interpreter` | **实时引擎**：继承三解析器，override `onOptionAssigned` 实现逐 Token 内联执行 |

## 三层架构（选项 action 执行）

```
Parser（纯解析）           Executor（后置执行）        Interpreter（实时执行）
┌────────────────┐        ┌────────────────┐         ┌──────────────────┐
│ OptionParser    │        │ 读 actionOrder │         │ 继承三解析器      │
│ (onOptionAssigned│  ──►  │ fnOnSet__ 触发 │  REPL   │ override 钩子     │
│  = no-op)      │ 批量   │ system_call 触发│ ◄────  │ 逐 Token 内联触发 │
└────────────────┘        └────────────────┘         └──────────────────┘
```

- **批量解析**（`Context::parse`）：Parser 纯解析 → Executor 按 actionOrder 执行 → finalize。
- **REPL/loop**（`Interpreter::loop`）：逐行 tokenize → 继承 OptionParser::parse（hook 触发 action）→ finalize。

## OptionParser 纯解析设计
- `__assign` 仅赋值 + `bExplicit__` 标记 + 记录解析顺序，**不触发 action**。
- `virtual onOptionAssigned(...)` 默认空操作；子类 override 实现内联 action。
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
- 解析：`parse`（OptionParser，pure virtual hook 模式）。
- 执行：`Executor::execute`（post-parse），`Interpreter::interpret`（per-token）。
