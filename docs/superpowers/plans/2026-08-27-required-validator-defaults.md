# 必需检查 / 值校验器 / 默认值判断 升级计划（Required / Validators / Defaults）

> **For agentic workers:** REQUIRED SUB-SKILL: Use subagent-driven-development (recommended) or executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为 Argument / OptionValue / environment 三类条目补齐**必需检查（required）**、**值校验器（validators）**与**默认值判断（hasDefault / usesDefault）**，并配套语义类型校验（ipv4 / ipv6 / port / file / date / duration / hostname / uuid / float / double 等）。

**Architecture:** 值在本库统一以字符串存储（`strValue`），因此校验器统一为「字符串语义校验」：`base/validator.h` 提供纯函数校验集（`is_ipv4` 等）与 `Validator` 结构（名称 + 校验函数 + 失败消息），`ValidatorMixin<Self_T>` 以 CRTP 方式给 Argument / OptionValue / EnvSpec 注入同一条链式 API（`required()` / `range()` / `oneOf()` / `match()` / `semantic()` / `check()`），避免三个类重复实现。校验时机固定在**赋值完成后、动作执行前**：批量管线 `Compiler.finalize()` 内（seed 环境兜底 → `${VAR}` 展开 → 校验）；流式管线 `Interpreter.__finalize()` 内（流式 action 本就即时，保持现状，仅行末补校验）。必选/校验失败使用**新错误域** `ValidationError_E`（MISSING_ARGUMENT / MISSING_OPTION / MISSING_ENV / VALIDATION_FAILED），经既有 `std::error_code` 双通道返回。

**Tech Stack:** C++20 header-only（xmake 构建，MSVC `/utf-8`、`-Wall -Werror`）、vendored Catch2 v2 单测、STL（`<functional>` / `<regex>` / `<filesystem>` / `<string>`）。

**Spec:** `README.md` 的「未来升级计划」章节（值验证器 / 默认值增强 / 必选约束 / Option 类型扩充），本计划为该章节的第一期落地，并以「语义类型校验器」路线实现类型扩充项。执行者应同时阅读 `.dsh/skills/cmd-args-code-style/SKILL.md`（命名 / 后置返回 / Doxygen 中文注释 / 链式 API / 四步核验流程）。

## Global Constraints

- **header-only**：所有新代码放 `include/cmd_args/...` 头文件（`inline` 或 `inline` 模板），不得引入 `.cpp`；新单测放 `dfx/unit_test/test_*.cpp`。
- **许可头**：每个新建头文件复制现有 Apache-2.0 许可头（`Copyright[2026][ValenciaFly]` / `Email: pengwanring@live.com`，见 `option.h` 1-17 行）。
- **代码风格**：执行 `cmd-args-code-style` 四步核验流程——对照规范清单 → 列出不匹配项 → 最小化修正 → 复检。要点：类型别名 `PascalCase + _T`；私有成员 `小写类型前缀 + __`；mutable 成员 `m_` 前缀（沿用现有 `bEnvHit__` 双下划线风格时保持一致性，见 `OptionValue::bExplicit__`）；形参 `_` 前缀 + PascalCase；返回类型长度 ≠ 4 字符必须 `auto ... -> T` 后置（`bool` / `void` 除外）；链式 setter `&` 与 `&&` 双重载；类内成员顺序 using → 构造 → 赋值 → 析构 → setter → getter → private；注释全中文 Doxygen。
- **错误域惯例**（照抄 `error/parser_error.h` 三件套）：`enum class XxxError_E`（OK=0）→ `mapXxxError` 文本映射 → `XxxErrorCategory::instance()` → `make_error_code` / `make_error_condition` → `std::is_error_code_enum` 特化。
- **测试命令**（每条任务通用）：
  ```bash
  xmake build unit_test
  xmake r unit_test -- "[tag]"     # tag 过滤；不带 tag 跑全量
  ```
  新增测试一律追加到 `dfx/unit_test/test_validation.cpp`（T1 起创建），按 tag 组织：`[validator]`（纯函数/工厂）、`[required]`（必选）、`[defaults]`（默认值）、`[validation]`（引擎/错误域 e2e）。
- **重要约束——不改公开链签名**：`Context::build<ArgumentNamed>(...).build<...>()` 链式写法在 `example/*` 与 `dfx/unit_test/test_context.cpp` 等被广泛使用，**禁止**改变 `ContextBase::build<T>(...)` 的返回类型。本计划的所有新增链式配置一律走既有句柄入口 `app.option("x")->...` / `app.argument("x")->...`（与 README 中 `->required()` 的意图像素级一致，句柄即是该语义的落地形式）。
- **优先级链不变**：CLI 显式值 > YAML 配置 > 环境变量 > 内置默认值；本计划不改动任何赋值优先级。
- **默认真实日期**：写测试时使用当前日期 `2026-08-27` 与两年内的闰年 `2024-02-29`。

## File Structure

| 文件 | 责任 | 动作 |
|---|---|---|
| `include/cmd_args/base/validator.h` | 语义校验纯函数 + `Validator` + `ValidatorMixin` + 工厂函数/语义表 | 新建（T1、T2） |
| `include/cmd_args/error/validation_error.h` | `ValidationError_E` 错误域（三件套 + `ErrorValidation`） | 新建（T6） |
| `include/cmd_args/expression/validation.h` | 校验引擎：`validate(Model&)`（env spec 补全/必选/值校验） | 新建（T7） |
| `include/cmd_args/option/option.h` | `OptionValue`：mixin 挂载、`bEnvHit__`、`isSatisfied`/默认值判断、`defaultExpr`/`defaultFn` | 修改（T3、T7、T8） |
| `include/cmd_args/argument/argument.h` | `Argument`：mixin 挂载、`bEnvHit__`、`isSatisfied`/默认值判断、`defaultExpr`/`defaultFn` | 修改（T4、T7、T8） |
| `include/cmd_args/env/env.h` | `EnvSpec` 声明式环境条目（继承 mixin + `defaultEnv`） | 修改（T5） |
| `include/cmd_args/env/env_data.h` | `EnvData`：spec 容器（`pushSpec`/`mutSpec`/`forEachSpec`） | 修改（T5） |
| `include/cmd_args/env/env_builder.h` | `EnvBuilder::requireEnv` / `envSpec` | 修改（T5） |
| `include/cmd_args/context/context_base.h` | `requireEnv` / `envSpec` 转发；`parse` 双通道补 `ErrorValidation` | 修改（T5、T6） |
| `include/cmd_args/context/context.h` | 批量管线顺序：finalize（含校验）在 Executor 之前 | 修改（T7） |
| `include/cmd_args/expression/compiler.h` | `finalize()` 内插入校验；`__seedEnvDefaults` 置 `bEnvHit__`；`__tryParse` 补 catch | 修改（T7） |
| `include/cmd_args/expression/interpreter.h` | `__finalize()` 内插入校验；`__seedEnvDefaults` 置 `bEnvHit__`；`interpret` 补 catch | 修改（T7） |
| `include/cmd_args/env/env_parser.h` | `expand` 支持 `${VAR:-fallback}` | 修改（T8） |
| `include/cmd_args/config/parameter.h` | `parameter_entry` 增加 `required`/`default_set`/`uses_default`/`validators` | 修改（T9） |
| `include/cmd_args/config/config_store.h` | `schema_of` 填充新字段 | 修改（T9） |
| `include/cmd_args/context/context_base.h` | `schema_string()` 展示新标记 | 修改（T9） |
| `dfx/unit_test/test_validation.cpp` | 全部新测试 | 新建（T1 起） |
| `example/validate/main.cpp`、`example/validate/xmake.lua`、`example/xmake.lua` | 新示例 target `validation` | 新建/修改（T10） |
| `README.md` | 用法示例 + 已实现列表 + 未来计划状态 | 修改（T10） |

**接口约定（跨任务契约，执行者按此对接）：**

- `validator_detail::is_ipv4(std::string const&) -> bool`（T1，纯函数集 `is_ipv4/is_ipv6/is_port/is_float/is_double/is_date/is_duration/is_hostname/is_uuid/is_path/is_existing_file/is_existing_dir`）。
- `struct Validator { std::string strName; std::function<bool(std::string const&)> fnCheck; std::string strFailMsg; auto validate(std::string const&) const -> std::string; }`（T2；`validate` 空串 = 通过）。
- `template <typename Self_T> class ValidatorMixin`（T2）：`required()(&/&&)`、`required(bool)(&/&&)`、`check(Validator const&)(&/&&)`、`check(Fn, name)(&/&&)`、`range(double, double, bool = true, bool = true)(&/&&)`、`oneOf(std::initializer_list<std::string>)(&/&&)`、`match(std::string const&)(&/&&)`、`semantic(std::string const&)(&/&&)`、`isRequired() const -> bool`、`hasValidators() const -> bool`、`validators() const -> std::vector<Validator> const&`、`validate(std::string const&) const -> std::string`。
- `OptionValue`（T3）：新增 `isSatisfied() const -> bool`、`hasDefault() const -> bool`、`defaultValue() const -> Value_T const&`、`usesDefault() const -> bool`、mutable 成员 `bEnvHit__`；T8 新增 `defaultExpr(Value_T const&) -> Self_T&`、`defaultFn(FnDefault_T) -> Self_T&`、`FnDefault_T = std::function<std::string()>`、成员 `fnDefault__`、`bDefaultExpr__`。
- `Argument`（T4）：新增同组方法（`isSatisfied`/`hasDefault`/`defaultValue`/`usesDefault`）+ `markEnvHit()/clearEnvHit()/isEnvHit()` + 成员 `bEnvHit__`（private）；T8 新增 `defaultExpr`/`defaultFn`/`FnDefault_T`/`fnDefault__`/`bDefaultExpr__`。
- `EnvSpec`（T5，env.h）：继承 `ValidatorMixin<EnvSpec>`；成员 `strKey/strHelp/strDefault__`；方法 `defaultEnv(std::string)(&/&&)`；`EnvData::pushSpec/mutSpec/forEachSpec`；`EnvBuilder::requireEnv(Key, Help = {}) -> EnvSpec*`、`EnvBuilder::envSpec(Key) -> EnvSpec*`；`ContextBase::requireEnv` / `ContextBase::envSpec`。
- `ValidationError_E`（T6）：`OK = 0, MISSING_ARGUMENT, MISSING_OPTION, MISSING_ENV, VALIDATION_FAILED`；`ErrorValidation(ValidationError_E, std::string&&)` 可携带描述。
- `validate_detail::validate(Model&) -> validate_detail::Result`（T7；`Result{std::error_code ec; std::string msg; bool ok() const;}`）。
- `parameter_entry`（T9）：新增 `bool required{false}; bool default_set{false}; bool uses_default{false}; std::vector<std::string> validators;`。

---

### Task 1: 语义校验纯函数库（base/validator.h — 第一部分）

**Files:**
- Create: `include/cmd_args/base/validator.h`
- Test: `dfx/unit_test/test_validation.cpp`（新建）

**Interfaces:**
- Consumes: `cmd_args/base/def.h`、`cmd_args/base/util.h`（`util::split` / `util::trim` / `util::replace`）。
- Produces: `validator_detail` 命名空间下的纯函数（T2 的工厂函数直接调用）：
  `bool is_port(std::string const&) noexcept`、`bool is_ipv4(std::string const&) noexcept`、`bool is_ipv6(std::string const&) noexcept`、`bool is_float(std::string const&) noexcept`、`bool is_double(std::string const&) noexcept`、`bool is_date(std::string const&) noexcept`、`bool is_duration(std::string const&) noexcept`、`bool is_hostname(std::string const&) noexcept`、`bool is_uuid(std::string const&) noexcept`、`bool is_path(std::string const&) noexcept`、`bool is_existing_file(std::string const&) noexcept`、`bool is_existing_dir(std::string const&) noexcept`。

- [ ] **Step 1: 写失败测试（建立 test_validation.cpp）**

创建 `dfx/unit_test/test_validation.cpp`：

