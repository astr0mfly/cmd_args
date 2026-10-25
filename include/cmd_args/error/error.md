# error — 五域错误码、错误堆栈与双通道异常处理

> **摘要**：error 收口 cmd_args 的错误体系。`errors.h` 提供 `ErrorGeneral` 与 `Errors` 运行时错误堆栈；五个域各自定义 `enum class XxxError_E`、`std::error_category` 与异常类（构筑 `BuildError_E`/词法 `LexError_E`/语法 `ParseError_E`/执行 `InterpError_E`/校验 `ValidationError_E`），形成“抛异常”与“`noexcept` + `std::error_code`”双通道；`exception_handler.h` 提供嵌套异常打印与 Windows 崩溃兜底；`functional_util.h` 是未接入的 `Optional`/`Lazy` 备用工具。
> **联系**：上级索引 [`../cmd_args.md`](../cmd_args.md)；根说明 [`../../../README_zh.md`](../../../README_zh.md) 的“错误处理”一节。依赖 [`../base/def.h`](../base/def.h) 的 `ENUM_NAME`，`exception_handler.h` 另依赖 [`../extern/airbag/`](../extern/airbag/) 的 4 个头文件。使用方包括 [`../context/context_base.h`](../context/context_base.h)（私有继承 `Errors`）、[`../expression/`](../expression/) 的 lexer/parser/interpreter 与 [`../base/validator.h`](../base/validator.h)；异常与堆栈行为测试见 [`../../../dfx/unit_test/test_validation.cpp`](../../../dfx/unit_test/test_validation.cpp)、[`../../../dfx/unit_test/test_parser.cpp`](../../../dfx/unit_test/test_parser.cpp)、[`../../../dfx/unit_test/test_lexer.cpp`](../../../dfx/unit_test/test_lexer.cpp)、[`../../../dfx/unit_test/test_interpreter.cpp`](../../../dfx/unit_test/test_interpreter.cpp)。

## truth

| 文件 | 行数 | 内容 |
| --- | --- | --- |
| `errors.h` | 146 | `ErrorGeneral`、`Errors`（错误堆栈） |
| `builder_error.h` | 129 | `detail::BuildError_E`、`ArgErrorCategory`、`ErrorArgument` |
| `lexer_error.h` | 124 | `LexError_E`、`LexErrorCategory`、`ErrorLexer` |
| `parser_error.h` | 132 | `ParseError_E`、`ParseErrorCategory`、`ErrorParse` |
| `interpreter_error.h` | 120 | `InterpError_E`、`InterpErrorCategory`、`ErrorInterp` |
| `validation_error.h` | 115 | `ValidationError_E`、`ValidationErrorCategory`、`ErrorValidation` |
| `exception_handler.h` | 226 | `exception::Exception`、`EnsureException`、`Box`、宏、`exception_init` |
| `functional_util.h` | 204 | `func_util::Optional<T>`、`func_util::Lazy<T>`、`lazy()`（无调用方） |

### ErrorGeneral 与 Errors（`errors.h`）

`ErrorGeneral` 继承 `std::runtime_error`：

- `explicit ErrorGeneral(std::error_code)`，`std::error_code const &code() const noexcept`。
- `char const *what() const noexcept override { return ec__.message().c_str(); }`（`errors.h:41`）——`message()` 返回临时 `std::string`，`c_str()` 指针在表达式结束时悬垂（已知缺陷，见“教训”）。

`Errors`（non-copyable、movable）：

