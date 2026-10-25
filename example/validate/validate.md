# validate — 必选、取值校验与声明式环境条目

> **摘要**：`validate` 演示 cmd_args 的校验链：选项 `required`、取值范围 `range` 与语义 `semantic`、方位参数必选、`defaultExpr` 环境模板，以及声明式的 `requireEnv` 环境条目（含 `defaultEnv` 兜底）。失败时它打印 `ec.message()` 与 `app.dump()` 并用错误码返回，成功则打印四个校验后的值。
> **联系**：[example 总览](../example.md)｜对照 [once](../once/once.md)（环境一等公民）、[config](../config/config.md)（外部赋值）｜头文件 [validator.h](../../include/cmd_args/base/validator.h)、[validation.h](../../include/cmd_args/expression/validation.h)、[validation_error.h](../../include/cmd_args/error/validation_error.h)、[scanner.h](../../include/cmd_args/expression/scanner.h)｜入口 [cmd_args.hpp](../../include/cmd_args/cmd_args.hpp)｜上级索引 [cmd_args.md](../../include/cmd_args/cmd_args.md)｜使用方 [unit_test.md](../../dfx/unit_test/unit_test.md)｜README [错误处理](../../README_zh.md#错误处理)、[环境变量一等公民](../../README_zh.md#环境变量一等公民)

## truth

- 目录内仅 `main.cpp`（51 行）与 `xmake.lua`；target 名为 `validation`（与目录名 `validate` 不同），`set_default(false)`，无 Windows `/EHa` 追加（2026-10-25 核对）。
- 参数与校验器：
  - `addOption<std::string>("host", "hostname host", "")` 后 `option("host")->required().semantic("hostname")`；
  - `addOption<int>("port", "port number", 0)` 后 `option("port")->range(1024, 65535).defaultExpr("${DEMO_PORT:-8080}")`；
  - `build<ArgumentNamed>("name", "your name")` 后 `argument("name")->required()`；
  - `requireEnv("DB_HOST", "database host")->semantic("hostname").defaultEnv("localhost")`（返回 `EnvSpec*`，链式继续挂校验器）。
- `semantic` 表由 `base/validator.h` 定义：`ipv4`、`ipv6`、`port`、`file`、`path`、`date`、`duration`、`hostname`、`uuid`、`float`、`double`；未知语义抛 `ErrorArgument(UNSUPPORT_TYPE)`。
- `range(a, b)` 是闭区间；`defaultExpr` 把模板串同时写入默认值与当前值并置 `bDefaultExpr__`，在 `compiler.finalize()` 的 `__expandEnvValues()` 阶段展开，再参与校验。
- 校验顺序（`expression/validation.h`）：`checkEnvSpecs`（缺失且 `defaultEnv` 非空则先补全并继续；仍缺失且 required → `MISSING_ENV`；存在则 validate → `VALIDATION_FAILED`）→ `checkRequired`（命名参数 → 方位参数 → 值选项，首错即返）→ `checkValues`（命名参数 → 方位参数 → 选项，首错即返）。
- 错误码（`error/validation_error.h` 的 `ValidationError_E`）：`OK=0`、`MISSING_ARGUMENT=1`、`MISSING_OPTION=2`、`MISSING_ENV=3`、`VALIDATION_FAILED=4`；经 `make_error_code` 注册后，`ec.value()` 即上述数值，`ec.message()` 为对应文本。
- 失败路径：`std::cerr << "(validation error) " << ec.message() << "\n" << app.dump()`，然后 `return static_cast<int>(ec.value())`；不会打印结果值。
- 成功路径打印 4 行：`host`、`port`、`name`、`DB_HOST = get_env_or("DB_HOST", "<defaulted>")`；随后 `return 0`（没有 `once()`）。
- `DEMO_PORT` 未设置时 `defaultExpr` 走 `:-` 兜底得到 `8080`；`DEMO_PORT=80` 时展开为 80 并因 `range(1024, 65535)` 报 `VALIDATION_FAILED`。
- 运行（头注释）：`xmake build validation`；`xmake r validation -- --host localhost --port 9090 --name peter`（成功）；`xmake r validation -- --port 80 --name peter` → `VALIDATION_FAILED`；`xmake r validation -- --name peter`（缺 host）→ `MISSING_OPTION`；完全省略 `name` → `MISSING_ARGUMENT`。
- `app.dump()` 在失败时输出当前模型状态，便于定位哪个参数/选项/环境条目未满足。

## 约束

- 校验器挂在句柄上：必须先 `addOption`/`build`/`requireEnv` 建出对象，再用 `option(...)`/`argument(...)` 取句柄链式调用；不存在“在声明里直接写校验器”的写法。
- `required()` 的判定依据是“是否被赋值/是否命中环境/是否来自外部”，不是“值非空”；因此默认值为空串的 `host` 仍需显式提供。
- `defaultExpr` 只有 `defaultEnv` 那一类“环境兜底”语义不同：前者是值模板（解析后当默认值），后者是环境条目的缺省值来源。
- 不要用含 `.` 的 IPv4 字面量测试 argv 路径：Scanner 会把 `127.0.0.1` 拆成多个 token（既有词法缺陷，见 README 已知限制），示例因此用 `hostname` 语义与整数 `port`。
- `requireEnv` 对已注册的键返回旧条目而不覆盖（`env_builder.h`）；重复声明同一环境键不会叠加校验器。
- target 固定 `validation`、`set_default(false)`；只使用公共 API；源文件 UTF-8 无 BOM。

## 索引

| 文件 | 关键符号 | 职责 |
| --- | --- | --- |
| `main.cpp` | `addOption<std::string>("host", ...)`、`option("host")->required().semantic("hostname")`、`addOption<int>("port", ...)`、`range(1024, 65535)`、`defaultExpr("${DEMO_PORT:-8080}")`、`build<ArgumentNamed>`、`argument("name")->required()`、`requireEnv("DB_HOST", ...)`、`semantic("hostname")`、`defaultEnv("localhost")`、`parse(ec, ...)`、`dump()` | 构筑校验模型、解析、打印结果或诊断 |
| `xmake.lua` | `target("validation")`、`set_kind("binary")`、`set_symbols("debug")`、`set_default(false)` | 构建 validation 可执行文件，默认不构建 |

## 决策

- 用 `hostname` 而不是 `ipv4` 演示语义校验：避开 Scanner 点号缺陷，示例在 argv 路径下可复现；IPv4/IPv6 语义由单测 `[validator]` 覆盖。
- `port` 同时挂 `range` 与 `defaultExpr`：一个示例同时展示“默认值来自环境模板”和“范围校验”，两条路径都可单独触发。
- `name` 用方位参数并设 `required`：补齐“命名参数 / 方位参数 / 选项 / 环境条目”四类对象的必选语义。
- 失败时打印 `dump()`：比只打错误码更有诊断价值；代价是输出较长，脚本化使用时应只取退出码。
- 被否方案：在示例里手工 catch `ErrorValidation` 打印。示例要演示 `error_code` 通道与退出码，异常文本已由 `ec.message()` 提供。

## 教训

- 校验顺序决定了报哪个错：环境条目先于必需选项检查，所以同时缺 `DB_HOST` 与 `--host` 时可能先看到 `MISSING_ENV`；排查时按“环境 → 必选 → 取值”顺序看。
- `defaultEnv` 会在校验前把缺省值写回会话环境（`setEnv`），因此成功后 `get_env_or("DB_HOST", ...)` 拿到 `localhost` 而不是 `"<defaulted>"`；`"<defaulted>"` 只在完全没有兜底时出现。
- Scanner 点号缺陷：`expression/scanner.h` 的 `__processInteger` 遇 `.` 进入实数状态，`__processReal` 遇非数字立即结束且不消费当前字符，`"127.0.0.1"` 被切成多个 token，`lexer.h` 的 `__normalize` 也不重新合并；这是 README 已列的已知限制，不要在示例里绕过后当成已修复。
- `range` 是闭区间，边界值 1024/65535 合法；写成开区间预期会误判。
- `defaultExpr` 在 finalize 展开，若模板里的环境变量未设置且没有 `:-` 兜底，得到的是空串，随后可能触发类型/范围错误——模板务必带兜底。
- target 名与目录名不一致（`validation` vs `validate`）：`xmake r validate` 会失败，必须用 `xmake r validation`。
- 行号会漂移，本文只锚定文件与符号。