```cpp
#include "catch2/catch.hpp"

#include <filesystem>
#include <fstream>
#include <string>

#include "cmd_args/cmd_args.hpp"        // 全家桶（后续任务经它拿到 Context / OptionValue）
#include "cmd_args/base/validator.h"   // T1 仅覆盖纯函数；两处都包含确保独立编译

using namespace cmd_args;

TEST_CASE("validator: is_port 接受 1..65535，拒绝越界与非数字", "[validator]")
{
    REQUIRE(validator_detail::is_port("80") == true);
    REQUIRE(validator_detail::is_port("1") == true);
    REQUIRE(validator_detail::is_port("65535") == true);
    REQUIRE(validator_detail::is_port("0") == false);
    REQUIRE(validator_detail::is_port("65536") == false);
    REQUIRE(validator_detail::is_port("-1") == false);
    REQUIRE(validator_detail::is_port("80a") == false);
    REQUIRE(validator_detail::is_port("") == false);
}

TEST_CASE("validator: is_ipv4 四段 0..255", "[validator]")
{
    REQUIRE(validator_detail::is_ipv4("192.168.1.1") == true);
    REQUIRE(validator_detail::is_ipv4("0.0.0.0") == true);
    REQUIRE(validator_detail::is_ipv4("255.255.255.255") == true);
    REQUIRE(validator_detail::is_ipv4("256.0.0.1") == false);
    REQUIRE(validator_detail::is_ipv4("1.2.3") == false);
    REQUIRE(validator_detail::is_ipv4("1.2.3.4.5") == false);
    REQUIRE(validator_detail::is_ipv4("a.b.c.d") == false);
    REQUIRE(validator_detail::is_ipv4("") == false);
}

TEST_CASE("validator: is_ipv6 标准/压缩/内嵌 IPv4", "[validator]")
{
    REQUIRE(validator_detail::is_ipv6("2001:db8::1") == true);
    REQUIRE(validator_detail::is_ipv6("::1") == true);
    REQUIRE(validator_detail::is_ipv6("::") == true);
    REQUIRE(validator_detail::is_ipv6("1:2:3:4:5:6:7:8") == true);
    REQUIRE(validator_detail::is_ipv6("::ffff:192.168.1.1") == true);
    REQUIRE(validator_detail::is_ipv6("2001:db8::1::2") == false);  // 两次压缩
    REQUIRE(validator_detail::is_ipv6("1:2:3:4:5:6:7:8:9") == false);  // 九组
    REQUIRE(validator_detail::is_ipv6("1:2:3:4:5:6:7:") == false);  // 单冒号结尾
    REQUIRE(validator_detail::is_ipv6(":1:2") == false);            // 单冒号开头
    REQUIRE(validator_detail::is_ipv6("g::1") == false);            // 非 hex
}

TEST_CASE("validator: is_float / is_double 数值语义", "[validator]")
{
    REQUIRE(validator_detail::is_float("1.5") == true);
    REQUIRE(validator_detail::is_float("-2.25e3") == true);
    REQUIRE(validator_detail::is_float("1e999") == false);  // 溢出 / inf
    REQUIRE(validator_detail::is_float("abc") == false);
    REQUIRE(validator_detail::is_float("1.2x") == false);
    REQUIRE(validator_detail::is_float("") == false);
    REQUIRE(validator_detail::is_double("0.1") == true);
    REQUIRE(validator_detail::is_double("3e-2") == true);
    REQUIRE(validator_detail::is_double("nan") == false);
    REQUIRE(validator_detail::is_double("inf") == false);
}

TEST_CASE("validator: is_date 真实日历与闰年", "[validator]")
{
    REQUIRE(validator_detail::is_date("2026-08-27") == true);
    REQUIRE(validator_detail::is_date("2024-02-29") == true);   // 闰年
    REQUIRE(validator_detail::is_date("2023-02-29") == false);  // 非闰年
    REQUIRE(validator_detail::is_date("2026-13-01") == false);
    REQUIRE(validator_detail::is_date("2026-00-10") == false);
    REQUIRE(validator_detail::is_date("2026-8-27") == false);   // 缺前导零
    REQUIRE(validator_detail::is_date("2026/08/27") == false);  // 分隔符错误
    REQUIRE(validator_detail::is_date("2026-02-30") == false);
}

TEST_CASE("validator: is_duration 数字[.小数] + 单位 s/m/h/d", "[validator]")
{
    REQUIRE(validator_detail::is_duration("30s") == true);
    REQUIRE(validator_detail::is_duration("5m") == true);
    REQUIRE(validator_detail::is_duration("1.5h") == true);
    REQUIRE(validator_detail::is_duration("2d") == true);
    REQUIRE(validator_detail::is_duration("90") == true);   // 裸数字 = 秒
    REQUIRE(validator_detail::is_duration("-5s") == false);
    REQUIRE(validator_detail::is_duration("10x") == false);
    REQUIRE(validator_detail::is_duration("") == false);
    REQUIRE(validator_detail::is_duration(".") == false);
}

TEST_CASE("validator: is_hostname / is_uuid / is_path / 存在性", "[validator]")
{
    REQUIRE(validator_detail::is_hostname("example.com") == true);
    REQUIRE(validator_detail::is_hostname("a-b.c99") == true);
    REQUIRE(validator_detail::is_hostname("-bad.com") == false);
    REQUIRE(validator_detail::is_hostname("a..b") == false);
    REQUIRE(validator_detail::is_hostname("") == false);
    REQUIRE(validator_detail::is_uuid("550e8400-e29b-41d4-a716-446655440000") == true);
    REQUIRE(validator_detail::is_uuid("550e8400-e29b-41d4") == false);
    REQUIRE(validator_detail::is_uuid("550e8400e29b41d4a716446655440000") == false);
    REQUIRE(validator_detail::is_path("C:\\tmp\\x.txt") == true);
    REQUIRE(validator_detail::is_path("") == false);
    REQUIRE(validator_detail::is_existing_file("no-such-file-8492.txt") == false);

    auto f = (std::filesystem::temp_directory_path() / "cmd_args_ut_validator.txt").string();
    { std::ofstream(f) << "x"; }
    REQUIRE(validator_detail::is_existing_file(f) == true);
    std::filesystem::remove(f);
    REQUIRE(validator_detail::is_existing_file(f) == false);
}
```

- [ ] **Step 2: 运行测试确认编译失败（头文件不存在）**

Run: `xmake build unit_test`
Expected: FAIL（`cannot open include file: cmd_args/base/validator.h`）。

- [ ] **Step 3: 实现纯函数库**

创建 `include/cmd_args/base/validator.h`（许可头照抄 `option.h` 1-17 行）：

```cpp
#pragma once

#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "cmd_args/base/def.h"
#include "cmd_args/base/util.h"

CMD_ARGS_NAMESPACE_BEGIN

namespace validator_detail {

/* 辅助：全部为 ASCII 数字 */
inline bool __allDigits(std::string const &_S) noexcept
{
    if (_S.empty()) return false;
    for (char c : _S) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    }
    return true;
}

/// @brief 端口：1..65535 十进制整数
inline bool is_port(std::string const &_S) noexcept
{
    if (_S.empty() || _S.size() > 5 || !__allDigits(_S)) return false;
    unsigned long v = std::strtoul(_S.c_str(), nullptr, 10);
    return v >= 1 && v <= 65535;
}

/// @brief IPv4：四段十进制 0..255（允许前导零）
inline bool is_ipv4(std::string const &_S) noexcept
{
    if (_S.empty()) return false;
    auto parts = util::split(_S, '.');
    if (parts.size() != 4) return false;
    for (auto const &p : parts) {
        if (p.empty() || p.size() > 3 || !__allDigits(p)) return false;
        if (std::strtoul(p.c_str(), nullptr, 10) > 255) return false;
    }
    return true;
}

/// @brief IPv6：8 组 hex 或 '::' 压缩（至多一次），允许内嵌 IPv4 尾段
inline bool is_ipv6(std::string const &_S) noexcept
{
    if (_S.empty()) return false;
    std::size_t i = 0, n = _S.size();
    std::size_t groups  = 0;
    bool        compressed = false;
    bool        ipv4Tail   = false;

    while (i < n) {
        if (_S[i] == ':') {
            if (i + 1 < n && _S[i + 1] == ':') {
                if (compressed) return false;   // 只允许一次压缩
                compressed = true;
                i += 2;                          // 尾部 "::" 合法（如 "1::"）
            }
            else {
                ++i;
                if (i == n) return false;        // 单冒号结尾非法
            }
            continue;
        }
        if (ipv4Tail) return false;              // IPv4 之后不再允许组

        std::size_t j = i;
        while (j < n && _S[j] != ':') ++j;
        std::string seg = _S.substr(i, j - i);
        if (seg.find('.') != std::string::npos) {
            if (j != n || !is_ipv4(seg)) return false;  // IPv4 段必须位于末尾
            ipv4Tail = true;
            groups += 2;
        }
        else {
            if (seg.empty() || seg.size() > 4) return false;
            for (char c : seg) {
                if (!std::isxdigit(static_cast<unsigned char>(c))) return false;
            }
            ++groups;
        }
        i = j;
    }

    if (compressed) return groups <= 7;   // 压缩至少补 1 组
    return groups == 8;
}

/// @brief float：完整解析且有限（拒绝溢出/NaN/Inf/尾随字符）
inline bool is_float(std::string const &_S) noexcept
{
    std::string t = util::trim(_S);
    if (t.empty()) return false;
    char *end = nullptr;
    errno = 0;
    float v = std::strtof(t.c_str(), &end);
    return end != t.c_str() && *end == '\0' && errno != ERANGE && std::isfinite(v);
}

/// @brief double：完整解析且有限
inline bool is_double(std::string const &_S) noexcept
{
    std::string t = util::trim(_S);
    if (t.empty()) return false;
    char *end = nullptr;
    errno = 0;
    double v = std::strtod(t.c_str(), &end);
    return end != t.c_str() && *end == '\0' && errno != ERANGE && std::isfinite(v);
}

/// @brief 日期：YYYY-MM-DD 且为真实日历（含闰年）
inline bool is_date(std::string const &_S) noexcept
{
    if (_S.size() != 10 || _S[4] != '-' || _S[7] != '-') return false;
    for (std::size_t i = 0; i < 10; ++i) {
        if (i == 4 || i == 7) continue;
        if (!std::isdigit(static_cast<unsigned char>(_S[i]))) return false;
    }
    int y = std::atoi(_S.substr(0, 4).c_str());
    int m = std::atoi(_S.substr(5, 2).c_str());
    int d = std::atoi(_S.substr(8, 2).c_str());
    if (m < 1 || m > 12) return false;
    static const int days[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    int md = days[m - 1];
    if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) md = 29;
    return d >= 1 && d <= md;
}

/// @brief 时长：数字[.小数] + 单位 s/m/h/d；裸数字 = 秒；非负
inline bool is_duration(std::string const &_S) noexcept
{
    std::string t = util::trim(_S);
    if (t.empty()) return false;
    std::size_t i = 0;
    bool        dot = false, hasDigit = false;
    while (i < t.size()) {
        char c = t[i];
        if (std::isdigit(static_cast<unsigned char>(c))) { hasDigit = true; ++i; continue; }
        if (c == '.' && !dot) { dot = true; ++i; continue; }
        break;
    }
    if (!hasDigit) return false;
    std::string unit = t.substr(i);
    if (unit.empty()) return true;                       // 裸数字 → 秒
    return unit == "s" || unit == "m" || unit == "h" || unit == "d";
}

/// @brief 主机名：RFC 1123 标签（1-63 字符，字母数字+连字符，首尾非连字符），总长 <= 253
inline bool is_hostname(std::string const &_S) noexcept
{
    if (_S.empty() || _S.size() > 253) return false;
    auto labels = util::split(_S, '.');
    for (auto const &l : labels) {
        if (l.empty() || l.size() > 63) return false;
        if (!std::isalnum(static_cast<unsigned char>(l.front()))
            || !std::isalnum(static_cast<unsigned char>(l.back()))) {
            return false;
        }
        for (char c : l) {
            if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-') return false;
        }
    }
    return true;
}

/// @brief UUID：8-4-4-4-12 十六进制
inline bool is_uuid(std::string const &_S) noexcept
{
    if (_S.size() != 36) return false;
    for (std::size_t i = 0; i < 36; ++i) {
        char c = _S[i];
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (c != '-') return false;
            continue;
        }
        if (!std::isxdigit(static_cast<unsigned char>(c))) return false;
    }
    return true;
}

/// @brief 路径（轻语法：非空、不含 NUL；不校验存在性）
inline bool is_path(std::string const &_S) noexcept
{
    return !_S.empty() && _S.find('\0') == std::string::npos;
}

/// @brief 存在且为常规文件
inline bool is_existing_file(std::string const &_S) noexcept
{
    std::error_code ec;
    return std::filesystem::is_regular_file(std::filesystem::path(_S), ec);
}

/// @brief 存在且为目录
inline bool is_existing_dir(std::string const &_S) noexcept
{
    std::error_code ec;
    return std::filesystem::is_directory(std::filesystem::path(_S), ec);
}

}  // namespace validator_detail

using validator_detail::is_port;
using validator_detail::is_ipv4;
using validator_detail::is_ipv6;
using validator_detail::is_float;
using validator_detail::is_double;
using validator_detail::is_date;
using validator_detail::is_duration;
using validator_detail::is_hostname;
using validator_detail::is_uuid;
using validator_detail::is_path;
using validator_detail::is_existing_file;
using validator_detail::is_existing_dir;

CMD_ARGS_NAMESPACE_END
```

- [ ] **Step 4: 运行测试确认通过**

Run: `xmake build unit_test && xmake r unit_test -- "[validator]"`
Expected: PASS（7 个用例，每个语义函数一个用例）。

- [ ] **Step 5: 提交**

```bash
git add include/cmd_args/base/validator.h dfx/unit_test/test_validation.cpp
git commit -m "feat(validator): add semantic validator functions (ipv4/ipv6/port/date/duration/...)"
```

---

### Task 2: Validator 结构与 ValidatorMixin（base/validator.h — 第二部分）

**Files:**
- Modify: `include/cmd_args/base/validator.h`（追加到 `namespace validator_detail` 内、`using` 列表之前）
- Test: `dfx/unit_test/test_validation.cpp`（追加用例）

**Interfaces:**
- Consumes: T1 全部纯函数；`cmd_args/error/builder_error.h` 的 `ErrorArgument` / `BuildError_E::UNSUPPORT_TYPE`（未知语义名抛构筑错误）。
- Produces: `struct Validator`、`template <typename Self_T> class ValidatorMixin`、工厂函数 `between / one_of / matches / custom / semantic_validator`（T3-T5 挂载与测试全部依赖；`semantic_validator` 未知名**抛** `ErrorArgument(UNSUPPORT_TYPE)`）。

- [ ] **Step 1: 追加失败测试**

在 `dfx/unit_test/test_validation.cpp` 末尾追加（注意匿名 `Dummy` 类型避开多个 TU 冲突）：

