# argument — 参数子系统

## 模块概述
命令行参数（命名参数 / 方位参数）的数据层、构筑层与解析层。
Argument 自 2026-08 起继承 `context/expression.h` 的 `Expression` 基类（`kind()` 自动分类，
`ExpressionKind_E::Argument`），`getName()` 返回自有存储（基类存储由构造时传入）。

## 文件职责

| 文件 | 类 | 职责 |
|------|---|------|
| `argument.h` | `Argument` / `ArgumentNamed` / `ArgumentPosition` | 参数基础数据结构：名称、帮助文本、类型标识、值（含默认值缓存）、环境变量绑定、外部标记 |
| `argument_data.h` | `ArgumentMgrItf` / `NamedArgumentMgr` / `PositionArgumentMgr` / `ArgumentData` | 管理器接口与实现 + 仓库（持有两组管理器） |
| `argument_builder.h` | `ArgumentBuilder` | 构筑层：`build<ArgumentNamed>` / `build<ArgumentPosition>`，含重名/空名/空帮助校验 |
| `argument_parser.h` | `ArgumentParser` | 解析层：`parseNamedOnly` / `parsePositionalOnly` / `getArgument<T>` / `isParsed` |

## 核心设计

### Argument 层次
```
Argument (基类)
  ├── ArgumentNamed   (命名参数，按名称赋值)
  └── ArgumentPosition (方位参数，按声明顺序赋值)
```

### 数据管理器 (ArgumentData)
- `NamedArgumentMgr`：以名称为键（`std::map`），提供 `doPush/doFind/doGet/doMutGet/doVisit/doConVisit/doGetHelp`。
- `PositionArgumentMgr`：以声明序号 `iSeq` 为键，额外提供 `mutGetBySeq`。
- `ArgumentData`：组合两组管理器（`mgrName` + `mgrPos`），作为 Model 的成员。

### ArgumentBuilder
- `build<T>(name, help)` — 空名/空帮助/重名校验后写入对应管理器。
- `build(ArgumentNamed &&/const &)` / `build(Group)` — 对象形态直接构筑。

### ArgumentParser
- `parseNamedOnly(tokens)` — 仅消化 `--name=value` 形式的命名参数 token。
- `parsePositionalOnly(tokens)` — 按声明顺序分配剩余 token 到方位参数。
- `getArgument<T>(name)` — 带类型校验读取（`type_string<T>` 严格匹配）。
- `isParsed(name)` — 命名参数是否已被解析出非空值。
- 数量校验：剩余 token 须落在 `[未外部赋值数, 总数]` 区间。

## 方法命名约定
- 构筑：`build`（保持小写）。
- 解析：`parseNamedOnly` / `parsePositionalOnly`。
- getter：`getArgument` / `getType` / `getName` / `getHelp` / `getEnvName`。
- 链式：`name(Name_T &&) &` / `name(Name_T &&) &&` → 返回 `Self_T &` / `Self_T`。
