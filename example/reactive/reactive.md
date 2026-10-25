# reactive — 响应式数据：Property 读写拦截与变更通知

> **摘要**：`example/reactive` 用最小可执行程序演示参数值的响应式拦截：`option("port")` 的写钩子做准入，`argument("name")` 的写钩子做原地规范化，`option("level")` 的读钩子提供默认值，`onChange` / `version()` 观察变更。它不定义新的库能力，只展示 [property.h](../../include/cmd_args/property/property.h) 的公共契约在 CLI 场景中的四条通道：解析写入、业务写入、旁路写入、重置。
> **联系**：上级文档 [example.md](../example.md)｜属性载体 [property.md](../../include/cmd_args/property/property.md)｜持有方 [argument.md](../../include/cmd_args/argument/argument.md)、[option.md](../../include/cmd_args/option/option.md)｜错误通道 [validation_error.h](../../include/cmd_args/error/validation_error.h)｜使用方对照 [unit_test.md](../../dfx/unit_test/unit_test.md) 中的 test_property｜README 见 [响应式数据：Property 属性](../../README_zh.md)。

## truth

- 目录构成（2026-10-25 核对）：`main.cpp`（172 行）+ `xmake.lua`（7 行）。target 名 `reactive`，`set_default(false)`，Windows 分支无 `/EHa`。
- 程序骨架：`Context app("reactive_demo")`（main.cpp:63）→ 注册参数与选项并挂钩子 → `app.parse(ec, _Argc, const_cast<char const **>(_Argv), _Envp)`（main.cpp:119）→ 读取并打印 → `return 0`。
- 四个演示点：
  1. 写拦截（准入）：`app.addOption<int>("port", "listening port", 8080)`（main.cpp:66）挂 `.onWrite(&checkPort)`（main.cpp:68）；`checkPort`（main.cpp:30-49）只接受纯数字且 1..65535，拒绝消息形如 `端口超出范围 1..65535：70000`。
  2. 写钩子兼做规范化：`argument("name")` 的钩子去首尾空白并转大写，直接改候选值（main.cpp:75-97）；去空白后为空则返回 `name 不能为空（去空白后）`。
  3. 读拦截（默认值）：`option("level")` 默认 `""`（main.cpp:100），`onRead` 空值→`info`（main.cpp:114-116）；写钩子把值归一为 debug/info/warn/error 小写（main.cpp:102-113）。
  4. 变更通知：两个 `onChange` 打印 `[onChange] <字段> : old -> new`（main.cpp:69-71、main.cpp:95-97）；`version()` 只在存储值真的变化时自增（main.cpp:134、main.cpp:141）。
- 四条写入/拒绝通道（与 property.md 的写入路径矩阵一致）：
  * `Property::set` 拒绝时返回 false，原因在 `lastError()`，值/版本不动（main.cpp:139-142）。
  * `OptionValue::setValue` 拒绝时抛 `ErrorValidation(ValidationError_E::VALIDATION_FAILED)`，消息 `option --port: 端口必须是十进制整数：abc`（main.cpp:145-150）。
  * CLI 解析期拒绝由 `parse(ec)` 收进 `ec`，`ec.value() == 4`；`ec.message()` 只有类别文案 `value validation failed`，具体原因仍在 `property().lastError()`（main.cpp:119-129）。
  * `setRaw` 旁路写钩子（main.cpp:163-165）；`reset` 回默认值并清 `isSet`（main.cpp:168-171）。
- 实测输出（2026-10-25，Windows x64 debug，直接运行 `build/windows/x64/debug/reactive.exe`）：
  * `--name alice`：`level  = info (raw = [], version 0)`——读钩子生效、存储保持空串。
  * `--name alice --port 9090 --level debug`：`port   = 9090 (version 1, isSet true)`；`port.set("70000") -> rejected / 端口超出范围 1..65535：70000`；`port after reject = 9090 (version 1)`；`name   = ALICE SMITH`。
  * `--name alice --port 70000`：打印 `(write rejected at parse) value validation failed`、`lastError = 端口超出范围 1..65535：70000`、`app.dump()`，退出码 4。
- 构建与运行：`xmake build reactive`；`xmake r reactive -- --name alice --port 9090 --level debug`（`--` 之后的 token 原样传给程序）。