```cpp
namespace {
/* 本地测试类型：只需继承 mixin 即可驱动链式 API */
struct Dummy : validator_detail::ValidatorMixin<Dummy> {};
}

TEST_CASE("validator: Validator 结构与工厂函数", "[validator]")
{
    REQUIRE(validator_detail::between(1.0, 10.0).validate("5") == "");
    REQUIRE(validator_detail::between(1.0, 10.0).validate("11") != "");       // 越上界
    REQUIRE(validator_detail::between(1.0, 10.0, false, false).validate("1") != "");   // 开下端
    REQUIRE(validator_detail::between(1.0, 10.0, false, false).validate("10") != "");  // 开上端
    REQUIRE(validator_detail::between(1.0, 10.0, false, false).validate("2") == "");

    REQUIRE(validator_detail::one_of({ "fast", "slow" }).validate("fast") == "");
    REQUIRE(validator_detail::one_of({ "fast", "slow" }).validate("mid") != "");

    REQUIRE(validator_detail::matches(R"(^[\w.]+@[\w.]+$)").validate("a@b.c") == "");
    REQUIRE(validator_detail::matches(R"(^[\w.]+@[\w.]+$)").validate("nope") != "");

    REQUIRE(validator_detail::semantic_validator("ipv4").validate("127.0.0.1") == "");
    REQUIRE(validator_detail::semantic_validator("ipv4").validate("999.0.0.1") != "");
    REQUIRE(validator_detail::semantic_validator("port").validate("8080") == "");

    // 未知语义名 = 用户构筑错误
    REQUIRE_THROWS_AS(validator_detail::semantic_validator("nope"), ErrorArgument);
}

TEST_CASE("validator: ValidatorMixin 链式 API 与 validate 语义", "[validator]")
{
    Dummy d;
    d.required();
    REQUIRE(d.isRequired() == true);
    d.required(false);
    REQUIRE(d.isRequired() == false);

    d.range(1024, 65535);
    d.semantic("port");
    d.oneOf({ "8888", "9999" });
    REQUIRE(d.hasValidators() == true);
    REQUIRE(d.validators().size() == 3);
    REQUIRE(d.validate("8888") == "");                     // range ✓ port ✓ oneOf ✓
    REQUIRE(d.validate("80") != "");                       // range 失败在前
    REQUIRE(d.validate("2048") != "");                     // oneOf 失败

    // 语义重复绑定 = 替换而非追加
    d.semantic("port");
    REQUIRE(d.validators().size() == 3);

    // custom 校验器
    Dummy d2;
    d2.check(validator_detail::custom(
        [](std::string const &v) { return v.size() == 2; }, "len2"));
    REQUIRE(d2.validate("ab") == "");
    REQUIRE(d2.validate("abc") != "");
}
```

- [ ] **Step 2: 运行测试确认编译失败（`Validator` 未定义）**

Run: `xmake build unit_test`
Expected: FAIL（`ValidatorMixin` 不是模板名 / `Validator` 未声明）。

- [ ] **Step 3: 实现 Validator / 工厂 / mixin**

把下面代码**追加**进 `include/cmd_args/base/validator.h`：先加 `#include <functional> <initializer_list> <regex> <utility> <vector>` 与 `#include "cmd_args/error/builder_error.h"`（放在既有 include 之后），再在 `namespace validator_detail` 的 `is_existing_dir` 之后、`} // namespace validator_detail` 之前插入：

```cpp
/// @brief 校验器条目：名称 + 校验函数 + 失败消息模板（%value% 替换为实际值）
/// @note fnCheck 为空表示恒通过（占位校验器）
struct Validator
{
    using FnCheck_T = std::function<bool(std::string const &)>;

    std::string strName;     ///< 校验器名称（schema 展示 / 语义去重用）
    FnCheck_T   fnCheck;     ///< 校验函数（空 = 恒通过）
    std::string strFailMsg;  ///< 失败消息模板（%value% 占位符）

    Validator() = default;
    Validator(std::string _Name, FnCheck_T _Fn, std::string _FailMsg = {})
        : strName(std::move(_Name))
        , fnCheck(std::move(_Fn))
        , strFailMsg(std::move(_FailMsg))
    {}

    /// @brief 校验值：通过返回空串，失败返回替换后的消息
    auto validate(std::string const &_Value) const -> std::string
    {
        if (!fnCheck) return {};
        if (fnCheck(_Value)) return {};
        if (!strFailMsg.empty()) return util::replace(strFailMsg, "%value%", _Value);
        std::string msg = "invalid value '" + _Value + "'";
        if (!strName.empty()) msg += " (" + strName + ")";
        return msg;
    }
};

/// @brief 数值区间校验（可配开闭；闭区间默认）：
///        先经 is_double 语义，再比较区间
inline Validator between(double _Min, double _Max,
                         bool _MinInclusive = true, bool _MaxInclusive = true)
{
    std::string name = "range (" + std::to_string(_Min)
        + (_MinInclusive ? " <= " : " < ") + "x"
        + (_MaxInclusive ? " <= " : " < ") + std::to_string(_Max) + ")";
    return Validator(
        name,
        [_Min, _Max, _MinInclusive, _MaxInclusive](std::string const &_S) {
            if (!is_double(_S)) return false;
            double v = std::strtod(_S.c_str(), nullptr);
            if (_MinInclusive ? v < _Min : v <= _Min) return false;
            if (_MaxInclusive ? v > _Max : v >= _Max) return false;
            return true;
        },
        "%value% out of range");
}

/// @brief 枚举集合校验：值必须命中 _Values 之一
inline Validator one_of(std::initializer_list<std::string> _Values)
{
    std::vector<std::string> vec(_Values);
    std::string name = "one of {";
    for (auto const &v : vec) name += v + " ";
    name += "}";
    return Validator(
        name,
        [vec](std::string const &_S) {
            for (auto const &v : vec) { if (v == _S) return true; }
            return false;
        },
        "%value% not allowed");
}

/// @brief 正则匹配校验（std::regex 完整匹配；非法正则视为校验失败）
inline Validator matches(std::string const &_Pattern)
{
    return Validator(
        "regex " + _Pattern,
        [_Pattern](std::string const &_S) {
            try { return std::regex_match(_S, std::regex(_Pattern)); }
            catch (...) { return false; }
        },
        "%value% does not match regex");
}

/// @brief 自定义校验器
inline Validator custom(Validator::FnCheck_T _Fn, std::string _Name = "custom",
                        std::string _FailMsg = "%value% rejected")
{
    return Validator(std::move(_Name), std::move(_Fn), std::move(_FailMsg));
}

/// @brief 语义校验表：语义名 → 校验器（T1 纯函数的登记处）
using SemanticTable_T = std::vector<std::pair<std::string, Validator>>;

inline SemanticTable_T const &semanticTable()
{
    static SemanticTable_T const table = {
        { "ipv4",     Validator("ipv4", is_ipv4, "%value% is not a valid IPv4 address") },
        { "ipv6",     Validator("ipv6", is_ipv6, "%value% is not a valid IPv6 address") },
        { "port",     Validator("port", is_port, "%value% is not a valid port (1-65535)") },
        { "file",     Validator("file", is_existing_file, "%value% is not an existing file") },
        { "path",     Validator("path", is_path, "%value% is not a valid path") },
        { "date",     Validator("date", is_date, "%value% is not a valid date (YYYY-MM-DD)") },
        { "duration", Validator("duration", is_duration, "%value% is not a valid duration (30s / 5m / 1h)") },
        { "hostname", Validator("hostname", is_hostname, "%value% is not a valid hostname") },
        { "uuid",     Validator("uuid", is_uuid, "%value% is not a valid UUID") },
        { "float",    Validator("float", is_float, "%value% is not a valid float") },
        { "double",   Validator("double", is_double, "%value% is not a valid double") },
    };
    return table;
}

/// @brief 按语义名取校验器；未知语义名抛 ErrorArgument（构筑期错误）
inline Validator semantic_validator(std::string const &_Name)
{
    for (auto const &kv : semanticTable()) {
        if (kv.first == _Name) return kv.second;
    }
    throw ErrorArgument(BuildError_E::UNSUPPORT_TYPE);
}
```

然后在 `namespace validator_detail` 外、`using` 列表内加：

```cpp
/// @brief 校验链 mixin：为 Argument / OptionValue / EnvSpec 注入统一的链式校验 API
/// @tparam Self_T CRTP 派生类型（必须可移动构造）
template <typename Self_T>
class ValidatorMixin
{
public:
    using SelfMixin_T = Self_T;
    using FnCheck_T   = std::function<bool(std::string const &)>;

    /** @brief 标记必选（左值链式）
     *  @return Self_T& 支持链式调用 */
    auto required() & -> Self_T &
    {
        bRequired__ = true;
        return static_cast<Self_T &>(*this);
    }

    /// @brief 标记必选（右值链式）
    auto required() && -> Self_T
    {
        bRequired__ = true;
        return std::move(static_cast<Self_T &>(*this));
    }

    /// @brief 动态设置必选与否（required(false) 解除必选）
    auto required(bool _On) & -> Self_T &
    {
        bRequired__ = _On;
        return static_cast<Self_T &>(*this);
    }

    /// @brief 动态设置必选与否（右值链式）
    auto required(bool _On) && -> Self_T
    {
        bRequired__ = _On;
        return std::move(static_cast<Self_T &>(*this));
    }

    /// @brief 追加校验器（左值链式）
    auto check(Validator const &_V) & -> Self_T &
    {
        vecValidators__.push_back(_V);
        return static_cast<Self_T &>(*this);
    }

    /// @brief 追加校验器（右值链式）
    auto check(Validator const &_V) && -> Self_T
    {
        vecValidators__.push_back(_V);
        return std::move(static_cast<Self_T &>(*this));
    }

    /// @brief 追加自定义校验器（函数式简写）
    auto check(FnCheck_T _Fn, std::string _Name = "custom") & -> Self_T &
    {
        vecValidators__.push_back(validator_detail::custom(std::move(_Fn), std::move(_Name)));
        return static_cast<Self_T &>(*this);
    }

    /// @brief 追加自定义校验器（右值链式）
    auto check(FnCheck_T _Fn, std::string _Name = "custom") && -> Self_T
    {
        vecValidators__.push_back(validator_detail::custom(std::move(_Fn), std::move(_Name)));
        return std::move(static_cast<Self_T &>(*this));
    }

    /// @brief 数值区间校验（闭区间默认，可配开闭）
    auto range(double _Min, double _Max,
               bool _MinInclusive = true, bool _MaxInclusive = true) & -> Self_T &
    {
        vecValidators__.push_back(validator_detail::between(_Min, _Max, _MinInclusive, _MaxInclusive));
        return static_cast<Self_T &>(*this);
    }

    /// @brief 数值区间校验（右值链式）
    auto range(double _Min, double _Max,
               bool _MinInclusive = true, bool _MaxInclusive = true) && -> Self_T
    {
        vecValidators__.push_back(validator_detail::between(_Min, _Max, _MinInclusive, _MaxInclusive));
        return std::move(static_cast<Self_T &>(*this));
    }

    /// @brief 枚举集合校验
    auto oneOf(std::initializer_list<std::string> _Values) & -> Self_T &
    {
        vecValidators__.push_back(validator_detail::one_of(_Values));
        return static_cast<Self_T &>(*this);
    }

    /// @brief 枚举集合校验（右值链式）
    auto oneOf(std::initializer_list<std::string> _Values) && -> Self_T
    {
        vecValidators__.push_back(validator_detail::one_of(_Values));
        return std::move(static_cast<Self_T &>(*this));
    }

    /// @brief 正则匹配校验
    auto match(std::string const &_Pattern) & -> Self_T &
    {
        vecValidators__.push_back(validator_detail::matches(_Pattern));
        return static_cast<Self_T &>(*this);
    }

    /// @brief 正则匹配校验（右值链式）
    auto match(std::string const &_Pattern) && -> Self_T
    {
        vecValidators__.push_back(validator_detail::matches(_Pattern));
        return std::move(static_cast<Self_T &>(*this));
    }

    /** @brief 语义类型校验（ipv4 / ipv6 / port / file / path / date / duration /
     *         hostname / uuid / float / double；同名替换、未知抛构筑错误）
     *  @note 语义名表见 validator_detail::semanticTable() */
    auto semantic(std::string const &_Name) & -> Self_T &
    {
        __applySemantic(_Name);
        return static_cast<Self_T &>(*this);
    }

    /// @brief 语义类型校验（右值链式）
    auto semantic(std::string const &_Name) && -> Self_T
    {
        __applySemantic(_Name);
        return std::move(static_cast<Self_T &>(*this));
    }

    /// @brief 是否标记为必选
    bool isRequired() const noexcept { return bRequired__; }

    /// @brief 是否已配置校验器
    bool hasValidators() const noexcept { return !vecValidators__.empty(); }

    /// @brief 校验器列表（只读，schema 展示用）
    auto validators() const noexcept -> std::vector<Validator> const & { return vecValidators__; }

    /** @brief 依次运行全部校验器，返回第一条失败消息（空串 = 全部通过） */
    auto validate(std::string const &_Value) const -> std::string
    {
        for (auto const &v : vecValidators__) {
            auto msg = v.validate(_Value);
            if (!msg.empty()) return msg;
        }
        return {};
    }

private:
    /// @brief 语义校验器绑定：同名替换，否则追加
    void __applySemantic(std::string const &_Name)
    {
        auto v = validator_detail::semantic_validator(_Name);
        for (auto &cur : vecValidators__) {
            if (cur.strName == v.strName) { cur = v; return; }
        }
        vecValidators__.push_back(std::move(v));
    }

protected:
    /** 必选标记（通过 isRequired() 读取） */
    bool                     bRequired__ = false;
    /** 校验器链 */
    std::vector<Validator>   vecValidators__;
};
```

