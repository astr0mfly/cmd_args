# base — 命名空间宏、字符串工具、作用域守卫与语义校验器

> **摘要**：base 是 cmd_args 的最底层目录，其余模块直接依赖它。`def.h` 统一命名空间并给出 `ARRAY_SIZE`/`ENUM_NAME`；`util.h` 提供类型名映射、字符串 ↔ 标量解析及 trim/split 等纯函数；`defer.hpp` 提供 RAII 退出守卫；`validator.h` 提供字符串语义校验器、工厂函数与可链式混入 `ValidatorMixin`。四个头文件零第三方依赖（`validator.h` 仅额外依赖库内 `error/builder_error.h`）。
> **联系**：上级索引 [`../cmd_args.md`](../cmd_args.md)；根说明 [`../../../README_zh.md`](../../../README_zh.md)（“目录结构”一节）。被 [`../argument/argument.h`](../argument/argument.h)、[`../option/option.h`](../option/option.h)、[`../env/env.h`](../env/env.h)、[`../expression/validation.h`](../expression/validation.h) 使用；校验器行为测试见 [`../../../dfx/unit_test/test_validation.cpp`](../../../dfx/unit_test/test_validation.cpp)。

## truth

| 文件 | 行数 | 对外内容 |
| --- | --- | --- |
| `def.h` | 41 | `CMD_ARGS_NAMESPACE_BEGIN`/`CMD_ARGS_NAMESPACE_END`、`ARRAY_SIZE`、`ENUM_NAME`、`detail::fakeArraySize` |
| `util.h` | 222 | `type_string<T>`、`to_string`、`parse_value<T>`、`parse_bool`、`starts_with`/`ends_with`、`trim`/`split`/`unwrap`/`replace`/`parse_line`、`Group<T>` |
| `defer.hpp` | 121 | `detail::Defer<F>`、`detail::UniqueDefer<F>`、`make_defer`/`make_unique_defer`、宏 `DEFER`/`UNIQUE_DEFER`/`DEFER_`/`UNIQUE_DEFER_` |
| `validator.h` | 528 | 12 个 `is_*` 谓词、`Validator`、`ValidatorMixin<Self_T>`、`between`/`one_of`/`matches`/`custom`、`SemanticTable_T`/`semanticTable`/`semantic_validator` |

### def.h

- `CMD_ARGS_NAMESPACE_BEGIN` 展开为 `namespace cmd_args {`，`CMD_ARGS_NAMESPACE_END` 展开为 `}`（`def.h:28-29`，2026-10-25 核对）。改名只需改这两行。
- `cmd_args` 前缀式宏 `CMD_ARGS_NAMESPACE_PREFIX`、`CMD_ARGS_CONCAT`、`CMD_ARGS_NAMESPACE` 已被注释（`def.h:30-32`），不再使用。
- `ARRAY_SIZE(_A)` 展开为 `sizeof(::detail::fakeArraySize(_A))`（`def.h:40`）。`fakeArraySize` 是**全局** `namespace detail` 中的未求值函数模板声明：`template <typename T, unsigned int N> char (&fakeArraySize(T (&)[N]))[N];`（`def.h:35-37`）。
- `ENUM_NAME(_E)` 展开为 `#_E`（`def.h:41`），用于 `std::error_category::name()` 返回枚举类型名。

### util.h

| 符号 | 行为 |
| --- | --- |
| `type_string<T>()` | 默认返回 `"null"`；对 `bool`/`int`/`int64_t`/`double`/`float`/`char`/`unsigned int`/`size_t`/`std::string` 特化，其中 `std::string` 返回 `"string"`（`util.h:38-93`） |
| `to_string<T>(v)` | 经 `std::ostringstream` 输出（`util.h:100`） |
| `parse_value<T>(s)` | 默认构造 `T result{}`，经 `std::istringstream >>` 读取后按值返回；流失败时返回默认值（`util.h:106-115`） |
| `parse_bool(s)` | `true`/`TRUE`/`yes`/`on` → `true`；`false`/`FALSE`/`no`/`off` → `false`；其余交给 `parse_value<bool>`（`util.h:118-123`） |
| `starts_with`/`ends_with` | 前缀/后缀判断，`noexcept`（`util.h:126`、`133`） |
| `trim` | 去掉首尾 `" \t\r\n"`；全空白返回空串（`util.h:140`） |
| `split(text, delim)` | 逐字符切分，**保留空段**；空输入产生一个空段（`util.h:151`） |
| `unwrap(text, left, right)` | 首尾恰好是一对括号字符时去掉一层，否则原样返回（`util.h:171`） |
| `replace(str, from, to)` | 替换全部 `from`（`util.h:181`）；`from` 为空会死循环（见“教训”） |
| `parse_line(line)` | 在首个 `'='` 处拆成键值对；无 `'='` 返回空 pair（`util.h:194`） |
| `Group<T>` | 继承 `std::initializer_list<T>` 的轻量包装，导出 `Super_T`/`Element_T`（`util.h:205-218`），并以 `using util::Group;` 暴露到 `cmd_args`（`util.h:220`） |