| 成员 | 语义 |
| --- | --- |
| `setLast(std::error_code)` | 把 `ec.message()` 压栈，同时保存 `ecDefault__`（最近错误码） |
| `setMsg(std::string)` | 只压一条消息，不改变 `ecDefault__`（用于 warning） |
| `getLast()` / `getLastVal()` / `getLastMsg()` | 最近错误码 / `.value()` / `.message()` |
| `clearLast()` | 清空消息栈并把 `ecDefault__` 置为默认错误码 |
| `isOk()` | `ecDefault__.value() == 0 && stkErros__.empty()`（类外定义，`errors.h:144`） |
| `tryRaise()` | 非 `isOk()` 时 `throw ErrorGeneral(ecDefault__)` |
| `raise(std::error_code)` | 先 `setLast` 再抛 `ErrorGeneral` |
| `dump()` | 倒序（最新在前）文本：`{\n0 : <最新>\n1 : ...\n}\n` |

`ContextBase` 私有继承 `Errors`，并 `using Errors::dump/getLast/getLastMsg/getLastVal/isOk`（`../context/context_base.h`），派生上下文通过 `_errors()` 访问。

### 五域错误码与异常类

各域的映射函数都是 `static inline char const *f(int) noexcept`，默认文案为 `"unknwon error"`（历史拼写，原样保留；校验域例外，见下）。

| 域 | 枚举值与顺序 | 异常类 / 构造 | category | 典型文案 |
| --- | --- | --- | --- | --- |
| 构筑 | `detail::BuildError_E`：`OK=0, NEED_NAME, NEED_HELP, DUPLICATE_NAME, UNSUPPORT_TYPE, BAD_PREFIX` | `ErrorArgument(std::error_code)` 或 `(BuildError_E)`；`what()` **总是** `mapArgumentError(code)` | `ArgErrorCategory`，`name() = ENUM_NAME(BuildError_E)` | `"need to named"`、`"name already exists"`、`"unsupported type"`、`"option name must be \`-\` or \`--\` prefixed"` |
| 词法 | `LexError_E`：`OK=0, EMPTY_SOURCE, UNTERMINATED_STRING, BARE_DASH, INVALID_TOKEN` | `ErrorLexer(std::error_code)` / `(LexError_E)` / `(LexError_E, std::string&&)` | `LexErrorCategory` | `"no input source"`、`"string is not terminated"`、`"invalid token"` |
| 语法 | `ParseError_E`：`OK=0, NOT_ENOUGH_ARGS, UNKNWON_ARGS, NEED_VALUE, MUST_BE_BOOL, UNEXCEPTED_BARE, WRONG_PREFIX, UNKNOWN_OPTION` | `ErrorParse` 三种构造同上 | `ParseErrorCategory` | `"not enough named_arguments"`、`"argument number missmatching"`、`"option need a value"`、`"unrecognized option"` |
| 执行 | `InterpError_E`：`OK=0, EXIT_REQUESTED, STOPPED, UNKNOWN_COMMAND, END_OF_INPUT` | `ErrorInterp(std::error_code)` / `(InterpError_E)`（无描述构造） | `InterpErrorCategory` | `"exit requested"`、`"interpreter stopped"`、`"end of input"` |
| 校验 | `ValidationError_E`：`OK=0, MISSING_ARGUMENT, MISSING_OPTION, MISSING_ENV, VALIDATION_FAILED` | `ErrorValidation(ValidationError_E, std::string&& desc = {})`（唯一构造） | `ValidationErrorCategory` | `"missing required argument"`、`"missing required option"`、`"missing required environment"`、`"value validation failed"`；默认 `"unknown error"` |

共同点：

- 每个域都有 `XxxErrorCategory::instance()`、`name()`（除构筑域外都在 `cmd_args` 命名空间；构筑域枚举与异常类在 `cmd_args::detail`，文件末尾以 `using detail::BuildError_E; using detail::ErrorArgument;` 导出）。
- 每个域都有 `make_error_code(E)`、`make_error_condition(E)` 与 `std::is_error_code_enum<...>` 特化，因此可直接与 `std::error_code` 互操作。
- 带描述构造的异常把 `std::string&&` 传给基类 `logic_error`（复制）；`what()` 先看基类文本是否非空，非空则返回它，否则回退到该域的错误文案。`ErrorArgument` 是例外：它没有描述构造，`what()` 恒为映射文本。

