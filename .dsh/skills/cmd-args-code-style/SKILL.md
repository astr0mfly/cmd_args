---
name: cmd-args-code-style
description: cmd_args 工程的代码风格（命名/auto后置返回/Doxygen中文注释/链式API），并规定四步核验流程
whenToUse: 在 cmd_args 工程编写/修改头文件、补注释、统一声明风格时
---
# argument.h 代码风格规范与核验流程

## 一、执行流程（必须逐条照做）

按以下四步处理待修改文件，缺一不可：

1. **对照规范清单**：把目标文件逐条对照下方"规范清单"逐项检查，判为"符合"或"不符合"（涉及文件头、命名、函数声明、注释、成员顺序、链式 API 六类）。
2. **列出不匹配项**：把不符合条目整理成清单，逐条写明：位置（行号）、违反的规范条目、当前写法、应改写法。
3. **修改不匹配项**：按清单逐一修正；修改是**最小化**的——只改不符合项，不动风格已符合的内容。
4. **核验修改是否成功**：修改后重新对照规范清单复检：每个不匹配项是否已消除、有无新增不匹配、编译是否通过；向用户报告"已核验通过"或"仍遗留 XX"。此步不允许跳过。

## 二、规范清单（argument.h 单页提炼）

### 1. 文件头与作用域
- 保留 Apache-2.0 许可头（Copyright[2026][ValenciaFly] / Email）。
- `#pragma once`；include：标准库 `<...>` 在前、工程头 `"cmd_args/..."` 在后。
- 命名空间一律 `CMD_ARGS_NAMESPACE_BEGIN/END`，禁用裸 `namespace cmd_args`。
- detail 实现入 `namespace XX_detail {}`，收尾前 `using XX_detail::X;` 导出。

### 2. 命名规范
- 类型别名：PascalCase + `_T`：`Name_T / Type_T / Value_T / Desc_T / Self_T / Seq_T`。
- 私有数据成员：小写类型前缀 + `__` 后缀：`strName__ / strValue__ / bExternal__`（bool → `b` 前缀）。
- 保护数据成员：小写类型前缀 + `_` 后缀：`strName_ / strValue_ / bExternal_`（bool → `b` 前缀）。
- 公开数据成员：小写类型前缀， 无后缀：`strName / strValue / bExternal`（bool → `b` 前缀）。
- mutable数据成员：在前面的基础上，使用`m_`前缀，比如`m_strName`、`m_strName_`、`m_strName__`
- 形参：`_` 前缀 + PascalCase：`_Name / _Help / _V / _Other / _Rhs / _EnvName / _Idx`。
- 方法：小写动词前缀；布尔谓词 `has / is` 前缀；getter `get` 前缀，+PascalCase。
- 公开方法：无前缀，保持 小写动词+PascalCase；`getName()`
- 保护方法：`_`前缀+小写动词+PascalCase，`_getName()`
- 私有方法：`__`前缀+小写动词+PascalCase，`__getName()`
- 虚函数方法: 使用`do`作为动词前缀，与权限叠加，比如 `doRun()`、`_doRun()`、`__doRun()`
- 引用 `T const &`（const 靠右）、右值 `T &&`。

### 3. 函数声明风格（关键）
- 返回类型长度 不是 4 字符 → `auto ... -> ReturnType` 后置（如 `-> Self_T &`、`-> Name_T const &`、`-> int`）。
- 返回类型恰 4 字符（`bool`、`void`）→ 经典前置、跳过（`bool hasValue()`、`operator bool()`）。
- 限定符顺序：参数 → `const` → `noexcept` → 后置返回；ref 限定符紧跟参数：`...(&) -> Self_T &`。
- 模板 `template <typename T>`，同样后置返回。
- 短方法可单行：`auto copy() const -> Self_T { return *this; }`。
- 不同函数声明至少保持一个空行，不要紧贴着

### 4. 类内成员顺序
1) `using` 别名 2) 构造 3) 赋值 4) 转换+析构 5) 链式 setter（& / && 双重载）6) getter 7) static方法 8) friend 声明 9)private 成员（`// ===== 分组 =====` 分区）。

### 5. 链式 API 惯例
- 为class名称XX 使用 `using Self_T = XX;`
- setter 返回 `*this`；`&` 版 `-> Self_T &`；`&&` 版 `return std::move(*this)` → `Self_T`。
- 形参按值接收后 `std::move` 入成员。

### 6. 注释规范（Doxygen，全中文）
- 类 `/* ... */` 概览；结构体 `/** @struct @brief */`。
- 简短方法 `/// @brief`；复杂方法 `/** @brief @param @return @note */`；trivial 行尾 `///<`。
- 成员变量行尾 `///<` + `// ===== 分组 =====` 分隔。

## 三、举例（argument.h 典范写法）

- 复杂方法样板：

```cpp
/**
 * @brief 还原为内置默认值
 * @return Self_T& 支持链式调用
 * @note 还原 strValue__ 为 strDefault__，清除 bExternal__ 标记
 */
auto reset() noexcept -> Self_T &
{
    strValue__ = strDefault__;
    bExternal__  = false;
    return *this;
}
```

- getter 样板：

```cpp
/// @brief 获取参数名称（只读）
auto getName() const noexcept -> Name_T const & { return strName__; }
```

## 四、反例

- 手写 `namespace cmd_args`（应用宏）。
- 长返回类型不用后置：`Argument &set(...)` → 应为 `auto set(...) -> Self_T &`。
- `bool` 误用后置：`auto isExternal() -> bool` → 应保持 `bool isExternal()`。
- 注释用英文或缺失；简单方法却写大段块注释（应视复杂程度选 `///` 或 `/** */`）。