`parse_value` 在函数内默认构造 `T result{}` 再流式读取，因此解析失败（或输入只被部分消费）时返回的是该类型的默认值，调用方无法从返回值判断成功与否。

### defer.hpp

- `Defer<F>`：构造时转发可调用对象，析构时执行 `fnRun__`；拷贝构造/赋值被删除（`defer.hpp:28-46`）。
- `UniqueDefer<F>`：同样在析构执行，但 `dismiss()` 置位后不再执行（`defer.hpp:48-70`）。
- `make_defer`/`make_unique_defer` 做类型推导（`defer.hpp:98`、`105`）。
- 宏 `DEFER(e)`/`UNIQUE_DEFER(e)` 用 `__LINE__` 拼接变量名；带下划线的 `DEFER_`/`UNIQUE_DEFER_` 接受显式变量名（`defer.hpp:115-121`）。
- 仓库内（`include/`、`example/`、`dfx/`）没有任何文件 include `defer.hpp`，也无 `DEFER`/`make_defer` 使用点 ⇒ 当前为备用工具头。

### validator.h

谓词（全部接受 `std::string const&`，返回 `bool`，`noexcept`）：

| 谓词 | 通过条件 |
| --- | --- |
| `is_port` | 纯数字、长度 ≤ 5、数值 1..65535（允许前导零，`"00080"` 合法） |
| `is_ipv4` | 恰好 4 段、每段纯数字且 ≤ 255、段长 ≤ 3（允许前导零） |
| `is_ipv6` | 8 组 hex，或至多一次 `::` 压缩后剩余组数 ≤ 7；单冒号结尾非法；允许末尾内嵌 IPv4 尾段（计 2 组） |
| `is_float`/`is_double` | `trim` 后非空，`strtof`/`strtod` 消费整串、`errno != ERANGE`、`std::isfinite` |
| `is_date` | 严格 `YYYY-MM-DD` 10 字符，且为真实日历日期（含闰年判断） |
| `is_duration` | 数字（可带小数）+ 单位 `s`/`m`/`h`/`d`；裸数字视为秒；单位仅小写 |
| `is_hostname` | RFC1123 label：1..63 字符、总长 ≤ 253、首尾为字母数字、内部可含 `-` |
| `is_uuid` | 36 字符 `8-4-4-4-12` 十六进制 |
| `is_path` | 非空且不含 `NUL`；**不校验路径是否存在** |
| `is_existing_file`/`is_existing_dir` | 基于 `std::filesystem` + `error_code` 判断存在且类型匹配 |

`Validator`（`validator.h:230-256`）：

- 成员：`strName`、`fnCheck`（`FnCheck_T = std::function<bool(std::string const&)>`）、`strFailMsg`。
- `std::string validate(std::string const &) const`：`fnCheck` 为空或返回 `true` → 空串（通过）；否则若 `strFailMsg` 非空，把其中全部 `%value%` 替换成实际值；否则生成 `invalid value '<v>' (<name>)`（`strName` 为空时无括号部分）。
- 工厂：`between(min, max, minInclusive = true, maxInclusive = true)` 先要求值能按 double 解析；`one_of({...})`；`matches(pattern)` 用 `std::regex_match`，非法正则返回失败；`custom(fn, name = "custom", msg = "%value% rejected")`（`validator.h:259-311`）。

语义表（`validator.h:315-331`）是 `semanticTable()` 内的局部 `static` `SemanticTable_T`（`std::vector<std::pair<std::string, Validator>>`），固定 11 项：

`ipv4`、`ipv6`、`port`、`file`、`path`、`date`、`duration`、`hostname`、`uuid`、`float`、`double`。

`semantic_validator(name)` 命中则返回值拷贝；未知名抛 `ErrorArgument(BuildError_E::UNSUPPORT_TYPE)`（`validator.h:334-342`）。

`ValidatorMixin<Self_T>`（CRTP，`validator.h:344-495`）提供：

- `required()`（左值返回 `Self_T&`、右值返回 `Self_T`）、`required(bool)`、`isRequired()`；
- `check(Validator const&)`、`check(FnCheck_T, name = "custom")`、`range(min, max, minInclusive, maxInclusive)`、`oneOf({...})`、`match(pattern)`、`semantic(name)`；
- `hasValidators()`、`validators()`（只读链）、`validate(value)` 返回**第一条**失败消息，全部通过返回空串；
- `semantic(name)` 对同名 `strName` 采取替换而非追加（`validator.h:487-495`）；
- protected 状态：`bool bRequired__ = false`、`std::vector<Validator> vecValidators__`；
- `Self_T` 必须可移动构造（`&&` 重载按值返回 `std::move(static_cast<Self_T&>(*this))`）。