### 双通道

- **异常通道**：`parse(...)` 失败抛 `ErrorLexer`/`ErrorParse`（`REQUIRE_THROWS_AS` 用例见 `test_lexer.cpp:50-90`、`test_parser.cpp:60-173`）；校验失败抛 `ErrorValidation`（`test_validation.cpp:363`）。
- **`std::error_code` 通道**：`ContextBase` 另有 `parse(std::error_code &, ...) noexcept` 重载，不抛异常而写回错误码（`../context/context_base.h` 的 412/441 行附近）。
- **运行时堆栈通道**：`Errors` 在每次调用内累积消息与最近错误码，供 `dump()` 展示；它不抛异常，warning 也进同一栈。

### exception_handler.h

命名空间 `exception`，实现细节在 `exception::detail`，导出 `exception::Box` 与 `exception::Exception`：

- `Exception(std::string const &_Message, char const *_File, size_t _Line)`：`what()` 返回 `"<file>:<line> : <message>"`（冒号前有空格）。
- `EnsureException(bool, char const *_File, size_t _Line)`：构造时拼 `"ensure failed : <exp>\n<file>(<line>)\ncontext variables:\n"`；`operator<<(std::pair<char const*, T>)` 追加 `"\t<key> : <value>\n"`，`operator<<(int)` 追加带编号的行。
- `Box`：`static void rethrow(msg, file, line)`（`std::throw_with_nested`）、`static void Backtrace(std::exception const&)`（递归 `rethrow_if_nested`，写 `std::cerr`）、`static void handleException(std::exception const&, char const *namefunc)`、`static void print_nested_exception(std::exception_ptr = std::current_exception(), size_t level = 0)`（`dynamic_cast` 到嵌套指针；遇到非 `std::exception` 时 `exit(EXIT_FAILURE)`）。
- 宏（`exception_handler.h:165-185`）：`EXCEPTION_THROW`、`EXCEPTION_RETHROW`、`PRINT_HANDLE_EXCEPTION`、`PRINT_TRACE_EXCEPTION`、`EXCEPTION_RETHROW_UNSPECIFIED`、`ENSURE`。
- `exception_init`（全局命名空间，`exception_handler.h:194-225`）：仅在 `_WIN32 && !__MINGW32__ && !__MINGW64__` 有成员，否则为空 struct。构造时注册：
  - `process_error.on_pure_call` → 打印 `"Oops: pure virtual function call"`；
  - `process_error.pre_system_failure` → 打印 `"Fatal that %s at %s, generating minidump at '%s'"`，随后 `minidump.generate(f)`，失败打印 `"Unable to generate minidump: %s"`；
  - `thread_error.on_terminate` → 打印 `"Failed to process thread that <msg>\nBacktrace:\n"` 并调用 `PRINT_TRACE_EXCEPTION()`；
  - 成员：`airbag::process_error process_error`、`airbag::thread_error thread_error`、`airbag::minidump minidump`。
- 头文件无条件 include `extern/airbag/` 的 `process_error.hpp`、`thread_error.hpp`、`system_failure.hpp`、`minidump.hpp`（不含 `stop_request.hpp`），所以包含本头即带上 airbag 的 Windows 依赖。
- 唯一使用点：`example/once/main.cpp:31` include、`:34` `static exception_init sg_exception_guard;`。

### functional_util.h

- 不在 `cmd_args` 命名空间，而是顶层 `namespace func_util`；文件无 Apache 许可头。
- `Optional<T>`：基于 `std::aligned_storage` 的手写可选值，提供 `operator bool`、`operator*`、`emplace`、`IsInit()`、比较运算；未初始化时解引用抛 `std::logic_error("is not init")`。
- `Lazy<T>`：`std::function<T()>` + `Optional<T>` 缓存，`Value()` 首次求值、`IsValueCreated()` 查询。
- `lazy(fun, args...)` 用 `std::result_of` 推导返回类型。
- 全仓库（`include/`、`example/`、`dfx/`）无任何引用 ⇒ 遗留备用。

