# once — once 执行模式、环境变量与选项 action

> **摘要**：`once` 演示 once 执行模式下的完整能力面：命名/方位参数、值选项与标志、环境变量绑定与 `set_env`/`env_string`/`expand_env`/`apply_env`、内置选项 `-?`/`-!`/`--env-set`/`--env-unset`/`--env-apply`、选项 `action` 以及 `parse(error_code&)` 非异常错误通道，最后用 `once()` 以退出码退出。它还单独 include `exception_handler.h`，展示可选的崩溃兜底（minidump）。
> **联系**：[example 总览](../example.md)｜对照 [config](../config/config.md)（YAML 与环境）、[validate](../validate/validate.md)（校验）｜头文件 [context.h](../../include/cmd_args/context/context.h)、[builtin_options.h](../../include/cmd_args/context/builtin_options.h)、[exception_handler.h](../../include/cmd_args/error/exception_handler.h)、[option.h](../../include/cmd_args/option/option.h)｜入口 [cmd_args.hpp](../../include/cmd_args/cmd_args.hpp)｜上级索引 [cmd_args.md](../../include/cmd_args/cmd_args.md)｜使用方 [unit_test.md](../../dfx/unit_test/unit_test.md)｜README [执行模式](../../README_zh.md#执行模式)、[环境变量一等公民](../../README_zh.md#环境变量一等公民)、[选项 action](../../README_zh.md#选项-action-system_call-子进程执行)

## truth

- 目录内仅 `main.cpp`（121 行）与 `xmake.lua`；target 名为 `once`，`set_default(false)`，Windows 下追加 `/EHa`。`once/xmake.lua` 的注释说明原因：airbag 的 `thread_error` 使用 `_set_se_translator()`，MSVC 需要 `/EHa` 才能编译。
- 头文件包含：`"cmd_args/cmd_args.hpp"` 与 `"cmd_args/error/exception_handler.h"`；后者不在 `cmd_args.hpp` 内，必须单独 include。
- 文件作用域静态对象 `static exception_init sg_exception_guard;`：`exception_init` 的构造函数注册 airbag 钩子（纯虚调用/未捕获异常/系统故障时生成 minidump，仅 Windows/MSVC 生效）；`main` 首行 `(void)sg_exception_guard;` 只为消除未使用告警，确保对象在 `main` 前构造。
- 参数模型：命名 `hello`/`world`；方位 `name`/`count`；`addOption<std::string>('o', "output", "output file path", "out.txt")`、`addOption<int>("timeout", "timeout in seconds", 30)`、`addFlag('v', "verbose", ...)`。
- 选项 action：`addOption<std::string>("notify", "run echo with the value as a system call", "")` 之后 `demo.option("notify")->action("echo notify: {value}")`。绑定经 `option("name")` 句柄完成，`{value}` 原样替换为本次命中的值；命令最终由 `std::system` 在子进程执行（Windows `cmd.exe`、POSIX `sh`）。
- action 触发条件：仅 CLI 显式命中；环境变量兜底、YAML `setExternal`、内置默认值都不触发。批量 `parse()` 路径下，用户 action 在 `compiler.finalize()` 之后由 Executor 统一执行，顺序等于命令行出现顺序；某条命令返回非 0 只向 stderr 告警，不中断解析。`action` 不进入 `schema`/`to_yaml`。
- 环境联动：`option("output")->env("CMD_ARGS_DEMO_OUTPUT")`、`option("timeout")->env("CMD_ARGS_DEMO_TIMEOUT")`、`argument("world")->env("CMD_ARGS_DEMO_WORLD")`，并在 parse 前 `set_env("CMD_ARGS_DEMO_WORLD", "env-says-hi")`；因此未显式给 `world` 时取会话环境值。
- 解析：`std::error_code ec; demo.parse(ec, _Argc, const_cast<char const **>(_Argv), _Envp);` 失败打印 `"(parse error) " << ec.message()` 并 `return static_cast<int>(ec.value())`（此时不会走到 `once()`）。
- 成功路径行为：打印 `"--- values ---"` 与 hello/world/name/count/output/timeout/verbose/notify；打印 `"--- environment snapshot ---"` 与 `env_string()`；打印 `get_env_or("CMD_ARGS_DEMO_WORLD", "<missing>")` 与 `expand_env("${CMD_ARGS_DEMO_WORLD}!")`；`set_env("CMD_ARGS_DEMO_NEW", "created-in-session")` 后再打印一次快照；`auto applied = demo.apply_env()` 并打印回写条数；最后 `demo.once()`。
- `Context::once()` 实现：未解析时抛 `ErrorInterp(InterpError_E::STOPPED)`；已解析则 `std::exit(nExitCode_)`（默认 0）。它只负责按退出码退出，不负责解析——示例的解析已由 `parse()` 完成。
- 内置选项（`context/builtin_options.h`）：`-?`/`--help` 打印帮助；`-!`/`--env` 打印环境快照；`--env-set KEY=VALUE` 写入会话环境；`--env-unset KEY` 删除；`--env-apply` 回写 OS 环境并逐条打印 `applied ...`。它们在首次 parse 时懒注册，且不进入 `schema`/`to_yaml`。
- `expand_env` 支持 `${NAME}`、`$NAME`、`%NAME%` 与 `${NAME:-fallback}` 形式；`apply_env()` 通过 `_putenv_s`（Windows）/`setenv`（POSIX）回写并返回条数。
- 运行命令（头注释）：`xmake r once -- --hello hi --timeout 99 -v peter 3`；`xmake r once -- -!`；`xmake r once -- -?`；`xmake r once -- --env-set=A=1 --env-apply`；`xmake r once -- --notify "deploy done"`。

## 约束

- `once()` 只能在成功 `parse()` 之后调用；未解析时抛 `ErrorInterp(STOPPED)`，且该异常不会被 `parse(ec)` 的错误码通道吸收。
- `once()` 使用 `std::exit`，会跳过自动对象析构与 `main` 的 `return`；需要清理的资源必须在调用前释放，或不要用 `once()`。
- `exception_handler.h` 是独立可选头，不能假定 `cmd_args.hpp` 已包含；其崩溃兜底只在 Windows/MSVC 生效，其他平台不要依赖 minidump。
- Windows 下必须保留 `/EHa`；缺少它时 `_set_se_translator` 相关代码无法按预期编译/工作。
- 内置选项一旦在首次 `parse()` 注册就不可撤销；若要关闭，须在首次 parse 前调用 `set_builtin_options(false)`。
- `--env-set`/`--env-apply`/`apply_env()` 会修改进程环境，属于真实副作用；示例只应在受控终端运行。
- target 固定 `once`、`set_default(false)`；只使用公共 API；源文件 UTF-8 无 BOM。

## 索引

| 文件 | 关键符号 | 职责 |
| --- | --- | --- |
| `main.cpp` | `exception_init sg_exception_guard`、`build<ArgumentNamed>`、`build<ArgumentPosition>`、`addOption<std::string>`、`addOption<int>`、`addFlag`、`option("notify")->action(...)`、`env(...)`、`set_env`、`env_string`、`get_env_or`、`expand_env`、`apply_env`、`parse(ec, ...)`、`once()` | 构筑、action 与环境绑定、解析、环境交互、退出 |
| `xmake.lua` | `target("once")`、`set_kind("binary")`、`set_symbols("debug")`、`set_default(false)`、Windows `/EHa`（注释说明 airbag 原因） | 构建 once 可执行文件，默认不构建 |

## 决策

- 用一次 `parse()` + `once()` 展示“构筑 → 解析 → 展示 → 退出”的完整生命周期；`loop()`（REPL）不在本示例范围，避免把一次性示例写成交互程序。
- 单独 include `exception_handler.h` 并在文件作用域放静态守卫：演示崩溃兜底是 opt-in 能力，且不污染 `cmd_args.hpp` 的默认依赖；`(void)` 转换明确表示“只为构造”。
- action 选 `echo notify: {value}`：跨平台（`std::system` 两侧都有 `echo`），又能展示 `{value}` 原样替换与子进程输出。
- 在 `parse` 前 `set_env` `world`：展示“环境变量作为兜底”的语义，同时避免依赖调用者终端里是否已导出该变量。
- 调用 `apply_env()` 展示回写 OS 环境的能力；代价是示例有真实副作用，故文档标注只在受控终端运行。
- 被否方案：用 try/catch 解析异常。示例专门演示 `error_code` 通道，异常通道由 README 与其他用例覆盖。

## 教训

- `-?`/`-!` 不会终止进程：短回路回调打印帮助/环境快照后，`parse` 正常返回，`main` 继续打印值、执行 `apply_env()`，最后才由 `once()` 退出；期望“只看帮助就结束”的脚本会多出后续输出。
- `main.cpp` 的头注释说 action “解析时命中即按命令行顺序执行”；批量 `parse()` 路径的实际时序是 finalize 之后由 Executor 统一执行（见 README“选项 action”）。顺序仍然按命令行，但“立即”容易误解为解析中途；若校验失败，finalize 抛错，action 根本不会执行。
- `once()` 的 `std::exit` 会跳过析构；任何依赖 RAII 的清理都不会发生，别把 flush/close 放在 `once()` 之后。
- 内置选项在首次 parse 时懒注册；测试里想关闭 `--help` 等必须在首次 parse 之前调用 `set_builtin_options(false)`，之后注册不可撤销。
- `env_string()` 是会话环境表的快照，不是 OS 全量环境；`apply_env()` 返回的是本次回写条数，不是环境变量总数。
- `--env-set=A=1` 会把 `A=1` 写入会话环境，`--env-apply` 再回写 OS 并打印；随后 `main` 的 `apply_env()` 可能再次回写其他键，输出条数不要按“一次”预期。
- `exception_init` 的守卫在非 Windows 平台不产生 minidump；不要把它当作跨平台崩溃恢复机制。
- 行号会漂移，本文只锚定文件与符号。