文件尾部以 `using` 把 12 个谓词、`Validator`、`ValidatorMixin`、`between`、`one_of`、`matches`、`custom`、`semantic_validator` 导入 `cmd_args` 命名空间（`validator.h:500-528`）。

## 约束

- **header-only**：本目录只有头文件，无 `.cpp`、无独立构建 target；所有定义须 `inline`/`constexpr`/模板，避免 ODR 冲突。
- **C++20 双平台**：只使用标准库；MSVC 强制 `/utf-8`（见仓库 `xmake.lua`）。不得引入 POSIX 专用或编译器扩展。
- **零第三方依赖**：`def.h`/`util.h`/`defer.hpp` 只含标准库；`validator.h` 仅允许额外依赖 `../error/builder_error.h`。base 不得反向 include `argument/`、`option/`、`env/`、`context/`、`config/`、`expression/` 等上层目录。
- **命名空间**：对外 API 头用 `CMD_ARGS_NAMESPACE_BEGIN`/`CMD_ARGS_NAMESPACE_END` 包裹（`include/cmd_args` 下 60 个头文件中 47 个如此），不直接写 `namespace cmd_args {`；内部实现命名空间直接书写——`util.h` 的 `namespace util`、`validator.h` 的 `namespace validator_detail`，`extern/` 下的 vendored 头保留上游命名空间。改库名只改 `def.h` 两行。
- **新类型同步**：引入可被 CLI 解析的新标量类型时，必须同时提供 `type_string<T>` 特化与可用的 `parse_value<T>` 路径；否则类型名退化为 `"null"`、解析按流默认语义。
- **校验器只认字符串**：`Validator::fnCheck` 的入参是 `std::string`，不感知目标类型；类型相关的语义转换在调用方完成。
- **语义名闭集**：内置语义校验名只有 `semanticTable()` 中 11 个；新增须加表项，否则 `semantic_validator` 运行时抛 `ErrorArgument(BuildError_E::UNSUPPORT_TYPE)`。
- **`ValidatorMixin` 的 `Self_T` 需可移动构造**，且通过 `static_cast<Self_T&>(*this)` 做 CRTP 下转。
- **`util::replace` 的 `from` 不得为空**（否则死循环，见“教训”）。
- **不作求值之外的事**：base 不读环境变量、不访问文件系统（`is_existing_*` 是唯一例外）、不写 stdout。

## 索引

| 文件 | 关键符号 | 职责 |
| --- | --- | --- |
| `def.h` | `CMD_ARGS_NAMESPACE_BEGIN`/`CMD_ARGS_NAMESPACE_END`、`ARRAY_SIZE`、`ENUM_NAME`、`detail::fakeArraySize` | 统一命名空间；编译期数组长度与枚举名工具 |
| `util.h` | `type_string<T>`、`to_string<T>`、`parse_value<T>`、`parse_bool`、`starts_with`/`ends_with`、`trim`/`split`/`unwrap`/`replace`/`parse_line`、`Group<T>` | 类型名映射、字符串解析与纯字符串工具 |
| `defer.hpp` | `detail::Defer<F>`、`detail::UniqueDefer<F>`、`make_defer`、`make_unique_defer`、`DEFER`/`UNIQUE_DEFER`/`DEFER_`/`UNIQUE_DEFER_` | RAII 作用域退出守卫（当前无调用方） |
| `validator.h` | `is_port`/`is_ipv4`/`is_ipv6`/`is_float`/`is_double`/`is_date`/`is_duration`/`is_hostname`/`is_uuid`/`is_path`/`is_existing_file`/`is_existing_dir`、`Validator`、`ValidatorMixin<Self_T>`、`between`/`one_of`/`matches`/`custom`、`SemanticTable_T`/`semanticTable`/`semantic_validator` | 值校验：内置谓词、用户文案、可链式校验器混入 |

grep 锚点（行号基于 2026-10-25 核对）：

- 命名空间宏：`def.h:28-29`
- 全局 `detail::fakeArraySize`：`def.h:35-37`；`ARRAY_SIZE`：`def.h:40`
- `parse_bool`：`util.h:118-123`；`replace`：`util.h:181-193`
- `Defer` 析构：`defer.hpp:34-38`；宏：`defer.hpp:115-121`
- `Validator::validate`：`validator.h:236-256`；语义表：`validator.h:315-331`；`ValidatorMixin`：`validator.h:344`

## 决策