并在 `using` 区追加导出：`using validator_detail::Validator;`、`using validator_detail::ValidatorMixin;`、`using validator_detail::between; using validator_detail::one_of; using validator_detail::matches; using validator_detail::custom; using validator_detail::semantic_validator;`。

注意：`ValidatorMixin` 使用 `validator_detail::` 前缀形式引用工厂——它本身定义在 `namespace cmd_args` 下（`using` 区之后），因此类内引用写 `validator_detail::between(...)`（见上面代码，已如此）。

- [ ] **Step 4: 运行测试确认通过**

Run: `xmake build unit_test && xmake r unit_test -- "[validator]"`
Expected: PASS（T1 7 个 + T2 2 个用例）。

- [ ] **Step 5: 提交**

```bash
git add include/cmd_args/base/validator.h dfx/unit_test/test_validation.cpp
git commit -m "feat(validator): add Validator + ValidatorMixin chain API and semantic table"
```

---

### Task 3: OptionValue 挂载（required / bEnvHit__ / 默认值判断）

**Files:**
- Modify: `include/cmd_args/option/option.h`
- Test: `dfx/unit_test/test_validation.cpp`（追加用例）

**Interfaces:**
- Consumes: `ValidatorMixin<OptionValue>`（T2）。
- Produces: `OptionValue` 新增 `mutable bool bEnvHit__`、`bool isSatisfied() const noexcept`、`bool hasDefault() const noexcept`、`auto defaultValue() const noexcept -> Value_T const&`、`bool usesDefault() const noexcept`；`reset()` 清理 `bEnvHit__`。T7 校验引擎与 Compiler/Interpreter seed 依赖这些。

- [ ] **Step 1: 追加失败测试**

```cpp
TEST_CASE("option: required 命中判定（显式/环境兜底/默认值）与 usesDefault", "[required][option]")
{
    Context app("ut_req_opt");
    app.addOption<std::string>("host", "host", "");
    app.option("host")->required();
    app.option("host")->env("UT_REQ_HOST");
    app.addOption<std::string>("port", "port", "8080");

    // 注意：本用例在 T7 之后仍会随全量回归运行，因此每次 parse 都必须满足 host 的
    // required（T7 起校验引擎会把它变成真正错误）。port 不 required，仅测默认值口径。
    std::error_code ec;
    app.parse(ec, Tokens_T{}, Envs_T{ "UT_REQ_HOST=env-h" });   // host 由 env 兜底命中
    REQUIRE(!ec);
    REQUIRE(app.option("host")->isSatisfied() == true);          // env 兜底命中
    REQUIRE(app.option("host")->bEnvHit__ == true);
    REQUIRE(app.option("port")->isSatisfied() == false);         // 默认值不算命中
    REQUIRE(app.option("port")->hasDefault() == true);
    REQUIRE(app.option("port")->usesDefault() == true);

    app.reset();
    app.parse(ec, Tokens_T{ "--host", "h1" }, Envs_T{});         // 显式命中
    REQUIRE(!ec);
    REQUIRE(app.option("host")->isSatisfied() == true);
    REQUIRE(app.option("host")->usesDefault() == false);

    app.reset();
    app.parse(ec, Tokens_T{}, Envs_T{ "UT_REQ_HOST=env-h" });    // env 兜底命中（再次）
    REQUIRE(!ec);
    REQUIRE(app.option("host")->isSatisfied() == true);
    REQUIRE(app.option("host")->usesDefault() == false);

    app.reset();
    REQUIRE(app.option("host")->isSatisfied() == false);         // reset 清 bEnvHit__/bExplicit__
}

TEST_CASE("option: 校验器挂载（range/oneOf/match/semantic）", "[validator][option]")
{
    Context app("ut_val_opt");
    app.addOption<std::string>("mode", "mode", "");
    app.option("mode")->oneOf({ "fast", "slow" });
    app.addOption<int>("port", "port", 0);
    app.option("port")->range(1024, 65535);
    app.addOption<std::string>("host", "host", "");
    app.option("host")->semantic("ipv4");
    app.addOption<std::string>("mail", "mail", "");
    app.option("mail")->match(R"(^[\w.]+@[\w.]+$)");

    REQUIRE(app.option("mode")->validate("fast") == "");
    REQUIRE(app.option("mode")->validate("mid") != "");
    REQUIRE(app.option("port")->validate("8080") == "");
    REQUIRE(app.option("port")->validate("80") != "");
    REQUIRE(app.option("host")->validate("192.168.0.1") == "");
    REQUIRE(app.option("host")->validate("192.168.0.999") != "");
    REQUIRE(app.option("mail")->validate("a@b.c") == "");
    REQUIRE(app.option("mail")->validate("nope") != "");
    REQUIRE(app.option("mode")->isRequired() == false);
}
```

- [ ] **Step 2: 运行测试确认编译失败（`isSatisfied` 等未定义）**

Run: `xmake build unit_test`
Expected: FAIL（`OptionValue` 无 `isSatisfied` / `hasDefault` / `usesDefault`）。

- [ ] **Step 3: 实现**

修改 `option.h`：

3.1 include 区（`#include "cmd_args/expression/expression.h"` 之后）加：

```cpp
#include "cmd_args/base/validator.h"
```

3.2 `struct OptionValue : Option` 改为 `struct OptionValue : Option, public ValidatorMixin<OptionValue>`（struct 默认公有继承，显式 `public` 无碍）。

3.3 「运行时状态」区（`bool bBuiltin__` 之后）加：

```cpp
    mutable bool bEnvHit__ = false;   ///< 环境变量兜底命中标记（seed 时置位；reset 清除）
```

3.4 `reset()` 里（清除 `bExternal__` 之后）加一行：

```cpp
        bEnvHit__   = false;
```

3.5 在 `reset()` 之后、结构体结尾 `};` 之前，追加查询方法：

```cpp
    /** @brief 新值是否满足必选（显式命中 / 外部赋值 / env 兜底命中 / 值已偏离默认值）
     *  @note 默认值本身不算命中；空值视为未提供 */
    bool isSatisfied() const noexcept
    {
        return bExplicit__ || bExternal__ || bEnvHit__
               || (!strValue.empty() && strValue != strDefault__);
    }

    /// @brief 是否声明了非空默认值
    bool hasDefault() const noexcept { return !strDefault__.empty(); }

    /// @brief 内置默认值（只读）
    auto defaultValue() const noexcept -> Value_T const & { return strDefault__; }

    /** @brief 当前值是否来自默认值（未被 CLI / 配置 / 环境变量覆盖）
     *  @note 判定依据：三类输入源均未命中 */
    bool usesDefault() const noexcept { return !bExplicit__ && !bExternal__ && !bEnvHit__; }
```

- [ ] **Step 4: 运行测试确认通过**

Run: `xmake build unit_test && xmake r unit_test -- "[option]"`
Expected: PASS（新增 2 个用例 + 既有 option 用例无回归）。

- [ ] **Step 5: 提交**

```bash
git add include/cmd_args/option/option.h dfx/unit_test/test_validation.cpp
git commit -m "feat(option): OptionValue mounts required/env-hit tracking and default judgment"
```

---

### Task 4: Argument 挂载（required / bEnvHit__ / 默认值判断）

**Files:**
- Modify: `include/cmd_args/argument/argument.h`
- Test: `dfx/unit_test/test_validation.cpp`（追加用例）

**Interfaces:**
- Consumes: `ValidatorMixin<Argument>`（T2）。
- Produces: `Argument` 新增 private `mutable bool bEnvHit__`、`markEnvHit() / clearEnvHit() / isEnvHit()`（供 Compiler/Interpreter seed 钩子与测试使用）、`bool isSatisfied() const noexcept`、`bool hasDefault() const noexcept`、`auto defaultValue() const noexcept -> Value_T const&`、`bool usesDefault() const noexcept`；`reset()` 清理 `bEnvHit__`。注意 `Argument` 是 `class`（默认 private 继承），mixin 必须显式 `public`。

- [ ] **Step 1: 追加失败测试**

```cpp
TEST_CASE("argument: required 命中判定与默认值语义（命名+方位+env 兜底）", "[required][argument]")
{
    Context app("ut_req_arg");
    app.build<ArgumentNamed>("name", "n");
    app.argument("name")->required();
    app.argument("name")->env("UT_REQ_NAME");
    app.build<ArgumentPosition>("dest", "d");
    app.argument("dest")->required();

    // 同样：T7 之后全量回归下，每次 parse 都必须满足全部 required。
    std::error_code ec;
    app.parse(ec, Tokens_T{ "--name", "x", "out" }, Envs_T{});
    REQUIRE(!ec);
    REQUIRE(app.argument("name")->isSatisfied() == true);
    REQUIRE(app.argument("dest")->isSatisfied() == true);
    REQUIRE(app.argument("name")->usesDefault() == false);

    app.reset();
    app.parse(ec, Tokens_T{ "out" }, Envs_T{ "UT_REQ_NAME=from-env" });  // name 走 env 兜底
    REQUIRE(!ec);
    REQUIRE(app.argument("name")->isSatisfied() == true);
    REQUIRE(app.argument("name")->isEnvHit() == true);
    REQUIRE(app.argument("dest")->isSatisfied() == true);

    app.reset();
    app.parse(ec, Tokens_T{ "--name", "x", "out" }, Envs_T{});
    REQUIRE(!ec);
    REQUIRE(app.argument("name")->isSatisfied() == true);

    app.reset();
    REQUIRE(app.argument("name")->isEnvHit() == false);       // reset 清标记
    REQUIRE(app.argument("name")->isSatisfied() == false);
}

TEST_CASE("argument: 校验器挂载（check/semantic）", "[validator][argument]")
{
    Context app("ut_val_arg");
    app.build<ArgumentNamed>("port", "p");
    app.argument("port")->range(1024, 65535);
    app.build<ArgumentNamed>("host", "h");
    app.argument("host")->semantic("ipv4");
    app.build<ArgumentNamed>("len", "l");
    app.argument("len")->check([](std::string const &v) { return v.size() <= 3; }, "len3");

    REQUIRE(app.argument("port")->validate("8080") == "");
    REQUIRE(app.argument("port")->validate("80") != "");
    REQUIRE(app.argument("host")->validate("10.0.0.1") == "");
    REQUIRE(app.argument("host")->validate("10.0.0.999") != "");
    REQUIRE(app.argument("len")->validate("abc") == "");
    REQUIRE(app.argument("len")->validate("abcd") != "");
}
```

- [ ] **Step 2: 运行测试确认编译失败**

Run: `xmake build unit_test`
Expected: FAIL（`Argument` 无 `isSatisfied` / `markEnvHit` 等）。

- [ ] **Step 3: 实现**

修改 `argument.h`：

3.1 include 区（`#include "cmd_args/expression/expression.h"` 之后）加：

```cpp
#include "cmd_args/base/validator.h"
```

3.2 类声明 `class Argument : public Expression` 改为 `class Argument : public Expression, public ValidatorMixin<Argument>`。

3.3 私有运行时状态区（`bool bExternal__` 之后）加：

```cpp
    /** 环境变量兜底命中标记（seed 时经 markEnvHit 置位；reset 清除） */
    mutable bool bEnvHit__ = false;
```

3.4 `reset()` 的 `bExternal__ = false;` 之后加：

```cpp
        bEnvHit__ = false;
```

3.5 在 `getValue()`（public 区末尾、`private:` 之前）加：

```cpp
    /** @brief 新值是否满足必选（外部赋值 / env 兜底命中 / 持有非空值）
     *  @note 空值视为未提供；默认值非空时 hasValue 恒真（声明 required + 非空默认是自相矛盾的用法） */
    bool isSatisfied() const noexcept { return bExternal__ || bEnvHit__ || hasValue(); }

    /// @brief 是否声明了非空默认值
    bool hasDefault() const noexcept { return !strDefault__.empty(); }

    /// @brief 内置默认值（只读）
    auto defaultValue() const noexcept -> Value_T const & { return strDefault__; }

    /** @brief 当前值是否来自默认值（无外部/env 命中且值等于内置默认）
     *  @note 与 OptionValue 的 usesDefault 语义对齐（本类以值比较近似） */
    bool usesDefault() const noexcept
    {
        return !bExternal__ && !bEnvHit__ && strValue__ == strDefault__;
    }

    /** @brief 标记环境变量兜底命中（seed 钩子写入） */
    auto markEnvHit() noexcept -> Self_T & { bEnvHit__ = true; return *this; }
    /** @brief 清除环境变量兜底命中标记（reset 内部使用；也供测试直接调用） */
    auto clearEnvHit() noexcept -> Self_T & { bEnvHit__ = false; return *this; }
    /// @brief 环境变量兜底是否命中过
    bool isEnvHit() const noexcept { return bEnvHit__; }
```

- [ ] **Step 4: 运行测试确认通过**

Run: `xmake build unit_test && xmake r unit_test -- "[argument]"`
Expected: PASS（新增 2 个用例 + 既有用例无回归）。

- [ ] **Step 5: 提交**

```bash
git add include/cmd_args/argument/argument.h dfx/unit_test/test_validation.cpp
git commit -m "feat(argument): Argument mounts required/env-hit tracking and default judgment"
```

---

### Task 5: 声明式环境条目（EnvSpec + requireEnv）

**Files:**
- Modify: `include/cmd_args/env/env.h`、`include/cmd_args/env/env_data.h`、`include/cmd_args/env/env_builder.h`、`include/cmd_args/context/context_base.h`
- Test: `dfx/unit_test/test_validation.cpp`（追加用例）