## 约束

- **`OK = 0` 不可变**：每个域的第一个枚举值必须是 `0`，`value() == 0` 等价于无错误；新增错误码只能追加在末尾，不得重排/重编号既有值（外部可能保存 `.value()`）。
- **不改历史拼写**：`UNKNWON_ARGS`、`"unknwon error"`、`"missmatching"`、`"infomation"` 都是对外可见文本，修正会改变 `ec.message()` 与既有测试/日志断言；确需修正须同时更新全部依赖方。
- **category 单例**：每个 `XxxErrorCategory` 通过 `instance()` 返回函数内 `static` 实例，`name()` 由 `ENUM_NAME(枚举类型)` 生成（类型名即 category 名）。
- **异常类的 `what()` 必须 `noexcept`**，只做映射/返回已存字符串；不得在 `what()` 内分配或抛异常。
- **`ErrorGeneral::what()` 的结果不可保存**：它返回临时 `std::string` 的内部指针；只能当场使用，需要保存时用 `code().message()`（已知缺陷）。
- **`Errors` 不可拷贝，只能移动**；循环/会话每轮调用前用 `clearLast()` 做行间隔离，否则上一轮消息污染 `isOk()`。
- **warning 也会让 `isOk()` 为假**：`setMsg` 压栈后 `isOk()` 返回 `false`，即使 `ecDefault__` 仍为 0；调用方若只关心错误码，应直接看 `getLastVal()`。
- **错误头文件不得反向依赖上层**：只允许 `../base/def.h` 与标准库；`exception_handler.h` 额外允许 `../extern/airbag/`。`functional_util.h` 保持完全独立（无 `cmd_args` 依赖）。
- **`ENSURE` 当前不可用**：宏展开引用 `::exception::EnsureException`（实际在 `exception::detail` 且未导出）和未定义的 `EXPAND_A`；任何使用点都会编译失败。不要新增 `ENSURE` 调用。
- **`exception_init` 是平台相关兜底**：只在 Windows/MSVC 有实体；非 Windows 下为空 struct。使用方须自行保证 `_WIN32` 分支，且不要把它当作跨平台异常安全机制。
- **不修改 `extern/airbag/`**：vendored 代码，保留版权头（见 [`../extern/airbag/airbag.md`](../extern/airbag/airbag.md)）。
- **新代码不引入 `func_util`**：C++20 已有 `std::optional`；该头未纳入任何构建/测试，新增使用会扩大未验证面。

## 索引

| 文件 | 关键符号 | 职责 |
| --- | --- | --- |
| `errors.h` | `ErrorGeneral`、`Errors`、`setLast`、`setMsg`、`getLast`、`getLastVal`、`getLastMsg`、`clearLast`、`isOk`、`tryRaise`、`raise`、`dump` | 运行时错误码 + 消息堆栈 |
| `builder_error.h` | `detail::BuildError_E`、`mapArgumentError`、`ArgErrorCategory`、`ErrorArgument` | 构筑期错误（命名/帮助/重复/类型/前缀） |
| `lexer_error.h` | `LexError_E`、`mapLexError`、`LexErrorCategory`、`ErrorLexer` | 词法错误（空源/未闭合/裸 `-`/非法 token） |
| `parser_error.h` | `ParseError_E`、`mapParseError`、`ParseErrorCategory`、`ErrorParse` | 语法/装载错误（缺参/缺值/前缀/未知选项） |
| `interpreter_error.h` | `InterpError_E`、`mapInterpError`、`InterpErrorCategory`、`ErrorInterp` | 解释器状态（退出/停止/未知命令/EOF） |
| `validation_error.h` | `ValidationError_E`、`mapValidationError`、`ValidationErrorCategory`、`ErrorValidation` | 校验错误（缺必选参数/选项/环境、值失败） |
| `exception_handler.h` | `exception::Exception`、`EnsureException`、`Box`、`EXCEPTION_THROW`、`EXCEPTION_RETHROW`、`PRINT_HANDLE_EXCEPTION`、`PRINT_TRACE_EXCEPTION`、`ENSURE`、`exception_init` | 嵌套异常打印、文件/行号异常、Windows 崩溃兜底 |
| `functional_util.h` | `func_util::Optional<T>`、`func_util::Lazy<T>`、`lazy` | 备用 `Optional`/`Lazy`（无调用方） |