- **用 `CMD_ARGS_NAMESPACE_BEGIN/END` 而不是 `inline namespace` 或前缀宏**：单点改名、调用方可选引入；代价是每个头文件多一对宏，且宏无法被 IDE 的命名空间重构识别。历史上曾设计 `CMD_ARGS_NAMESPACE_PREFIX`/`CMD_ARGS_CONCAT`/`CMD_ARGS_NAMESPACE`（`def.h:30-32` 仍保留注释），后因使用繁琐被否。
- **`ARRAY_SIZE` 用函数模板推导而非 `sizeof(a)/sizeof(a[0])`**：对指针实参直接编译失败，避免静默算错；代价是必须放在**全局** `detail`（见“教训”）。
- **`type_string` 用显式特化而非 RTTI/`typeid`**：字符串稳定、可读、不依赖 RTTI；代价是新增类型需手动补特化（漏写时返回 `"null"`，该字符串也是配置导出中“未知类型”的标记）。
- **校验以“返回空串即通过”表达**：失败时可携带替换过 `%value%` 的用户文案，且无需异常；代价是每次失败才构造消息，调用方需用 `empty()` 判断而非布尔返回值。
- **`Validator` 与 `ValidatorMixin` 分离**：`Validator` 是可拷贝的值对象，`ValidatorMixin` 用 CRTP 给 `Argument`/`Option`/`Env` 复用链式 API 而不引入虚函数；代价是模板化头文件与 `Self_T` 可移动约束。
- **语义表放在函数内 `static`**：避免跨翻译单元的静态初始化顺序问题，也便于头文件式分发；代价是表不可运行期扩展，自定义语义只能用 `custom`。
- **`defer.hpp` 用 `__LINE__` 生成变量名**：兼容不支持 `__COUNTER__` 的编译器；代价是同一行写两个 `DEFER` 会重名（此时用 `DEFER_` 显式命名）。
- **`defer.hpp` 目前不接入任何调用点**：先保留工具头而不强行使用；代价是它同样不进入构建验证，缺陷只能靠阅读发现。

## 教训

- **`ARRAY_SIZE` 引用的是全局 `::detail`，不是 `cmd_args::detail`**：注释写作“经 detail::fakeArraySize 推导”（`def.h:25`），若照注释在库内改写为 `detail::fakeArraySize`，会解析到 `cmd_args::detail` 而找不到声明。`def.h:35-40`（2026-10-25 核对）明确用了前导 `::`；移动/重命名该宏时必须保留。
- **`parse_bool` 对混合大小写一律判假**：只认 `true`/`TRUE`/`yes`/`on` 等精确拼写，`"True"` 落入 `parse_value<bool>` 的失败路径并返回 `false`（`util.h:118-123`）。解析布尔选项时不要指望大小写不敏感。
- **`replace(str, "", to)` 会无限循环**：`find("") == pre_pos` 且 `pre_pos` 不前进（`util.h:181-193`）。传 `from` 前必须判空；这也意味着 `"%value%"` 替换的空模式不可能被调用。
- **`trim` 的空白集只有空格、`\t`、`\r`、`\n`**：垂直制表/换页不在内（`util.h:140-147`）。处理外来文本时不要假设 `isspace` 全集。
- **`split` 保留空段且空输入返回 `{""}`**（`util.h:151-168`）：统计段数或按索引取值前先过滤空段，否则 `"a,,b"` 会得到 3 段。
- **`Validator` 不填 `strFailMsg` 时消息格式不同**：默认消息含 `(name)`，仅当 `strName` 为空才省略括号（`validator.h:247-255`）。依赖消息文本做断言的测试需固定文案。
- **`semantic_validator` 的未知名校验发生在运行期**：编译期无法发现拼写错误，必须等到构造 `Argument`/`Option` 时才抛 `ErrorArgument(UNSUPPORT_TYPE)`（`validator.h:334-342`）。新增语义名时同步 `semanticTable()`。
- **`is_path` 不代表文件存在**：它只查非空与 NUL（`validator.h:209-213`）；需要存在性时用 `is_existing_file`/`is_existing_dir`，后者依赖 `std::filesystem`（Windows 下同样可用）。
- **`Defer` 中抛异常会 `std::terminate`**：析构函数未标 `noexcept(false)`，C++ 默认析构即 `noexcept`，lambda 抛出即终止（`defer.hpp:34-38`）。守卫体内只做不抛出的清理。
- **`ValidatorMixin` 的 `&&` 重载按值返回**：若 `Self_T` 删除移动构造，链式 `std::move(obj).semantic(...)` 无法编译（`validator.h:366-380` 等）。派生类保持可移动。
- **`is_port`/`is_ipv4` 接受前导零**：`"00080"`、`"001.2.3.4"` 均通过（`validator.h:51-69`）。若需要规范化字符串，不能依赖它们拒绝前导零。