**Interfaces:**
- Consumes: `ValidatorMixin<EnvSpec>`（T2）。
- Produces: `struct EnvSpec`（env.h，public 继承 `ValidatorMixin<EnvSpec>`；成员 `strKey / strHelp / strDefault__`；方法 `defaultEnv(std::string)(&/&&)`）；`EnvData::pushSpec(move)/mutSpec(Key)->EnvSpec*/forEachSpec(fn)`；`EnvBuilder::requireEnv(Key, Help = {}) -> EnvSpec*`（已有 spec 返回既有句柄，不覆盖）、`EnvBuilder::envSpec(Key) -> EnvSpec*`；`ContextBase::requireEnv(Name_T const&, Desc_T = {}) -> EnvSpec*`、`ContextBase::envSpec(Name_T const&) noexcept -> EnvSpec*`。T7 校验引擎消费 `EnvData::forEachSpec` 与 `mutSpec`。

- [ ] **Step 1: 追加失败测试**

```cpp
TEST_CASE("env: requireEnv 声明式条目与句柄稳定性（重复注册不覆盖）", "[required][env]")
{
    Context app("ut_env_spec");
    auto *p = app.requireEnv("DB_HOST", "database host");
    REQUIRE(p != nullptr);
    REQUIRE(p->isRequired() == true);
    p->semantic("hostname");
    p->defaultEnv("localhost");

    auto *q = app.requireEnv("DB_PORT", "database port");
    q->range(1, 65535);

    auto *r = app.requireEnv("DB_HOST", "another help");   // 重复注册：返回既有句柄
    REQUIRE(r == p);
    REQUIRE(r->isRequired() == true);
    REQUIRE(r->strHelp == "database host");                // 未被覆盖

    REQUIRE(app.envSpec("DB_HOST") == p);
    REQUIRE(app.envSpec("NOPE") == nullptr);

    // 校验器与缺省值经句柄生效（`isEnvHit` 之类是 Argument 的方法，此处用 mixin 查询）
    REQUIRE(p->hasValidators() == true);
    REQUIRE(p->validators().size() == 1);
    REQUIRE(p->validate("localhost") == "");
    REQUIRE(p->validate("bad host!") != "");
    REQUIRE(p->strDefault__ == "localhost");
}
```

（注：`strDefault__` 是 EnvSpec 公开成员，可直读；测试第 3 行 `p->validate("bad host!")` 的失败消息来自 hostname 语义校验器。）

- [ ] **Step 2: 运行测试确认编译失败（`requireEnv` 未定义）**

Run: `xmake build unit_test`
Expected: FAIL（`ContextBase` 无 `requireEnv` / `envSpec`）。

- [ ] **Step 3: 实现**

3.1 `env.h`：include 区（`#include "cmd_args/expression/expression.h"` 之后）加 `#include "cmd_args/base/validator.h"`；在 `struct environment` 之后、`using envs_t` 之前插入：

```cpp
/**
 * @struct EnvSpec
 * @brief 声明式环境条目：程序对某个环境变量的"期望"（必选 / 校验器 / 缺省值）。
 *
 * 与 environment（运行时条目）不同：EnvSpec 是构筑期声明，解析完成后由校验引擎统一处理——
 * - isRequired()：缺失（进程环境与会话均无或为空）报 MISSING_ENV
 * - validate()  ：存在时依次校验
 * - defaultEnv()：缺失时以该值补全会话（标记新建，env 导出可见）
 */
struct EnvSpec : public ValidatorMixin<EnvSpec>
{
    using Self_T = EnvSpec;

    std::string strKey;      ///< 环境变量键
    std::string strHelp;     ///< 帮助说明
    std::string strDefault__; ///< 缺省补全值（非空时缺失即补全会话，标 new）

    EnvSpec() = default;

    EnvSpec(std::string _Key, std::string _Help = {})
        : strKey(std::move(_Key))
        , strHelp(std::move(_Help))
    {}

    /// @brief 设置缺省补全值（左值链式）
    auto defaultEnv(std::string _Value) & -> Self_T &
    {
        strDefault__ = std::move(_Value);
        return *this;
    }

    /// @brief 设置缺省补全值（右值链式）
    auto defaultEnv(std::string _Value) && -> Self_T
    {
        strDefault__ = std::move(_Value);
        return std::move(*this);
    }
};
```

3.2 `env_data.h`：`#include "cmd_args/env/env.h"` 已存在；`EnvData` 的 `doErase` 之后增加：

```cpp
    /* ---------- 声明式条目（EnvSpec） ---------- */

    /// @brief 注册声明式环境条目（键已存在则保持原条目）
    void pushSpec(EnvSpec const &_Spec) { mapSpecs__.emplace(_Spec.strKey, _Spec); }

    /// @brief 注册声明式环境条目（移动形态）
    void pushSpec(EnvSpec &&_Spec) { mapSpecs__.emplace(_Spec.strKey, std::move(_Spec)); }

    /// @brief 查询声明式环境条目（可变，未找到返回 nullptr；map 节点稳定不失效）
    auto mutSpec(Key_T const &_Key) noexcept -> EnvSpec *
    {
        auto it = mapSpecs__.find(_Key);
        if (it == mapSpecs__.end()) return nullptr;
        return std::addressof(it->second);
    }

    /// @brief 只读遍历声明式条目（校验引擎用）
    void forEachSpec(std::function<void(EnvSpec const &)> const &_Fn) const
    {
        for (auto const &kv : mapSpecs__) { _Fn(kv.second); }
    }
```

并在 private 区加：

```cpp
    using SpecsMap_T = std::map<Key_T, EnvSpec>;  ///< 声明式条目容器（按键排序去重）
    SpecsMap_T mapSpecs__;                         ///< 声明式环境条目（EnvSpec）
```

（`<map>`、`<functional>` 已 include。）

3.3 `env_builder.h`：`unsetEnv` 之后加：

```cpp
    /** @brief 注册必选环境变量（声明式条目；缺失报 MISSING_ENV）
     *  @param _Key 环境变量键
     *  @param _Help 帮助说明
     *  @return EnvSpec* 命中句柄（可再链式配置校验器/缺省值；
     *          重复注册返回既有句柄，不覆盖）
     */
    auto requireEnv(EnvData::Key_T const &_Key, EnvData::Value_T const &_Help = {}) -> EnvSpec *
    {
        if (auto *p = pData__->mutSpec(_Key)) return p;
        EnvSpec sp(_Key, _Help);
        sp.required();
        pData__->pushSpec(std::move(sp));
        return pData__->mutSpec(_Key);
    }

    /// @brief 查询声明式环境条目（未注册返回 nullptr）
    auto envSpec(EnvData::Key_T const &_Key) noexcept -> EnvSpec *
    {
        return pData__->mutSpec(_Key);
    }
```

3.4 `context_base.h`：`set_env` / `unset_env` 附近（`/* ---------- 环境变量交互 ---------- */` 区）加转发：

```cpp
    /** @brief 注册必需环境变量（声明式条目；缺失报 MISSING_ENV）
     *  @return EnvSpec* 命中句柄（链式 .semantic()/.range()/.defaultEnv() 等） */
    auto requireEnv(Name_T const &_Key, Desc_T _Help = {}) -> EnvSpec *
    {
        return EnvBuilder(&instData_.envs).requireEnv(_Key, std::move(_Help));
    }

    /// @brief 查询声明式环境条目句柄（未注册返回 nullptr）
    auto envSpec(Name_T const &_Key) noexcept -> EnvSpec *
    {
        return EnvBuilder(&instData_.envs).envSpec(_Key);
    }
```

注意：`Name_T` / `Desc_T` 在本头文件已作为全局别名使用（见 `EnvironmentSpec` 上下文中的 `NameLong_T` 用例），若编译器报未定义，改为 `std::string`。

- [ ] **Step 4: 运行测试确认通过**

Run: `xmake build unit_test && xmake r unit_test -- "[env]"`
Expected: PASS（新增 1 个用例 + 既有 env 用例无回归）。

- [ ] **Step 5: 提交**

```bash
git add include/cmd_args/env/env.h include/cmd_args/env/env_data.h include/cmd_args/env/env_builder.h include/cmd_args/context/context_base.h dfx/unit_test/test_validation.cpp
git commit -m "feat(env): add declarative EnvSpec entries with requireEnv/envSpec handles"
```

---

### Task 6: ValidationError 错误域（error/validation_error.h）

**Files:**
- Create: `include/cmd_args/error/validation_error.h`
- Modify: `include/cmd_args/context/context_base.h`（parse 双通道补 catch ×4）
- Test: `dfx/unit_test/test_validation.cpp`（追加用例）

**Interfaces:**
- Consumes: 无（独立错误域；模式照抄 `error/parser_error.h`）。
- Produces: `enum class ValidationError_E { OK = 0, MISSING_ARGUMENT, MISSING_OPTION, MISSING_ENV, VALIDATION_FAILED }`、`make_error_code` / `make_error_condition` / `std::is_error_code_enum` 特化、`class ErrorValidation : public std::logic_error`（构造 `ErrorValidation(ValidationError_E, std::string&&)`，`code() / what()`）。T7 校验引擎与 Compiler/Interpreter/ContextBase 全部依赖。

- [ ] **Step 1: 追加失败测试**

```cpp
TEST_CASE("validation: ValidationError 错误域注册与异常形态", "[validation]")
{
    auto ec = make_error_code(ValidationError_E::MISSING_OPTION);
    REQUIRE(ec.value() == static_cast<int>(ValidationError_E::MISSING_OPTION));
    REQUIRE(ec.message() == "missing required option");

    REQUIRE(make_error_code(ValidationError_E::OK).value() == 0);
    REQUIRE(make_error_code(ValidationError_E::MISSING_ARGUMENT).message() == "missing required argument");
    REQUIRE(make_error_code(ValidationError_E::MISSING_ENV).message() == "missing required environment");
    REQUIRE(make_error_code(ValidationError_E::VALIDATION_FAILED).message() == "value validation failed");
}

TEST_CASE("validation: ErrorValidation 携带描述且 what() 可读", "[validation]")
{
    ErrorValidation ev(ValidationError_E::MISSING_ARGUMENT, std::string("argument 'name'"));
    REQUIRE(ev.code() == make_error_code(ValidationError_E::MISSING_ARGUMENT));
    REQUIRE(std::string(ev.what()) == "argument 'name'");

    ErrorValidation ev2(ValidationError_E::VALIDATION_FAILED, std::string("value out of range"));
    REQUIRE(std::string(ev2.what()) == "value out of range");
}
```

- [ ] **Step 2: 运行测试确认编译失败**

Run: `xmake build unit_test`
Expected: FAIL（`ValidationError_E` 未声明）。

- [ ] **Step 3: 实现**

3.1 创建 `include/cmd_args/error/validation_error.h`（许可头照抄）：

```cpp
#pragma once

#include <cstring>
#include <string>
#include <system_error>

#include "cmd_args/base/def.h"

CMD_ARGS_NAMESPACE_BEGIN

/*
 * 校验期错误域（ValidationError_E）：赋值完成后、动作执行前的统一校验失败。
 * 典型场景：必选参数/选项/环境变量缺失（MISSING_*），值未通过校验器（VALIDATION_FAILED）。
 * 三件套惯例与其它错误域一致（枚举 → 映射 → category → make_error_code 特化）。
 */

/* 校验错误码：OK=0 表示无错 */
enum class ValidationError_E
{
    OK = 0,
    MISSING_ARGUMENT,   // 必需参数缺失
    MISSING_OPTION,     // 必需选项缺失
    MISSING_ENV,        // 必需环境变量缺失
    VALIDATION_FAILED   // 值未通过校验器
};

/* 错误码 → 人类可读文本 */
static inline char const *mapValidationError(int _ErrCode) noexcept
{
    switch (static_cast<ValidationError_E>(_ErrCode)) {
    case ValidationError_E::OK: return "no error";
    case ValidationError_E::MISSING_ARGUMENT: return "missing required argument";
    case ValidationError_E::MISSING_OPTION: return "missing required option";
    case ValidationError_E::MISSING_ENV: return "missing required environment";
    case ValidationError_E::VALIDATION_FAILED: return "value validation failed";
    default: return "unknown error";
    }
}

class ValidationErrorCategory : public std::error_category
{
public:
    ValidationErrorCategory()
        : error_category()
    {}
    const char *name() const noexcept override { return ENUM_NAME(ValidationError_E); }
    std::string message(int _V) const override { return mapValidationError(_V); }

    static ValidationErrorCategory const &instance()
    {
        static ValidationErrorCategory instance;
        return instance;
    }
};

inline std::error_code make_error_code(ValidationError_E code)
{
    return {
        static_cast<int>(code),
        ValidationErrorCategory::instance(),
    };
}

inline std::error_condition make_error_condition(ValidationError_E code)
{
    return {
        static_cast<int>(code),
        ValidationErrorCategory::instance(),
    };
}

class ErrorValidation : public std::logic_error
{
public:
    explicit ErrorValidation(ValidationError_E _E, std::string &&_Desc = {})
        : logic_error(_Desc.data())
        , ec__(make_error_code(_E))
    {}

    std::error_code const &code() const noexcept { return ec__; }
    char const *what() const noexcept override
    {
        auto const *p = logic_error::what();
        if (std::strlen(p)) { return p; }
        return mapValidationError(ec__.value());
    }

private:
    std::error_code ec__;
};

CMD_ARGS_NAMESPACE_END

namespace std {
template <>
struct is_error_code_enum<cmd_args::ValidationError_E> : true_type
{};
}  // namespace std
```

3.2 `context_base.h` include 区（`#include "cmd_args/error/parser_error.h"` 之后）加：

```cpp
#include "cmd_args/error/validation_error.h"
```

3.3 `context_base.h` 四个 `catch` 块各补一个分支（`catch (ErrorLexer const &e)` 之后）：