grep 锚点（行号基于 2026-10-25 核对）：

- `ErrorGeneral::what`：`errors.h:41`；`isOk` 定义：`errors.h:144`；`dump`：`errors.h:120-136`
- `BuildError_E`：`builder_error.h:40`；`mapArgumentError`：`builder_error.h:51`；`ErrorArgument`：`builder_error.h:100`
- `LexError_E`：`lexer_error.h:35`；`ParseError_E`：`parser_error.h:36`；`InterpError_E`：`interpreter_error.h:35`；`ValidationError_E`：`validation_error.h:34`
- `ENSURE`：`exception_handler.h:180-185`；`exception_init`：`exception_handler.h:194`

## 决策

- **每域独立 `std::error_category` 而不是返回裸枚举/字符串**：可与 `std::error_code`/`std::error_condition` 生态互通，`ec.message()` 自带文案；代价是每域约 60 行模板式代码（枚举 + map + category + make_* + trait 特化）。
- **异常类独立于 `std::system_error`**：需要 `logic_error` 语义（调用方编程/输入错误）并能携带自定义描述；`what()` 支持“有描述用描述、无描述用映射”两种来源；代价是异常类各自持有一份 `std::error_code`，不能直接依赖 `system_error::code()`。
- **`Errors` 同时保存“最近错误码 + 消息历史”**：相比只抛异常的路径，它能让 `parse` 聚合多条 warning/错误并在一次 `dump()` 中展示；代价是 `isOk()` 把 warning 也计为不 Ok（带 warning 的测试场景需按 `getLastVal()` 判断）。
- **用 `setMsg` 表达 warning**：warning 无错误码，但仍需进统一诊断栈；代价是堆栈语义混合，“是否成功”不能只看 `isOk()`。
- **构筑域 `BuildError_E` 放在 `cmd_args::detail`**：构筑错误属于内部实现细节，避免 `cmd_args` 顶层枚举过多；通过文件末尾 `using` 仍可写 `cmd_args::BuildError_E`，代价是 `std::is_error_code_enum` 特化也必须在 `std` 命名空间里全限定 `cmd_args::BuildError_E`。
- **双通道（异常 / `noexcept`+`error_code`）**：热路径或不允许异常的环境用后者，普通调用者用前者；代价是同一错误在两套 API 中要各自维护，测试需覆盖两条路径。
- **`exception_handler.h` 用 vendored airbag 实现 Windows 兜底**：minidump/purecall/terminate 需要 SEH 与 DbgHelp，自己实现成本高；代价是平台分支、`/EHa` 依赖与额外的第三方版权文件。
- **`Box::rethrow` 用 `std::throw_with_nested`**：保留异常链而不需自定义链表；代价是 `print_nested_exception` 需要递归 `dynamic_cast`，且遇到非 `std::exception` 直接 `exit`。
- **`functional_util.h` 保留但不再使用**：C++20 的 `std::optional` 已覆盖 `Optional`，`Lazy` 也无调用点；保留是为了不破坏潜在外部引用，代价是它永远不进入构建验证。
- **不使用 `ENSURE`**：宏实现残缺（见“教训”），与其修复一套未被使用的断言宏，不如在调用点显式抛 `Exception`。

## 教训