## 约束

- 只用公共 API：仅 `#include "cmd_args/cmd_args.hpp"`（main.cpp:25），不 include 内部头、不触碰私有成员。
- 钩子必须在 `addOption` / `build` 之后、`parse` 之前注册：`option()` 返回的指针在解析期可能因 `ValueOptMgr` 扩容而失效（见 [property.md](../../include/cmd_args/property/property.md) 教训）。
- 写钩子空串通过、非空拒绝；钩子里的异常会穿过 `set` 直达解析层，示例只返回消息、不抛异常。
- 读钩子只做取值变换，不得改存储、不得重入 `set` / `setRaw`；示例以 `raw()` 仍为空串作为该约束的可观察证据（main.cpp:158-159）。
- `setRaw` 只演示受控来源旁路；业务写入必须走 `set` / `setValue`，否则准入检查形同虚设。
- target 必须保留 `set_default(false)`；示例不进入默认构建，也不进入发布产物。
- 源码 UTF-8 无 BOM；Windows 下依赖根 `xmake.lua` 的 `/utf-8` 才能正确编译中文字符串。

## 索引

| 文件 | 关键符号 | 职责 |
| --- | --- | --- |
| `main.cpp` | `checkPort` / `display` / `onWrite` / `onRead` / `onChange` / `set` / `setValue` / `setRaw` / `reset` / `lastError` / `version` | Property 读写拦截、变更通知与四条写入通道的最小演示 |
| `xmake.lua` | `target("reactive")` / `set_default(false)` | reactive 构建描述（无 `/EHa`） |

## 决策

- 用 option 与 argument 两种持有方各演示一类钩子：`port` / `level` 是 `OptionValue`，`name` 是 `Argument`，说明拦截点与参数种类无关；被否方案是只做一个选项，无法同时展示两套拒绝消息前缀（`option --port: ` 与 `argument name: `）。
- 准入检查写成自由函数 `checkPort(std::string &)` 而非 lambda：钩子签名是 `std::function<std::string(Value_T &)>`，自由函数可直接取地址（`&checkPort`），读者对照契约更直接；代价是多一个匿名命名空间函数。
- 以 `setRaw` + `reset` 收尾而不是再演示一次拒绝：展示“受控来源旁路”与“回到默认值”两条不同通道，避免重复打印同类拒绝。
- 错误分支同时打印 `ec.message()` 与 `lastError()`：`ec` 侧只有错误类别文案，具体拒绝原因只能在属性侧取；只打印 `ec` 会让读者误以为拒绝消息丢失。
- 不把 `required()` / `range()` 等 `ValidatorMixin` 校验器接进示例：它们在 parse 之后的统一校验阶段运行，与写入拦截是两条通道（详见 [property.md](../../include/cmd_args/property/property.md) 决策），混在一起会掩盖拦截发生时机。

## 教训

- `${VAR}` 展开曾把读钩子的默认值物化：原实现 `__expandEnvValues` 用 `o.getValue()`（过读钩子）取值后 `setRaw` 写回，导致 `level` 的 `info` 被写进存储、`version()` 变 1。现改为以 `property().raw()` 为展开源（compiler.h:275-290、interpreter.h:500-514），示例才稳定打印 `raw = []`。
- `getArgument<std::string>` / `getOption<std::string>` 只取首个空白分隔 token：`util::parse_value` 用 `std::istringstream >> T`（util.h:107-115），多词值 `ALICE SMITH` 会被截成 `ALICE`。多词值请读 `property().get()` / `raw()`；示例的 `name` 打印因此改用属性接口（main.cpp:152-156）。
- `ec` 通道不保留异常消息文本：`ContextBase::parse(std::error_code&, ...)` 只做 `_Ec = e.code()`，`ec.message()` 是类别文案 `value validation failed`；详情要么读 `Property::lastError()`，要么用抛异常版 `parse`（context_base.h:412-426）。
- 写钩子的 `Value_T &` 可以就地改值，但拒绝时不要留下部分修改：`checkPort` 只读不写，规范化钩子先构造临时串、校验通过后整体赋值（main.cpp:85-92）。
- `app.dump()` 也只显示错误类别文案（`0 : value validation failed`），不要指望 dump 给出具体拒绝原因。