抛出型 `parse(int, ...)`：
```cpp
        catch (ErrorValidation const &e) { setLast(e.code()); throw; }
```
错误码型 `parse(std::error_code&, int, ...)`：
```cpp
        catch (ErrorValidation const &e) { _Ec = e.code(); setLast(e.code()); }
```
抛出型 `parse(Tokens_T const&, Envs_T const&)`：
```cpp
        catch (ErrorValidation const &e) { setLast(e.code()); throw; }
```
错误码型 `parse(std::error_code&, Tokens_T const&, Envs_T const&)`：
```cpp
        catch (ErrorValidation const &e) { _Ec = e.code(); setLast(e.code()); }
```

- [ ] **Step 4: 运行测试确认通过**

Run: `xmake build unit_test && xmake r unit_test -- "[validation]"`
Expected: PASS（2 个用例）。

- [ ] **Step 5: 提交**

```bash
git add include/cmd_args/error/validation_error.h include/cmd_args/context/context_base.h dfx/unit_test/test_validation.cpp
git commit -m "feat(error): add ValidationError_E domain with ErrorValidation"
```

---

### Task 7: 校验引擎与管线集成（Compiler / Interpreter / Context）

**Files:**
- Create: `include/cmd_args/expression/validation.h`
- Modify: `include/cmd_args/expression/compiler.h`、`include/cmd_args/expression/interpreter.h`、`include/cmd_args/context/context.h`
- Test: `dfx/unit_test/test_validation.cpp`（追加 e2e 用例）

**Interfaces:**
- Consumes: T3/T4/T5 的全部命中判定与校验器；T6 错误域。
- Produces: `validate_detail::Result { std::error_code ec; std::string msg; bool ok() const noexcept; }`、`validate_detail::checkEnvSpecs(Model&) -> Result`、`validate_detail::checkRequired(Model const&) -> Result`、`validate_detail::checkValues(Model const&) -> Result`、`validate_detail::validate(Model&) -> Result`。Compiler / Interpreter 在 finalize 中调用 `validate` 并抛 `ErrorValidation`。**管线顺序变更**：批量模式下 `finalize()`（含校验）提前到 `Executor::execute()` 之前（校验失败则不执行任何 action）。

- [ ] **Step 1: 追加失败测试（e2e）**

```cpp
TEST_CASE("validation: 批量管线 required/校验失败进错误码与 Errors 栈", "[validation]")
{
    Context app("ut_val_e2e");
    app.addOption<std::string>("host", "host", "");
    app.option("host")->required();
    app.addOption<int>("port", "port", 0);
    app.option("port")->range(1024, 65535);
    app.build<ArgumentNamed>("name", "n");
    app.argument("name")->required();

    std::error_code ec;
    app.parse(ec, Tokens_T{ "--host", "h1", "--port", "80", "--name", "ok" }, Envs_T{});
    REQUIRE(ec.value() == static_cast<int>(ValidationError_E::VALIDATION_FAILED));
    REQUIRE(app.getLastVal() == static_cast<int>(ValidationError_E::VALIDATION_FAILED));

    // 全部合法 → 通过
    app.reset();
    app.parse(ec, Tokens_T{ "--host", "h1", "--port", "8080", "--name", "ok" }, Envs_T{});
    REQUIRE(!ec);

    // 缺必选 → MISSING_OPTION（首个缺失决定错误码）
    Context app2("ut_val_e2e2");
    app2.addOption<std::string>("host", "h", "");
    app2.option("host")->required();
    app2.parse(ec, Tokens_T{}, Envs_T{});
    REQUIRE(ec.value() == static_cast<int>(ValidationError_E::MISSING_OPTION));

    // 缺必选参数 → MISSING_ARGUMENT
    Context app3("ut_val_e2e3");
    app3.build<ArgumentNamed>("name", "n");
    app3.argument("name")->required();
    app3.parse(ec, Tokens_T{}, Envs_T{});
    REQUIRE(ec.value() == static_cast<int>(ValidationError_E::MISSING_ARGUMENT));

    // 抛异常通道
    REQUIRE_THROWS_AS(app3.parse(Tokens_T{}, Envs_T{}), ErrorValidation);
}

TEST_CASE("validation: 环境 spec 缺省补全与 MISSING_ENV", "[validation][env]")
{
    Context app("ut_env_spec_e2e");
    app.requireEnv("DB_HOST", "host")->semantic("hostname");
    app.requireEnv("DB_PORT", "port")->range(1, 65535)->defaultEnv("5432");

    std::error_code ec;
    app.parse(ec, Tokens_T{}, Envs_T{});
    REQUIRE(ec.value() == static_cast<int>(ValidationError_E::MISSING_ENV));

    app.reset();
    app.parse(ec, Tokens_T{}, Envs_T{ "DB_HOST=localhost" });
    REQUIRE(!ec);
    REQUIRE(app.get_env("DB_PORT") == "5432");    // 缺省补全写入会话

    // 无效值 → VALIDATION_FAILED
    app.reset();
    app.parse(ec, Tokens_T{}, Envs_T{ "DB_HOST=bad..host" });
    REQUIRE(ec.value() == static_cast<int>(ValidationError_E::VALIDATION_FAILED));
}

TEST_CASE("validation: 流式 loop 行末校验（action 即时执行保持）", "[validation][interpreter]")
{
    Context app("ut_val_stream");
    app.addOption<std::string>("host", "h", "");
    app.option("host")->required();
    app.addOption<int>("port", "p", 0);
    app.option("port")->range(1024, 65535);

    // 第一行 port=80 越界 → 行末校验 VALIDATION_FAILED → 输出 (parse error)；
    // 第二行全部合法 → 无错误；行间 reset 保证 port 恢复默认。
    std::istringstream in("--port 80\n--port 8080 --host ok\nexit\n");
    std::ostringstream out;
    std::streambuf *oldIn  = std::cin.rdbuf(in.rdbuf());
    std::streambuf *oldOut = std::cout.rdbuf(out.rdbuf());

    app.loop();

    std::cin.rdbuf(oldIn);
    std::cout.rdbuf(oldOut);

    REQUIRE(out.str().find("(parse error)") != std::string::npos);
    REQUIRE(app.getOption<std::string>("host") == "ok");
    REQUIRE(app.getOption<int>("port") == 8080);
}

- [ ] **Step 2: 运行测试确认编译失败（`validate_detail` 未定义）**

Run: `xmake build unit_test`
Expected: FAIL（`validate_detail` 不是命名空间 / 无 `validate`）。

- [ ] **Step 3: 实现**

3.1 创建 `include/cmd_args/expression/validation.h`（许可头照抄）：

```cpp
#pragma once

#include <string>
#include <vector>

#include "cmd_args/base/def.h"
#include "cmd_args/base/validator.h"
#include "cmd_args/error/validation_error.h"
#include "cmd_args/env/env_builder.h"
#include "cmd_args/context/model.h"

CMD_ARGS_NAMESPACE_BEGIN

/*
    校验引擎（赋值完成后、动作执行前）：三类条目统一校验。
    - checkEnvSpecs：环境 spec 缺省补全 + 必选存在性 + 值校验（可变：缺写补全）
    - checkRequired：参数/选项必选命中判定（首错即返）
    - checkValues  ：参数/选项值依次过校验器（首错即返）
    - validate     ：统一入口（先 spec 后 required 再 values）
*/

namespace validate_detail {

/// @brief 校验结果：ec 非 0 表示失败；msg 为失败描述
struct Result
{
    std::error_code ec;
    std::string     msg;

    bool ok() const noexcept { return !ec; }
};

/// @brief 环境声明条目：缺省补全 + 必选 + 值校验（可变：补全写入会话）
inline Result checkEnvSpecs(Model &_M)
{
    bool hasFail = false;
    _M.envs.forEachSpec([&](EnvSpec const &spec) {
        if (hasFail) return;
        auto *p = _M.envs.doMutGet(spec.strKey);
        bool present = p && !p->strValue.empty();

        if (!present && !spec.strDefault__.empty()) {
            EnvBuilder(&_M.envs).setEnv(spec.strKey, spec.strDefault__);  // 补全（标新）
            p = _M.envs.doMutGet(spec.strKey);
            present = p && !p->strValue.empty();
        }

        if (!present && spec.isRequired()) {
            hasFail = true;
        }
        else if (present) {
            auto msg = spec.validate(p->strValue);
            if (!msg.empty()) {
                hasFail = true;
            }
        }
        // 失败信息暂存（下面统一出栈写出）
        ... (见 3.1b)
    });
}
```

**说明：** 上面的 lambda 捕获/提前返回写法在 C++ 中难以优雅地"即时返回"——执行者按下面这个**展开实现**（判定 → 立即构造 Result 返回）替换：

```cpp
/// @brief 环境声明条目：缺省补全 + 必选 + 值校验（可变：补全写入会话）
inline Result checkEnvSpecs(Model &_M)
{
    std::string msg;
    _M.envs.forEachSpec([&](EnvSpec const &spec) {
        if (!msg.empty()) return;
        auto *p = _M.envs.doMutGet(spec.strKey);
        bool present = p && !p->strValue.empty();

        if (!present && !spec.strDefault__.empty()) {
            EnvBuilder(&_M.envs).setEnv(spec.strKey, spec.strDefault__);
            p = _M.envs.doMutGet(spec.strKey);
            present = p && !p->strValue.empty();
        }

        if (!present && spec.isRequired()) {
            msg = "missing required environment: " + spec.strKey;
            return;
        }
        if (present) {
            auto vmsg = spec.validate(p->strValue);
            if (!vmsg.empty()) {
                msg = "environment " + spec.strKey + ": " + vmsg;
            }
        }
    });

    if (!msg.empty()) {
        bool req = msg.find("missing required environment") == 0;
        return Result{
            make_error_code(req ? ValidationError_E::MISSING_ENV : ValidationError_E::VALIDATION_FAILED),
            std::move(msg)
        };
    }
    return Result{ make_error_code(ValidationError_E::OK), {} };
}

/// @brief 必选检查：命名/方位参数 + 值选项（首错即返）
inline Result checkRequired(Model const &_M)
{
    std::string msg;
    ValidationError_E ec = ValidationError_E::OK;

    if (msg.empty()) {
        _M.args.mgrName.doConVisit([&](Argument const &a) {
            if (msg.empty() && a.isRequired() && !a.isSatisfied()) {
                msg = "missing required argument: " + a.getName();
                ec  = ValidationError_E::MISSING_ARGUMENT;
            }
        });
    }
    if (msg.empty()) {
        _M.args.mgrPos.doConVisit([&](Argument const &a) {
            if (msg.empty() && a.isRequired() && !a.isSatisfied()) {
                msg = "missing required argument: " + a.getName();
                ec  = ValidationError_E::MISSING_ARGUMENT;
            }
        });
    }
    if (msg.empty()) {
        _M.opts.mgrValues.forEachValue([&](OptionValue const &o) {
            if (msg.empty() && o.isRequired() && !o.isSatisfied()) {
                msg = "missing required option: --" + o.strLongName;
                ec  = ValidationError_E::MISSING_OPTION;
            }
        });
    }

    if (!msg.empty()) return Result{ make_error_code(ec), std::move(msg) };
    return Result{ make_error_code(ValidationError_E::OK), {} };
}

/// @brief 值校验：命名/方位参数 + 值选项依次过校验器（首错即返）
inline Result checkValues(Model const &_M)
{
    std::string msg;
    std::string name;

    _M.args.mgrName.doConVisit([&](Argument const &a) {
        if (!msg.empty() || !a.hasValidators() || !a.hasValue()) return;
        auto vmsg = a.validate(a.getValue());
        if (!vmsg.empty()) { msg = "argument " + a.getName() + ": " + vmsg; }
    });
    if (msg.empty()) {
        _M.args.mgrPos.doConVisit([&](Argument const &a) {
            if (!a.hasValidators() || !a.hasValue()) return;
            auto vmsg = a.validate(a.getValue());
            if (!vmsg.empty()) { msg = "argument " + a.getName() + ": " + vmsg; }
        });
    }
    if (msg.empty()) {
        _M.opts.mgrValues.forEachValue([&](OptionValue const &o) {
            if (o.strValue.empty() || !o.hasValidators()) return;
            auto vmsg = o.validate(o.strValue);
            if (!vmsg.empty()) { msg = "option --" + o.strLongName + ": " + vmsg; }
        });
    }

    (void)name;
    if (!msg.empty()) {
        return Result{ make_error_code(ValidationError_E::VALIDATION_FAILED), std::move(msg) };
    }
    return Result{ make_error_code(ValidationError_E::OK), {} };
}

/// @brief 统一校验入口：环境 spec → 必选 → 值校验
inline Result validate(Model &_M)
{
    auto r = checkEnvSpecs(_M);
    if (!r.ok()) return r;
    r = checkRequired(_M);
    if (!r.ok()) return r;
    return checkValues(_M);
}

}  // namespace validate_detail

using validate_detail::Result;

CMD_ARGS_NAMESPACE_END
```

3.2 `compiler.h`：
- include 区（`#include "cmd_args/context/context_fwd.h"` 之后）加：

```cpp
#include "cmd_args/error/validation_error.h"
#include "cmd_args/expression/validation.h"
```

- `finalize()`（147-157 行）改为：

