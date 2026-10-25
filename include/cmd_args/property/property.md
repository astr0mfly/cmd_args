# property — 参数值的属性载体与读写拦截点

> **摘要**：property 目录只有一件东西：模板 `Property<T>`。它是参数/选项值的唯一存储，也是“响应式数据”的拦截点——写钩子 `onWrite` 做准入与原地规范化，读钩子 `onRead` 做取值变换与默认值拦截，变更回调 `onChange` 与修订号 `version()` 支撑下游失效传播。`Argument` 与 `OptionValue` 各私有持有一个 `Property<std::string>`，经 `property()` 暴露、经 `set` / `setValue` / `getValue` 生效；它不做解析、不做统一校验，也不跨对象广播。
> **联系**：上级索引 [cmd_args.md](../cmd_args.md)；持有方 [argument.md](../argument/argument.md)、[option.md](../option/option.md)；错误通道 [validation_error.h](../error/validation_error.h)；接线点 [compiler.h](../expression/compiler.h)、[interpreter.h](../expression/interpreter.h)、[validation.h](../expression/validation.h)、[config_store.h](../config/config_store.h)；单测 [test_property.cpp](../../../dfx/unit_test/test_property.cpp)；README 见 [环境变量一等公民](../../../README_zh.md#环境变量一等公民) 与 [配置脚本（YAML）：第三类外界环境](../../../README_zh.md#配置脚本yaml第三类外界环境)。

## truth

**文件与定位**（2026-10-25 核对）：目录内只有 `property.h`（229 行，header-only），定义模板 `Property<T>`。命名空间为 `cmd_args::property_detail`，随后 `using property_detail::Property;` 导出（property.h:45、225-229），外部一律写 `cmd_args::Property<T>`。

**对外签名**（property.h:52-197 逐字抄写）：

```cpp
template <typename T>
class Property {
public:
    using Self_T      = Property<T>;
    using Value_T     = T;
    using FnWrite_T   = std::function<std::string(Value_T &)>;                 // 返回空串接受；非空串拒绝
    using FnRead_T    = std::function<Value_T(Value_T const &)>;               // 取值变换，不得抛
    using FnChange_T  = std::function<void(Value_T const &, Value_T const &)>; // (old, new)，不得抛
    using FnDefault_T = std::function<Value_T()>;                              // 默认值惰性求值

    explicit Property(Value_T _Initial = {});

    bool set(Value_T _Value);                        // 过写钩子；false = 被拒绝（值/版本/回调不动）
    auto setRaw(Value_T _Value) -> Self_T &;         // 跳过写钩子（受控来源专用）
    auto get() const -> Value_T;                     // 过读钩子链；不改存储、不涨版本
    auto raw() const noexcept -> Value_T const &;    // 原始值

    bool isSet() const noexcept;
    auto version() const noexcept -> std::uint64_t;
    auto lastError() const noexcept -> std::string const &;
    auto clearError() noexcept -> Self_T &;

    auto defaultValue() const noexcept -> Value_T const &;
    auto defaultValue(Value_T _Value) -> Self_T &;
    auto defaultFn(FnDefault_T _Fn) -> Self_T &;
    auto clearDefaultFn() noexcept -> Self_T &;
    auto reset() -> Self_T &;

    auto onWrite(FnWrite_T _Fn) & -> Self_T &;    auto onWrite(FnWrite_T _Fn) && -> Self_T;
    auto onRead(FnRead_T _Fn) & -> Self_T &;      auto onRead(FnRead_T _Fn) && -> Self_T;
    auto onChange(FnChange_T _Fn) & -> Self_T &;  auto onChange(FnChange_T _Fn) && -> Self_T;
};
```

**状态机**（property.h:60-159、200-212）：

1. 构造：`tValue__ = _Initial`，`tDefault__` 同步为同值；`bSet__ = false`、`uVersion__ = 0`。
2. `set(v)`：按注册顺序跑 `vecWrite__`；首个返回非空消息的钩子 → `strLastError__ = msg`、返回 `false`，值/版本/变更回调全部不动；全部通过 → 清错、`__apply(v, true)`。
3. `__apply(v, markSet)`：仅当 `tValue__ != v` 时换值、`++uVersion__`、按注册顺序回调 `vecChange__(old, new)`；`markSet` 为真时置 `bSet__ = true`。
4. `setRaw(v)`：跳过写钩子直接 `__apply(v, true)`，且**不清** `lastError()`。
5. `get()`：拷贝 `tValue__` 后依次过 `vecRead__`，返回变换结果；不写存储、不涨版本。
6. `defaultValue(v)` / `defaultFn(f)`：仅在 `!bSet__` 时同步当前值（“读默认值”语义）；`defaultFn` 保存后立即求值一次；`clearDefaultFn()` 只清函数、保留当前默认值。
7. `reset()`：若挂了 `fnDefault__` 先重算 `tDefault__` → 清错 → `__apply(tDefault__, false)` → `bSet__ = false`。

**写入路径矩阵**（是否经过写钩子）：

| 写入来源 | 过写钩子 | 依据 |
| --- | --- | --- |
| CLI 解析（长/短选项、命名/方位参数） | 是 | option_parser.h:531-543 经 `setValue`；argument_parser.h 各赋值点经 `set` |
| 业务代码 `Argument::set` / `OptionValue::setValue` | 是 | argument.h:194-208 / option.h:238-245 |
| env 兜底 seed（`__seedEnvDefaults`） | 是 | compiler.h:256-273、interpreter.h:486-499 |
| `${VAR}` 展开（`__expandEnvValues`） | 否（`setRaw`） | compiler.h:275-290、interpreter.h:501-513 |
| YAML / 快照 / `setExternal` / `load_config` | 否（`setRaw`） | argument.h:217-234、option.h:157-172、config_store.h |
| 默认值同步 / `reset` | 否（`__apply`） | property.h:126-159 |

**拒绝的错误通道**：`Property` 自身不抛异常，只把消息留在 `lastError()`。持有方负责转成 `ErrorValidation(ValidationError_E::VALIDATION_FAILED, ...)`，消息前缀分别为 `argument <name>: `（argument.h:373-379）与 `option --<longName>: `（option.h:238-245）。因此写拒绝在两条通道都可见：`set`/`setValue` 直接抛；解析与 `loop` 由 `ContextBase::parse` 四个重载与 `context_step.h` 的 `catch (ErrorValidation)` 收进 `Errors` 堆栈并返回 `ec`。

**与统一校验的关系**：`validate_detail::validate`（validation.h:156-163）仍然是 env spec → required → values 的顺序与唯一聚合点；`Property` 不参与它，`.check()` / `.range()` / `.oneOf()` 等 `ValidatorMixin` 校验器也不会自动变成写钩子。要“写入即校验”，由使用者在 `onWrite` 里自行调用 `Validator::validate` 并返回其消息。

## 约束

- header-only：实现全部写在 `property.h`，禁止新增 `.cpp` 编译单元（README_zh.md:3）。
- C++20（xmake.lua:11）；改动须同时满足 Windows / Linux。
- 命名空间必须经 `CMD_ARGS_NAMESPACE_BEGIN` / `CMD_ARGS_NAMESPACE_END`，实现放 `property_detail`，只用 `using` 导出 `Property`（property.h:28、45、225-229）。
- `T` 必须可默认构造、可拷贝、支持 `!=`（`__apply` 以不等判定是否变化）；当前持有方统一用 `std::string`。
- 写钩子以 `Value_T &` 取得候选值、返回 `std::string`：空串通过、非空拒绝；钩子抛出的异常会穿过 `set` 直达解析层，不得抛。
- 读钩子与变更回调不得抛异常、不得对同一 `Property` 重入 `set` / `setRaw`（property.h:39-43 注释即契约）。
- `setRaw` 只允许受控来源使用：`setExternal`、配置/快照导入、`${VAR}` 展开、默认值同步；业务写入必须走 `set`，否则准入检查形同虚设。
- 拒绝消息由持有方拼前缀并抛 `ErrorValidation`；禁止在 `Property` 内抛（保持错误通道单一、`noexcept` 边界清晰）。
- 非线程安全；不要跨线程共享同一个 `Context`。

## 索引

| 文件 | 关键符号 | 职责 |
| --- | --- | --- |
| `property.h` | `Property` / `set` / `setRaw` / `get` / `raw` / `isSet` / `version` / `lastError` / `clearError` / `defaultValue` / `defaultFn` / `clearDefaultFn` / `reset` / `onWrite` / `onRead` / `onChange` | `Property<T>` 模板：值 + 默认值 + 写/读/变更钩子链 + 修订号 |

grep 锚点（2026-10-25 核对）：`grep -n "class Property" include/cmd_args/property/property.h` 命中 50；`grep -n "Property<Value_T> propValue__" include/cmd_args/argument/argument.h include/cmd_args/option/option.h` 命中 argument.h:384 / option.h:254；`grep -n "auto property()" include/cmd_args/argument/argument.h` 命中 311 / 314；`grep -n "property().setRaw" include/cmd_args/expression/compiler.h` 命中 280 / 284 / 288。行号只作稳定锚点，重构后需重新核对。

## 决策

- 用 `std::function` 钩子链而非派生子类：同一参数可叠加多个写/读检查，注册顺序即执行顺序；代价是每个 `Property` 三个 `std::vector` 与间接调用开销。
- 写钩子签名 `std::string(Value_T &)`（而非 `bool`）：非空消息直接就是拒绝理由，可原样进入 `lastError()` 与最终错误串；`Value_T &` 允许就地规范化（trim/归一）。代价是钩子能改值，需在注释里写明契约。
- 拒绝立即抛 `ErrorValidation(VALIDATION_FAILED)` 而不是延迟到统一校验：错误发生在写入点，`ec` 通道与异常通道天然一致，且解析中途不会留下“半合法”值；代价是 `set` / `setValue` / `setExternal` 不再 `noexcept`，既有 `noexcept` 假设随之清理（argument.h、option.h 已去掉）。
- `${VAR}` 展开与 YAML / `setExternal` 走 `setRaw` 旁路：展开是派生值，不应再次触发准入，也不应覆盖上一条拒绝消息（compiler.h:279 注释写明理由）；代价是这些路径不受写钩子保护，约束节因此限定其来源。
- 现有 `ValidatorMixin`（required/check/range/oneOf/match/semantic）**不**自动挂写钩子：保留“解析完统一校验”的错误聚合与既有顺序（env → required → values），避免同一错误在两条路径重复报告；代价是两套校验路径并存，文档需给出 `onWrite` 内调用 `Validator` 的配方。
- 默认值仍留在 `Property` 内（`tDefault__` + 可选 `fnDefault__`），没有迁到 `EnvData`：本期只交付“读写拦截点”，以环境变量为唯一存储是后续议题。
- 破坏性取舍：删除 `OptionValue::strValue` 公有字段，统一为 `getValue()` / `setValue()` / `property()`。理由是不再存在“绕过钩子的公有值存储”；代价是既有源码级使用方必须迁移（测试与内部点位已全部改完，2026-10-25 核对）。

## 教训

- 解析期懒注册内置选项会让 `ValueOptMgr` 的 `std::vector` 扩容（option_data.h:85），跨 `parse()` 持有 `OptionValue*` 会悬垂；`property()` 返回的引用同样失效——钩子必须在 `build` / `addOption` 之后、`parse` 之前注册。单测曾因此触发 bad allocation。
- 拷贝/移动 `Property` 会连钩子一起搬（property.h:42-43 注释）：钩子若捕获 `this`，搬完后指向旧实例。规避：捕获外部状态而非对象地址；`Argument` 存于 `std::map` 指针稳定，`OptionValue` 存于 `std::vector` 则不稳定。
- `set` 与 `setRaw` 语义不同：前者过钩子、能拒绝、清错；后者静默写入、保留旧错误。混用会让准入检查被完全绕过（compiler.h:280/284 的展开路径是有意旁路）。
- `OptionValue::strValue` 曾被测试直接赋值（原 test_model.cpp:92），本次改为 `setValue`：公有字段删除属破坏性变更，升级方需全量替换为 `getValue()` / `setValue()`。
- `defaultValue(T)` 与 `defaultFn` 都带 `!bSet__` 守护：显式赋值过就不再被默认值覆盖；这与旧 `defaultExpr` “无条件覆盖值”的行为不同，改为 `defaultExpr` = `clearDefaultFn` + `defaultValue(_Expr)` + `reset()`（argument.h:333-340）。
- 文档漂移：旧 `argument.md` / `option.md` 记录 `strValue__` / `strDefault__` / `fnDefault__` 与 mutable `strValue`，`option.h:176` 的 `reset` 注释也还写着 `strValue`——Property 接入后全部失效（2026-10-25 已同步）。
- 读钩子返回空值不等于未设置：`hasValue()` / `operator bool` 用 `raw()` 判定，`getValue()` 可能返回钩子造出的非空串（如 `(anonymous)`）；判断“有没有值”只能查 `raw()` / `isSet()` / `isSatisfied()`。
