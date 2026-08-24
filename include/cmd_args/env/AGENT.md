# env — 环境变量子系统

## 模块概述
环境变量的**一等公民**实现：存储、展示、交互（设置/删除）、引用与修改追踪、`${VAR}/%VAR%/$VAR` 展开、回写操作系统。这是 `cmd_args` 区别于其他命令行解析库的核心差异化功能。

## 文件职责

| 文件 | 类 | 职责 |
|------|---|------|
| `env.h` | `environment` / `envs_t` | 环境条目结构：`strKey` / `strValue` + 三标记（`bReferenced__` / `bModified__` / `bNew__`） |
| `env_data.h` | `EnvData` | 环境数据仓库：`std::map<Key, environment>` 按键排序去重，提供 doPush/doFind/doGet/doMutGet/doErase/doVisit/doMutVisit |
| `env_builder.h` | `EnvBuilder` | 写操作层：`setEnv`（存在→改值+标 modified，新建→标 new）、`unsetEnv` |
| `env_parser.h` | `EnvParser` | 解析/服务层：`parse(Envs_T)`、`get/getOr/has/touch`（含引用标记）、`expand`、`apply`、`envString`、`forEach`、`size/empty`、`referencedCount/changedCount` |

## 环境条目（`environment`）
```
struct environment {
    std::string strKey;     // 键
    mutable std::string strValue;    // 值（mutable：允许 const 指针读取时标记引用）
    mutable bool bReferenced__ = false;  // 被程序引用过（> 标记）
    mutable bool bModified__   = false;  // 被修改过（* 标记）
    mutable bool bNew__        = false;  // 本次新建（+ 标记）
};
```
> **设计说明**：`mutable` 成员是为兼容 `std::set<environment>` 中的 const 迭代器写标记；`map` 存储（`EnvData`）下非 const 迭代器也可直接写入，`mutable` 在 `EnvData` 路径下冗余但保留兼容性。

## 环境标记与展示格式
```
environments: <program>
No. : "key" = "value"
0   : "A" = "1"
1*  : "B" = "2"     (* 被修改)
2>  : "C" = "3"     (> 被引用)
3*+ : "D" = "4"     (+ 本次新建)
```

## 核心行为
- **优先级链**：CLI 显式值 > 配置文件(YAML) > 环境变量 > 内置默认值
- **展开**：`${NAME}` / `%NAME%` / `$NAME`，命中标记引用，未命中保持原样
- **回写**：`apply()` 只回写 `bModified__ || bNew__` 条目，返回 `"KEY=VALUE"` 列表
- **展示**：`envString(program)` 生成格式化展示（含 `*>+` 标记）

## 方法命名约定（EnvParser）
- 查询：`has` / `get` / `getOr` / `touch` / `find`。
- 修改：`EnvBuilder::setEnv` / `EnvBuilder::unsetEnv`。
- 展示/展开：`envString` / `expand`。
- 执行：`apply`（回写 OS）。
- 统计：`size` / `empty` / `referencedCount` / `changedCount`。