```cpp
    /// 解析后置：seed 环境兜底 + ${VAR} 展开 + 必选/校验 + save-config 写出
    void finalize()
    {
        __seedEnvDefaults();
        __expandEnvValues();

        auto vr = validate_detail::validate(*pModel__);
        if (!vr.ok()) {
            throw ErrorValidation(static_cast<ValidationError_E>(vr.ec.value()), std::move(vr.msg));
        }

        if (!pModel__->strSaveConfigPath.empty()) {
            auto text = config::to_yaml(*pModel__);
            if (!config::write_file(pModel__->strSaveConfigPath, text)) {
                std::cerr << "(save-config) cannot write file: " << pModel__->strSaveConfigPath << "\n";
            }
        }
    }
```

- `__seedEnvDefaults()`（246-260 行）三处命中后加标记：

```cpp
            if (envParser__.has(a.getEnvName())) { a.set(envParser__.get(a.getEnvName())); a.markEnvHit(); }
```
（命名参数与方位参数两处同样；选项处：）

```cpp
            if (envParser__.has(o.strEnvName__)) { o.strValue = envParser__.get(o.strEnvName__); o.bEnvHit__ = true; }
```

- `__tryParse`（160-175 行）catch 区加：

```cpp
        catch (ErrorValidation const &e) {
            return e.code();
        }
```

3.3 `interpreter.h`：
- include 区（`#include "cmd_args/error/parser_error.h"` 之后）加：

```cpp
#include "cmd_args/error/validation_error.h"
#include "cmd_args/expression/validation.h"
```

- `__finalize()`（509-520 行）开头（`__seedEnvDefaults()` 前无需动；在 `__expandEnvValues()` 之后、save-config 之前）插入：

```cpp
        auto vr = validate_detail::validate(*pModel__);
        if (!vr.ok()) {
            throw ErrorValidation(static_cast<ValidationError_E>(vr.ec.value()), std::move(vr.msg));
        }
```

- `__seedEnvDefaults()`（478-494 行）三处命中后加标记（同上，`a.markEnvHit()` / `o.bEnvHit__ = true`）。

- `interpret()` catch 区（113-118 行）加：

```cpp
        catch (ErrorValidation const &e) { _Errors.setLast(e.code()); return e.code(); }
```

3.4 `context.h`：`_doRunPipelineWithoutSubcommands`（150-164 行）改为（**顺序变更**：校验/finalize 前移）：

```cpp
    /// @brief 原有的管线逻辑（无子命令版本）
    void _doRunPipelineWithoutSubcommands(Tokens_T &_Toks, Envs_T const &_Envs)
    {
        compiler__.set_program_name(instData_.strProgramName);
        compiler__.set_description(strPrjName_);

        compiler__.ensureBuiltins();
        compiler__.parse(_Toks, _Envs);
        compiler__.finalize();   // seed env 兜底 + ${VAR} 展开 + 必选/校验（失败即抛，action 不执行）

        if (instData_.hasAction()) {
            executor__.execute();
        }

        _markParsed();
    }
```

（原顺序 `finalize()` 在 Executor 之后；现在前置。语义提升：action 的 `{value}` 将拿到 env 兜底/展开后的最终值，且校验失败不再执行任何 action。README 中"action 仅在 CLI 显式命中触发"的约定不受影响——命中判定仍看 `bExplicit__`。）

- [ ] **Step 4: 运行测试确认通过**

Run: `xmake build unit_test && xmake r unit_test -- "[validation]"`
Expected: PASS（3 个 e2e 用例；同时全量跑一次 `xmake r unit_test` 确认无回归——重点是既有 action / config / env 用例）。

- [ ] **Step 5: 提交**

```bash
git add include/cmd_args/expression/validation.h include/cmd_args/expression/compiler.h include/cmd_args/expression/interpreter.h include/cmd_args/context/context.h dfx/unit_test/test_validation.cpp
git commit -m "feat(validation): wire validation engine into batch/stream pipelines before actions"
```

---

### Task 8: 默认值增强（defaultExpr / defaultFn + ${VAR:-fallback} 展开）

**Files:**
- Modify: `include/cmd_args/env/env_parser.h`、`include/cmd_args/option/option.h`、`include/cmd_args/argument/argument.h`
- Test: `dfx/unit_test/test_validation.cpp`（追加用例）

**Interfaces:**
- Consumes: T3/T4 已有 `reset()` 与默认值成员。
- Produces: `OptionValue` / `Argument` 新增 `using FnDefault_T = std::function<std::string()>`、`defaultExpr(Value_T const&) -> Self_T&`、`defaultFn(FnDefault_T) -> Self_T&`、private（Argument）/ 公开（OptionValue）成员 `fnDefault__`、`bDefaultExpr__`；`reset()` 在 `fnDefault__` 存在时重算 `strDefault__`；`EnvParser::expand` 支持 `${VAR:-fallback}` 回退语法（仅 `${}` 形式）。

- [ ] **Step 1: 追加失败测试**

```cpp
TEST_CASE("defaults: defaultExpr 支持 ${VAR:-fallback} 回退与优先级", "[defaults]")
{
    Context app("ut_def_expr");
    app.addOption<int>("port", "port", 0);
    app.option("port")->defaultExpr("${CMD_DEF_PORT:-8080}");

    std::error_code ec;
    app.parse(ec, Tokens_T{}, Envs_T{ "CMD_DEF_PORT=9090" });
    REQUIRE(!ec);
    REQUIRE(app.getOption<int>("port") == 9090);          // 环境命中

    app.reset();
    app.parse(ec, Tokens_T{}, Envs_T{});
    REQUIRE(!ec);
    REQUIRE(app.getOption<int>("port") == 8080);          // 回退值

    // 显式 CLI 覆盖表达式
    app.parse(ec, Tokens_T{ "--port", "7000" }, Envs_T{ "CMD_DEF_PORT=9090" });
    REQUIRE(!ec);
    REQUIRE(app.getOption<int>("port") == 7000);
}

TEST_CASE("defaults: defaultFn 每次 reset 重算（loop 单行语义）", "[defaults]")
{
    Context app("ut_def_fn");
    int tick = 0;
    app.addOption<int>("n", "n", 0);
    app.option("n")->defaultFn([&] { return std::to_string(++tick); });

    std::error_code ec;
    app.parse(ec, Tokens_T{}, Envs_T{});
    REQUIRE(!ec);
    REQUIRE(app.getOption<int>("n") == 1);

    app.reset();
    app.parse(ec, Tokens_T{}, Envs_T{});
    REQUIRE(!ec);
    REQUIRE(app.getOption<int>("n") == 2);
}

TEST_CASE("defaults: Argument defaultExpr 同样生效", "[defaults]")
{
    Context app("ut_def_expr_arg");
    app.build<ArgumentNamed>("host", "h");
    app.argument("host")->defaultExpr("${CMD_DEF_HOST:-localhost}");

    std::error_code ec;
    app.parse(ec, Tokens_T{}, Envs_T{});
    REQUIRE(!ec);
    REQUIRE(app.getArgument<std::string>("host") == "localhost");

    app.reset();
    app.parse(ec, Tokens_T{}, Envs_T{ "CMD_DEF_HOST=my.host" });
    REQUIRE(!ec);
    REQUIRE(app.getArgument<std::string>("host") == "my.host");
}
```

- [ ] **Step 2: 运行测试确认编译失败（`defaultExpr` 未定义）**

Run: `xmake build unit_test`
Expected: FAIL（`OptionValue` 无 `defaultExpr` / `defaultFn`；Argument 同）。

- [ ] **Step 3: 实现**

3.1 `env_parser.h` `expand` 支持 `${VAR:-fallback}`：
- 函数体开头（`std::string ret;` 之前）加 `std::string strFallback;`
- `${...}` 分支（183-189 行）改为：

```cpp
                if (i + 1 < _Text.size() && _Text[i + 1] == '{') {
                    endPos = _Text.find('}', i + 2);
                    if (endPos != std::string::npos) {
                        std::string inner = _Text.substr(i + 2, endPos - i - 2);
                        auto        sep   = inner.find(":-");
                        if (sep != std::string::npos) {
                            name        = inner.substr(0, sep);
                            strFallback = inner.substr(sep + 2);
                        }
                        else {
                            name = inner;
                        }
                        brace = true;
                    }
                }
```

- 命中分支不变；未命中时（`if (!name.empty() && endPos != ...)` 块内 `if (p) {...}` 之后）加：

```cpp
                    if (strFallback.empty() == false) {
                        ret += strFallback;
                        i = endPos + 1;
                        continue;
                    }
```

注意：`strFallback` 在每次 `${}` 处理后要复位（避免上一条残留）——在命中分支 `continue` 前与未命中回退分支 `continue` 前都无需（因为变量只在同一轮使用；为稳妥，在 `if (p)` 命中分支里加 `strFallback.clear();`，并在回退分支 `continue` 前加 `strFallback.clear();`）。

3.2 `option.h`：
- `OptionValue` 的类型别名区加：

```cpp
    using FnDefault_T = std::function<std::string()>;     ///< 惰性默认值求值函数
```

- 「构造期默认与钩子」区（`strActionTemplate__` 之后）加：

```cpp
    FnDefault_T fnDefault__;      ///< 惰性默认值：reset() 时重算（循环每行一次）
    bool        bDefaultExpr__ = false;  ///< 默认值来自表达式（schema 展示用）
```

- `defaultValue()` / `usesDefault()` 之后加：

```cpp
    /** @brief 表达式默认值：形如 "${PORT:-8080}"，经 ${VAR:-fallback} 展开取最终值
     *  @return Self_T& 支持链式调用
     *  @note 覆盖既有默认值；展开发生在解析后置（seed + expand 阶段） */
    auto defaultExpr(Value_T const &_Expr) -> Self_T &
    {
        strDefault__   = _Expr;
        strValue       = _Expr;
        bDefaultExpr__ = true;
        fnDefault__    = {};
        return *this;
    }

    /** @brief 惰性默认值：reset() 时重新求值（loop 每行前调用一次）
     *  @param _Fn 求值函数（返回字符串；不应抛异常）
     *  @return Self_T& 支持链式调用 */
    auto defaultFn(FnDefault_T _Fn) -> Self_T &
    {
        fnDefault__ = std::move(_Fn);
        if (fnDefault__) {
            strDefault__ = fnDefault__();
            strValue     = strDefault__;
        }
        return *this;
    }
```

- `reset()` 首部与 `bEnvHit__` 行间改：

```cpp
    auto reset() noexcept -> Self_T &
    {
        if (fnDefault__) { strDefault__ = fnDefault__(); }  // 惰性默认值重算
        strValue    = strDefault__;
        bExplicit__ = false;
        bExternal__ = false;
        bEnvHit__   = false;
        return *this;
    }
```

3.3 `argument.h`：对称追加（类型别名区加 `using FnDefault_T = std::function<std::string()>;`；私有成员区加 `FnDefault_T fnDefault__;` 与 `bool bDefaultExpr__ = false;`；public 区 `defaultValue()` 之后加 `defaultExpr` / `defaultFn`（同上，返回 `Self_T &`）；`reset()` 内 `strValue__ = strDefault__;` 之前插入 `if (fnDefault__) { strDefault__ = fnDefault__(); }`——`bEnvHit__ = false;` 行在 T4 已加入，此处无需重复。）。

- [ ] **Step 4: 运行测试确认通过**

Run: `xmake build unit_test && xmake r unit_test -- "[defaults]"`
Expected: PASS（3 个用例；`[env]` 旧用例无回归）。

- [ ] **Step 5: 提交**

```bash
git add include/cmd_args/env/env_parser.h include/cmd_args/option/option.h include/cmd_args/argument/argument.h dfx/unit_test/test_validation.cpp
git commit -m "feat(defaults): defaultExpr/defaultFn and ${VAR:-fallback} expansion"
```

---

### Task 9: schema / parameter 导出（required / default / validators 标注）

**Files:**
- Modify: `include/cmd_args/config/parameter.h`、`include/cmd_args/config/config_store.h`、`include/cmd_args/context/context_base.h`
- Test: `dfx/unit_test/test_validation.cpp`（追加用例）

**Interfaces:**
- Consumes: T3/T4 的 `isRequired/validators/hasDefault/usesDefault`。
- Produces: `parameter_entry` 新字段 `required / default_set / uses_default / validators`；`config::schema_of` 填充；`ContextBase::schema_string()` 打印 `[required]` / `[default]` / `[validators: ...]` 标签。

- [ ] **Step 1: 追加失败测试**

```cpp
TEST_CASE("schema: required/default/validators 进入参数格式与文本", "[schema]")
{
    Context app("ut_schema_val");
    app.addOption<int>("port", "port", 8080);
    app.option("port")->required().range(1, 65535);
    app.build<ArgumentNamed>("name", "n");
    app.argument("name")->required();
    app.addOption<std::string>("host", "host", "");
    app.option("host")->semantic("ipv4");

    // T7 之后 parse 会真正校验，因此本行显式给足全部 required（port / name）
    std::error_code ec;
    app.parse(ec, Tokens_T{ "--host", "10.0.0.1", "--port", "8080", "--name", "n" }, Envs_T{});
    REQUIRE(!ec);

    auto sch = app.schema();
    auto const *pe = sch.find("port");
    REQUIRE(pe != nullptr);
    REQUIRE(pe->required == true);
    REQUIRE(pe->default_set == true);
    REQUIRE(pe->uses_default == false);        // 本轮显式给出 --port 8080
    REQUIRE(pe->validators.size() == 1);
    REQUIRE(pe->validators[0].find("range") != std::string::npos);

    auto const *ne = sch.find("name");
    REQUIRE(ne != nullptr);
    REQUIRE(ne->required == true);
    REQUIRE(ne->default_set == false);

    auto const &txt = app.schema_string();
    REQUIRE(txt.find("[required]") != std::string::npos);
    REQUIRE(txt.find("[default]") != std::string::npos);
    REQUIRE(txt.find("[validators:") != std::string::npos);
}

- [ ] **Step 2: 运行测试确认编译失败（无新字段）**

Run: `xmake build unit_test`
Expected: FAIL（`parameter_entry` 无 `required` / `default_set`）。

- [ ] **Step 3: 实现**

3.1 `parameter.h` `struct parameter_entry` 尾部（`external_set` 之后）加：

```cpp
    bool        required{false};                 // 声明为必选（未满足将报 MISSING_*）
    bool        default_set{false};              // 声明了非空默认值
    bool        uses_default{false};             // 当前值来自默认值
    std::vector<std::string> validators;         // 校验器名称列表（schema 展示）