- **`ErrorGeneral::what()` 返回悬垂指针**：`ec__.message()` 产生临时 `std::string`，`c_str()` 在 `return` 完成后即失效（`errors.h:41`）。`catch (ErrorGeneral const &e) { std::string s = e.what(); }` 是未定义行为；改用 `e.code().message()`。若修复，应把 message 存成员。
- **只有 warning 时 `tryRaise()` 会抛出“成功”错误码**：`isOk()` 要求栈为空，`setMsg` 后栈非空，`tryRaise` 便以 `ecDefault__`（可能 value 0）抛 `ErrorGeneral`，`what()` 得到 `"Success"` 一类文本（`errors.h:100-118`、`144`）。要么不混用 warning 与 `tryRaise`，要么先判 `getLastVal()`。
- **`ENSURE` 双重不可用**：`exception_handler.h:180-181` 展开为 `throw ::exception::EnsureException(...) << EXPAND_A`，但 `EnsureException` 只在 `exception::detail` 且未 `using` 到 `exception`，同时 `EXPAND_A`/`EXPAND_B` 在 183-185 行已被 `#undef`。即便补上命名空间，`EXPAND_A` 作为函数式宏单独出现也无法展开。任何 `ENSURE(...)` 都会编译失败；不要在文档/示例中推荐它。
- **`/EHa` 只在 debug 配置**：仓库 `xmake.lua` 仅 Windows debug 加 `/EHa /ZI`，release 用 `/EHsc`；`thread_error` 构造调用 `_set_se_translator`（MSVC 要求 `/EHa` 才能可靠翻译 SEH）。release 下 `exception_init` 的 SEH/崩溃兜底能力不可假定（`exception_handler.h:26` 注释也提示“示例 target 在 Windows 需 /EHa”）。
- **`from_yaml` 类 warning 会污染 `Errors::isOk()`**：`ContextBase::from_yaml` 把 `this` 作为 `Errors*` 传入，未知键/类型不匹配都会 `setMsg`，随后 `isOk()` 为假（`../config/config_store.h` 的 `__warn`）。配置文件带无关键时不要用 `isOk()` 判定加载成功。
- **`ErrorArgument::what()` 忽略任何描述**：它只有一个 `std::error_code` 成员，`what()` 恒为 `mapArgumentError(ec.value())`（`builder_error.h:100-118`）；需要“具体是哪个名字冲突”时只能靠调用点自行包装文本。
- **`BuildError_E` 的作用域容易写错**：它定义在 `cmd_args::detail`，`cmd_args::detail::BuildError_E::OK` 与（借助 `using`）`cmd_args::BuildError_E::OK` 都可用，但 `detail::BuildError_E` 在 `cmd_args` 外不可见。文档/示例应写 `cmd_args::BuildError_E`。
- **`ErrorValidation` 没有 `std::error_code` 构造**：只有 `(ValidationError_E, std::string&& = {})`（`validation_error.h:88`）。想从已存 `std::error_code` 还原异常时，需按 `.value()` 转成枚举再构造。
- **校验域的默认文案是 `"unknown error"`，其余四域是 `"unknwon error"`**：`validation_error.h:44-53` 与其余 `map*Error` 不一致；统一拼写会改变 `ec.message()` 输出，测试/日志需同步。
- **`Box::print_nested_exception` 可能直接终止进程**：`catch (...)` 分支在无法 `dynamic_cast` 到嵌套 `std::exception` 时 `exit(EXIT_FAILURE)`（`exception_handler.h:130-152`）。库代码里调用它等于接受“打印失败即退出”。
- **`func_util::Optional` 的解引用有运行期异常**：未初始化时 `operator*` 抛 `std::logic_error("is not init")`，不是 UB；但它没有 `std::optional` 的 `has_value` 语义与 `noexcept` 保证，且不受 `cmd_args` 命名空间/构建覆盖。新代码一律用 `std::optional`。
- **`exception_init` 在非 Windows 是空 struct**：`static exception_init g;` 可以无条件编译，但没有任何兜底效果（`exception_handler.h:194-225`）。跨平台代码不要据此声称“崩溃会生成 minidump”。
