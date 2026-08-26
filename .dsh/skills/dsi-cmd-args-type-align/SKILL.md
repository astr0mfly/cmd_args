---
name: dsi-cmd-args-type-align
description: 在 cmd_args 工程中维护核心类的类型别名对齐——检测接口位裸类型、推理语义归属、提出或复用别名、最小化修复并编译核验
---

# 类型别名推理与对齐

## 概述

模块核心类应用 `using Xxx_T = ...` 给类型起别名，后续切换模板只需改一处。但随着时间推移，部分接口位（数据成员、函数参数、返回类型）的类型使用可能偏离别名——这叫「类型泄露」。本技能提供系统化的检测、推理、对齐流程。

## 四步执行流程

按以下四步处理目标模块，缺一不可：

1. **盘点**：建立该模块的「三层别名清单」——全局层 `context_fwd.h`、核心类层（各类内 using）、缺口标记
2. **检测**：逐文件扫描接口位的裸类型，产出泄露位置清单
3. **推理**：为每个泄露点推断语义角色，映射已有别名或提出新别名
4. **对齐**：最小化修改 + 编译核验 + 复检报告

## 一、盘点——三层别名体系

### 第一层：全局别名（`context_fwd.h`）

整个工程共用，跨模块引用时优先使用：

| 别名 | 底层类型 | 语义 |
|------|---------|------|
| `Name_T` | `std::string` | 通用名字 |
| `NameShort_T` | `char` | 短名 |
| `NameLong_T` | `std::string` | 长名 |
| `Desc_T` | `std::string` | 描述/帮助 |
| `Tokens_T` | `std::list<std::string>` | Token 流 |
| `Envs_T` | `std::vector<std::string>` | 环境串列表 |

### 第二层：类内别名（核心类 using 声明）

每个核心类自持与自身语义强绑定的别名。**类内优先：类自身的方法优先使用自身声明的别名。**

### 第三层：缺口

当检测到裸类型且三层中无现成别名可接时——提出新别名（PascalCase + `_T`），声明在**语义拥有者**类内。

## 二、检测——找泄露

### 什么是「接口位」

接口位 = 该类型的使用者能**间接依赖**其身份的位置：

| 位置 | 示例 |
|------|------|
| 数据成员声明 | `std::function<...> fnOnSet__;` |
| 函数参数 | `void action(std::string _Cmd);` |
| 函数返回类型 | `auto get() -> std::string;` |
| 类型别名内部 | `using ActionOrder_T = std::vector<std::string>;`（内层 std::string 泄露） |
| 模板实参嵌套 | `std::function<void(std::string const &)>` 内层 `std::string` 泄露 |

### 豁免规则

| 类型 | 处理 |
|------|------|
| 基础类型 `bool / int / size_t / char` | ✅ 豁免，直接写（`char` 短名沿用 `NameShort_T` 是既有先例，不强制对所有 char 别名化） |
| 函数体内局部变量 | ✅ 豁免，属于实现细节 |
| `std::string::npos` / 字面量 | ✅ 豁免 |
| `std::function<...>`（复合类型） | ❌ 不豁免，需别名化 |
| `std::vector<...>` / `std::map<...>`（容器） | ❌ 不豁免，需别名化（除非该成员仅用于单次临时场景） |
| 模板内嵌套的具体类型 | ❌ 不豁免，需逐层剥离 |

### 检测算法

```
对每个头文件的每个接口位：
  提取该位置使用的全部具体类型（含模板实参中的类型）
  对每个具体类型：
    是基础类型？→ 跳过
    该位置已有别名可替换？→ 记录泄露
    无别名可接？→ 标记为"需新别名"
```

## 三、推理——定归属

### 语义角色推断

从以下线索推断类型在该位置的语义角色（查一推一）：