```

3.2 `config_store.h` `schema_of` 三个 entry 构造处（named / positional / option）补齐（option 分支加在 `e.external_set = o.bExternal__;` 之后；named / positional 加在 `e.external_set = a.isExternal();` 之后）：

```cpp
        e.required     = a.isRequired();
        e.default_set  = a.hasDefault();
        e.uses_default = a.usesDefault();
        for (auto const &v : a.validators()) { e.validators.push_back(v.strName); }
```
（option 分支对应 `o.` 前缀。）

3.3 `context_base.h` `schema_string()`（316-329 行）更新：

```cpp
    std::string schema_string() const
    {
        std::ostringstream oss;
        auto               sch = schema();
        oss << "schema: " << (sch.program.empty() ? strPrjName_ : sch.program) << "\n";
        for (auto const &e : sch.entries) {
            oss << "  [" << e.kind << "] " << e.name;
            if (!e.type.empty()) { oss << " <" << e.type << ">"; }
            if (!e.value.empty()) { oss << " = \"" << e.value << "\""; }
            if (e.required) { oss << " [required]"; }
            if (e.default_set) { oss << " [default]"; }
            if (!e.validators.empty()) {
                oss << " [validators:";
                for (auto const &v : e.validators) { oss << " " << v; }
                oss << "]";
            }
            if (!e.env.empty()) { oss << "  env: " << e.env; }
            oss << "\n";
        }
        return oss.str();
    }
```

- [ ] **Step 4: 运行测试确认通过**

Run: `xmake build unit_test && xmake r unit_test -- "[schema]"`
Expected: PASS（1 个用例 + 既有 config/schema 用例无回归——`schema_string` 输出兼容追加，不影响既有断言）。

- [ ] **Step 5: 提交**

```bash
git add include/cmd_args/config/parameter.h include/cmd_args/config/config_store.h include/cmd_args/context/context_base.h dfx/unit_test/test_validation.cpp
git commit -m "feat(schema): export required/default/validators markers into parameter format"
```

---

### Task 10: 示例与 README 收尾

**Files:**
- Create: `example/validate/main.cpp`、`example/validate/xmake.lua`
- Modify: `example/xmake.lua`、`README.md`
- Test: 手动运行示例 + `xmake build unit_test` 全量回归

**Interfaces:**
- Consumes: 全部前序任务。
- Produces: 面向使用者的文档与可运行示例。

- [ ] **Step 1: 创建示例**

`example/validate/main.cpp`（许可头之后）：

```cpp
/*
 * example / validate
 *
 * 演示 必选检查 + 值校验器 + 默认值判断 + 声明式环境条目：
 *   1. option("host")->required().semantic("ipv4")
 *   2. option("port")->range(1024, 65535)  + defaultExpr("${DEMO_PORT:-8080}")
 *   3. argument("name")->required()
 *   4. requireEnv("DB_HOST")->semantic("hostname")->defaultEnv("localhost")
 *
 * 运行示例：
 *   xmake build validation
 *   xmake r validation -- --host 127.0.0.1 --port 9090 peter
 *   xmake r validation -- --host bad-ip peter      # VALIDATION_FAILED
 *   xmake r validation -- peter                    # MISSING_OPTION
 */
#include <iostream>
#include <string>

#include "cmd_args/cmd_args.hpp"

int main(int _Argc, char *_Argv[], char *_Envp[])
{
    using namespace cmd_args;

    Context app("validation_demo");

    app.addOption<std::string>("host", "ipv4 host", "");
    app.option("host")->required().semantic("ipv4");
    app.addOption<int>("port", "port number", 0);
    app.option("port")->range(1024, 65535)->defaultExpr("${DEMO_PORT:-8080}");
    app.build<ArgumentNamed>("name", "your name");
    app.argument("name")->required();
    app.requireEnv("DB_HOST", "database host")->semantic("hostname")->defaultEnv("localhost");

    std::error_code ec;
    app.parse(ec, _Argc, const_cast<char const **>(_Argv), _Envp);
    if (ec) {
        std::cerr << "(validation error) " << ec.message() << "\n" << app.dump();
        return static_cast<int>(ec.value());
    }

    std::cout << "host  = " << app.getOption<std::string>("host") << "\n";
    std::cout << "port  = " << app.getOption<int>("port") << "\n";
    std::cout << "name  = " << app.getArgument<std::string>("name") << "\n";
    std::cout << "DB_HOST = " << app.get_env_or("DB_HOST", "<defaulted>") << "\n";
    return 0;
}
```

`example/validate/xmake.lua`（照抄 `example/once/xmake.lua`，target 改名 `validation`）：

```lua
target("validation")
    set_kind("binary")
    set_symbols("debug")
    set_default(false)
    add_files("*.cpp")
    add_includedirs("$(projectdir)/include", {public = true})
```

`example/xmake.lua` 追加一行 `includes("validate")`。

- [ ] **Step 2: 构建并运行示例 3 个场景**

```bash
xmake build validation
xmake r validation -- --host 127.0.0.1 --port 9090 peter      # 成功
xmake r validation -- --host bad-ip --port 9090 peter          # 校验失败（ec 非 0）
xmake r validation -- peter                                     # 缺 --host（MISSING_OPTION）
```
Expected: 场景 1 打印取值；场景 2/3 stderr 输出错误信息且退出码非 0。

- [ ] **Step 3: 更新 README.md**

3.1 「用法示例」代码块（README 107 行 `app.load_config(...)` 之后）追加一段：

```cpp
// 必选 + 校验器 + 默认值判断（链式挂在句柄上）
app.option("host")->required().semantic("ipv4");       // 取值非法即 VALIDATION_FAILED
app.option("timeout")->range(1, 3600);                 // 数值区间
app.option("mode")->oneOf({"fast","slow"})->match(R"(^\w+$)");   // 枚举 + 正则
app.argument("name")->required();                      // 必选参数
app.requireEnv("DB_HOST")->semantic("hostname")->defaultEnv("localhost");

// 默认值判断：来源标记
app.option("port")->defaultExpr("${CMD_PORT:-8080}");  // 表达式默认（含回退）
app.option("port")->usesDefault();   // 当前值是否来自默认值
app.option("port")->hasDefault();    // 是否声明默认值
app.option("port")->defaultValue();  // 内置默认值
```

3.2 「命令行语法速查」追加一行：

```
- `->required()` / `->range(a,b)` / `->oneOf({..})` / `->match(re)` / `->semantic("ipv4"|"ipv6"|"port"|"file"|"path"|"date"|"duration"|"hostname"|"uuid"|"float"|"double")` —— 句柄链式约束
```

3.3 「错误处理」小节补一句：第四类错误域 `ValidationError_E`（`MISSING_ARGUMENT` / `MISSING_OPTION` / `MISSING_ENV` / `VALIDATION_FAILED`），赋值完成后、action 执行前统一校验，同样支持异常与 `std::error_code` 双通道。

3.4 「已实现 / 展望」：把「未来升级计划」中的 **值验证器（Range/Enum/Regex/Custom）**、**必选约束（必选参数/必选选项，非数值计数）**、**默认值增强（表达式/惰性默认值/默认值判断）**、**Option 类型扩充（语义校验路线：ipv4/ipv6/port/file/date/duration/hostname/uuid/float/double）** 划入「已实现」；
「仍在展望」补充：`counter` 计数选项（`-vvv`→3，涉及解析层聚合改造）、`requiredIf` / `mutuallyExclusive` 条件必选、`removeOption` 卸载、Array Checker（`minSize`/`maxSize`，依赖容器语义）、默认值继承（子命令场景）。

3.5 「编译与测试」更新单测统计行（改为实际计数；执行者跑 `xmake r unit_test` 后按输出填写用例/断言数）。

- [ ] **Step 4: 全量回归**

```bash
xmake build unit_test
xmake r unit_test
```
Expected: 全量 PASS（无回归；既有 57 用例 + 新增约 17 用例）。

- [ ] **Step 5: 提交**

```bash
git add example/validate example/xmake.lua README.md
git commit -m "docs: add validation example and update README for required/validator/defaults"
```

---

## Self-Review（写计划时的自查记录）

**1. Spec 覆盖（README 未来升级计划 → 任务）：**

| README 条目 | 任务 | 落地方式 |
|---|---|---|
| 值验证器 Range/Enum/Regex/Custom | T1-T3、T4 | `range/oneOf/match/check` + `ValidatorMixin` |
| 值验证器 Array Checker | 未做（见后续计划） | 值统一字符串，容器语义待定 |
| 默认值：表达式 / 惰性 / 可见性 | T8、T9 | `defaultExpr`（`${VAR:-fallback}`）、`defaultFn`（reset 重算）、schema 标注 |
| 默认值继承 | 未做（后续计划） | 依赖子命令/父 Context 机制 |
| 必选参数 / 必选选项 | T3、T4、T7 | `required()` + `MISSING_ARGUMENT/OPTION` |
| 环境变量必选 | T5、T7 | `requireEnv` / `EnvSpec` → `MISSING_ENV` |
| 数值 Flag（count） | 未做（后续计划） | 解析层聚合改造，见 roadmap |
| 条件必选 / 互斥组 | 未做（后续计划） | 见 roadmap |
| Option 类型扩充（FilePath/Counter/Date/Duration） | T1、T2（语义校验路线） | `semantic("ipv4"/.../"file"/"date"/"duration")`；counter 见 roadmap |
| ipv4/ipv6/port（用户发散补充） | T1、T2 | 纯函数 + `semantic()` 表 |
| Option 卸载 / 动态约束 | T2（部分） | `required(false)` 动态取消已支持；`removeOption` 见 roadmap |

**2. 占位符扫描：** 计划中无 TBD / TODO /「类似 Task N」；每个实现代码块完整。**已消化的 3 处起草期修正**：① T3/T4/T9 的测试改为「每次 parse 都满足全部 required」形态（T7 校验接入后全量回归仍绿）；② T5 测试改用 mixin 断言替代误写的 `isEnvHit`；③ T7 流式测试用 `--port 80` 越界直击 VALIDATION_FAILED 路径（而非未注册选项）。全部以计划内最终代码为准。

**3. 类型一致性核对：**
- `isSatisfied()`：OptionValue 依据 `bExplicit__/bExternal__/bEnvHit__`；Argument 依据 `bExternal__/bEnvHit__/hasValue()` —— 与各自既有成员一致。
- `bEnvHit__`：三处 seed 钩子（Compiler 命名/方位/选项 + Interpreter 同名三处）统一置位；`reset()` 统一清理——Argument 经 `markEnvHit()` 公开方法（private 成员），OptionValue 直接赋值（公开成员）。
- `validate_detail::Result`：`checkEnvSpecs/checkRequired/checkValues/validate` 返回类型一致；Compiler/Interpreter 抛 `ErrorValidation(static_cast<ValidationError_E>(vr.ec.value()), ...)` —— `ErrorValidation` 构造签名仅接受 `ValidationError_E`，转换安全（枚举值域相同）。
- `parameter_entry` 新字段名：`required/default_set/uses_default/validators` 在 parameter.h、config_store.h、context_base.h 三处一致。
- `defaultExpr/defaultFn`：OptionValue 与 Argument 同名同签名（`Value_T const&` / `FnDefault_T`），reset 内重算逻辑一致。

**4. 兼容性：** 未改动 `build<T>()` 签名与任何既有公开接口的返回类型；`schema_string()` 输出为追加式（不删除既有文本）；批量管线顺序变更（finalize 前移）在 T7 说明理由与影响面（action 拿到最终值、校验失败不执行 action），并以测试锁定。

---

## 后续计划（本次不做，起草为 roadmap）

1. **counter 计数选项**：`option<Counter>` / `->count()`（`-vvv` → 3、`--verbose=5`）。需要 `Lexer/Scanner` 与 `Interpreter::__reduceStack` 的聚合短选项分支（现仅 bool 可聚合，非 bool 报 `MUST_BE_BOOL`）扩展 counter 类型判定 + 计数累加语义。
2. **条件必选 requiredIf / 互斥组 mutuallyExclusive**：挂接既有 `optionGraph`（条件/依赖边）与 `OptionGroup`；校验引擎增加 `checkRelations(Model const&)`。
3. **removeOption / Option 分组启用禁用 / build_if**：需要 `ValueOptMgr` 增加删除/禁用接口（erase / mask），Model 增加运行时约束开关。
4. **Array Checker（minSize/maxSize）**：以分隔符约定（如逗号）或预留容器类型表达式语义后再定。
5. **默认值继承**：父 Context/子命令场景下默认值合并，需要子命令路由处的格式合并机制。
6. **日期/时长格式化入口**：`->format("%Y-%m-%d")` 等输出格式化（当前仅校验语义）。

---

## 执行交接

计划已保存。两种执行方式：

**1. Subagent-Driven（推荐）** —— 每个任务派一个全新 subagent 执行，任务间我做两阶段审查（代码 + 测试），快速迭代。

**2. Inline Execution** —— 本会话内按 executing-plans 批量执行，设检查点供审查。

请选择执行方式。