| 线索 | 推断 |
|------|------|
| 成员名前缀 `strValue` / `strHelp` | 值 / 描述 → `Value_T` / `Desc_T` |
| 成员名前缀 `fn` / `Callback` | 回调 → 新别名 `FnXxx_T` |
| 形参名 `_Help` / `_LName` | 描述 / 长名 → `Desc_T` / `NameLong_T` |
| 注释提到"类型标识" / "环境变量名" | → `Type_T` / `NameEnv_T` |
| 函数名 `find` / `getOption` / `hasOption` 的键参数 | → `Name_T` |
| 返回 `vector<string>` 语义为"激活列表" / "动作顺序" | → 新别名 `ActivationList_T` / `ActionOrder_T` |

### 映射决策

```
推断语义角色 → 在三层别名体系中查找：
  类内已有匹配？→ 用自身别名
  全局层已有匹配？→ 用全局别名（跨类时用限定名 ClassName::Alias_T，已有先例）
  无匹配？→ 提出新别名 PascalCase_T，声明在语义拥有者类内
```

### 新别名声明位置规则

| 别名用途 | 声明在 |
|---------|--------|
| 仅单个类使用 | 该类内 `using` |
| 多个同层类使用 | 最自然的拥有者类内，其他类用限定名 |
| 跨模块使用 | `context_fwd.h` 全局层 |

## 四、对齐——改+验

1. **最小化修改**：只改泄露项；不动风格已符合的内容
2. **编译核验**：修改后 `xmake build` 确认编译通过
3. **复检报告**：重新对照检测清单确认无新增泄露，报告遗留项

## 工程实例（option 模块）

### 需新增的别名（option.h）

| 新别名 | 声明位置 | 替换目标 |
|--------|---------|---------|
| `FnOnSet_T = std::function<void(Value_T const &)>` | `OptionValue` | `fnOnSet__` 成员 + `onSet()` 参数 |
| `ActionCmd_T = std::string` | `OptionValue` | `strActionTemplate__` 成员 + `action()` 参数 |
| `FnCallback_T = std::function<void(void)>` | `OptionAction` | `fnCallback` 成员 + 构造参数 |

### 需复用已有别名的位置（option.h → option_data.h）

| 裸类型位置 | 映射到 |
|-----------|--------|
| `setExternal(std::string const & / &&)` | `Value_T` |
| `OptionValue` 构造参数 `_SName`/`_LName`/`_Help` | 已正确使用 `Option::NameShort_T` 等 ✓ |
| `ValueOptMgr::push(char, std::string×4)` | `Option::NameShort_T` / `Option::NameLong_T` / `Option::Desc_T` / `OptionValue::Type_T` / `OptionValue::Value_T` |
| `ActionOptMgr::push(char, std::string×2, std::function<void(void)>)` | 同上 + `FnCallback_T` |
| `find()`/`mutFind()`/`hasOption()`/`getOption()` 的键参数 | `Name_T` |
| `recordParseOrder(std::string const &)` | `Name_T` |
| `actionOrder()` 返回 `vector<string>` | 新别名 `ActionOrder_T` |

### 「同源双声明」说明

`Option` 类内重新声明了全局已有的 `NameShort_T` / `NameLong_T` / `Desc_T`——这是本工程「核心类自持别名」的既定取向，**不强制删除**，技能标注但不修改此类冗余。

## 常见错误

- ❌ 只查数据成员、不管函数参数和返回类型——它们同样是接口位
- ❌ `std::function<void(std::string const &)>` 只别名化外层、忽略内层 `std::string`——模板实参内的裸类型也需对齐
- ❌ 把新别名全放 `context_fwd.h`——只放真正跨模块的，仅类内使用的放在类内
- ❌ 修完不编译验证——类型别名化可能引发重载/隐式转换问题

## 核验清单（完成后逐条检查）

- [ ] 全部接口位数据成员已对齐（无裸具体类型）
- [ ] 全部接口位函数参数/返回类型已对齐
- [ ] 模板实参嵌套的具体类型已逐层对齐
- [ ] 新别名声明位置正确（语义拥有者类内）
- [ ] `xmake build` 编译通过
- [ ] 无新增不匹配项
