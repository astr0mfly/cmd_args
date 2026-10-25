# config — 命令行 / 环境变量 / YAML 三类输入源的往返

> **摘要**：`config` 在同一程序里演示三类输入源——argv、环境变量、YAML 配置脚本，并用 `save_config()`/`load_config()`（以及内置 `--save-config`/`--load-config`）展示参数与环境如何落盘、如何反哺。示例通过 `reset()` 后重新加载来模拟一次全新调用，最后用 `once()` 以退出码退出。
> **联系**：[example 总览](../example.md)｜对照 [once](../once/once.md)（环境变量与执行模式）、[validate](../validate/validate.md)（校验）、[chain](../chain/chain.md)（链式）｜头文件 [config_store.h](../../include/cmd_args/config/config_store.h)、[context_base.h](../../include/cmd_args/context/context_base.h)、[builtin_options.h](../../include/cmd_args/context/builtin_options.h)｜入口 [cmd_args.hpp](../../include/cmd_args/cmd_args.hpp)｜上级索引 [cmd_args.md](../../include/cmd_args/cmd_args.md)｜使用方 [unit_test.md](../../dfx/unit_test/unit_test.md)｜README [配置脚本（YAML）](../../README_zh.md#配置脚本yaml第三类外界环境)

## truth

- 目录内仅 `main.cpp`（91 行）与 `xmake.lua`；target 名为 `config_demo`（目录名不是 target 名），`set_default(false)`，Windows 下没有 `/EHa` 分支。
- 参数模型：命名参数 `hello`、`world`；方位参数 `name`、`count`；`addOption<std::string>('o', "output", "output file path", "out.txt")`；`addOption<int>("timeout", "timeout in seconds", 30)`；`addFlag('v', "verbose", ...)`。
- 环境变量绑定与预置：`option("output")->env("CMD_ARGS_DEMO_OUTPUT")`、`option("timeout")->env("CMD_ARGS_DEMO_TIMEOUT")`、`argument("world")->env("CMD_ARGS_DEMO_WORLD")`；随后 `set_env("CMD_ARGS_DEMO_WORLD", "env-says-hi")`，因此未在 CLI 给出 `world` 时取该会话环境值。
- 运行主流程（`main`）：
  1. 打印 `"===== schema ====="` 与 `app.schema_string()`；
  2. `parse(ec, ...)`，失败打印 `"(parse error) " + ec.message()` 并 `return ec.value()`；
  3. 打印 `"===== values ====="` 与 7 个字段；
  4. 打印 `"===== to_yaml() ====="` 与 `app.to_yaml()`；
  5. `save_config("demo.yaml")`，成功打印 `"saved demo.yaml"`，失败向 stderr 打印并 `return 1`；
  6. `app.reset()` 后 `load_config("demo.yaml")`，失败打印并 `return 1`；
  7. 打印反哺后的 `env_string()`、`timeout`、`world`，然后 `app.once()`。
- `save_config`/`load_config` 均返回 `bool`：前者是 `config::write_file` 的结果，后者在文件读不到或 YAML 解析失败时返回 false 并把消息写入 `Errors`（ContextBase）。
- YAML 采用扁平点号分区键（`config/config_store.h`）：命名参数 → `arguments.<name>`，方位参数 → `positionals.<name>`，值选项 → `options.<name>`，标志 → `flags.<name>`，环境变量 → `environment.<KEY>`；短回路/内置选项不进入配置。`timeout` 是值选项，所以它落在 `options.timeout`，不是 `arguments.timeout`。
- `to_yaml()` 先写两行注释 `"# cmd_args config for: <program>"`、`"# format: yaml v1"`，再按 schema 条目写键，并只导出“相关环境变量”：store 中被引用/修改/新建的键，加上 schema 中 `.env()` 绑定且当前 store 里存在的键。
- `from_yaml()` 的优先级实现：`environment.*` 直接 `setEnv`；`arguments.*`/`positionals.*` 在目标已有值（CLI 已赋值）时跳过，否则 `setExternal`；`options.*`/`flags.*` 在 `bExplicit__` 为真时跳过，否则 `setExternal`。因此 CLI 显式值 > YAML 外部值；而 YAML 外部值又会阻止 finalize 阶段的环境变量兜底，故 YAML > 环境变量 > 默认值。
- `reset()` 只还原 Argument/Option 的模型数据并清理最近错误，环境变量会话表保留；示例用它模拟“下一次调用”，但不是真正的新进程。
- 第二条命令 `xmake r config_demo -- --load-config demo.yaml --hello cli-hi` 会加载两次 YAML：一次是内置 `--load-config` 在解析期内联执行，一次是 `main` 在 `reset()` 之后的 `load_config`；中间的 `save_config` 已把 `demo.yaml` 覆盖为本次 CLI 值。
- 成功路径以 `app.once()` 结束，其实现是 `std::exit(nExitCode_)`（`context/context.h`），因此 `main` 末尾的 `return 0` 不会执行；解析失败路径在 `once()` 之前就返回错误码。
- 运行命令（头注释）：`xmake r config_demo -- --hello hi --timeout 99 -v peter 3`（解析并生成当前工作目录下的 `demo.yaml`）；`xmake r config_demo -- --load-config demo.yaml --hello cli-hi`（CLI 的 `hello` 覆盖配置值）。

## 约束

- target 固定为 `config_demo`，不是目录名 `config`；必须保留 `set_default(false)`。
- 程序会在当前工作目录创建/覆盖 `demo.yaml`，运行目录必须可写；不要把生成物提交进仓库。
- `YAML` 只支持库实现的扁平点号键，不要写嵌套映射或多文档；未知键/类型不匹配只会产生警告并跳过。
- 只使用公共 API：`schema_string`、`to_yaml`、`save_config`、`load_config`、`reset`、`env_string`、`once`；不直接构造 `yaml_doc`。
- 环境变量绑定通过 `.env()` 完成，不要在示例里手工读 `getenv`；会话内预置用 `set_env`。
- 源文件 UTF-8 无 BOM，C++20；该 target 在 Windows 不加 `/EHa`，不要为了“统一”而添加。

## 索引

| 文件 | 关键符号 | 职责 |
| --- | --- | --- |
| `main.cpp` | `Context app("config_demo")`、`schema_string()`、`to_yaml()`、`save_config("demo.yaml")`、`reset()`、`load_config("demo.yaml")`、`env_string()`、`once()` | 构筑模型、打印 schema、解析、YAML 保存/加载、退出 |
| `xmake.lua` | `target("config_demo")`、`set_kind("binary")`、`set_symbols("debug")`、`set_default(false)`、`add_includedirs(...)` | 构建 config_demo（Windows 无 `/EHa`） |

## 决策

- 一次运行内串起 `save → reset → load`，而不是拆成两个程序：能直接看到配置往返；代价是 `reset()` 不清理环境变量会话态，所以只能“模拟”新调用，文档需明确这一点。
- 同时保留 API 与内置选项两条路径：`save_config`/`load_config` 用于显式控制，`--save-config`/`--load-config` 用于命令行场景；代价是第二条命令实际加载两次 YAML，输出解释需要说明。
- 优先级不写成一个比较函数，而由 `from_yaml` 的“CLI 已赋值则跳过”和 finalize 的“外部值/显式值存在则不兜底环境”共同实现；示例用 `--hello cli-hi` 展示 CLI 覆盖。
- 被否方案：嵌套 YAML（如 `options: {timeout: 99}`）。库的存储格式是扁平点号键，示例不展示无法往返的格式。

## 教训

- `main.cpp` 打印 `timeout` 时写“来自配置 arguments 分区的外部赋值”，这与源码不符：`timeout` 是值选项，`to_yaml` 写入的是 `options.timeout`（`config_store.h` 的 `__yamlSection`），应以源码为准。
- `load_config` 返回 true 不代表零警告：未知键、类型不匹配只通过 `Errors::setMsg` 记录并跳过，不会让 `load_config` 失败；只有文件读不到或 YAML 解析失败才返回 false。
- `reset()` 不重置环境变量：`resetModelData` 只还原 Argument/Option，`set_env` 产生的会话态仍在；因此“模拟下一次调用”时 `world` 的初始值并非真正的进程环境。
- `env_string()` 打印的是会话环境表，不是整个 OS 环境；`to_yaml` 也只导出相关环境键，不能用它做“全量环境备份”。
- `demo.yaml` 写在当前工作目录且 `write_file` 以截断方式打开，重复运行会覆盖上一次内容；第二条命令的 CLI 值会再次写回文件。
- 内置 `--load-config` 在解析期即时生效：它之后出现的 CLI 会覆盖同名键，之前的 CLI 会让 `from_yaml` 直接跳过该键；两种顺序下最终都以 CLI 为准，但配置副作用（环境变量写入）发生在解析中途。
- `once()` 使用 `std::exit`，会跳过后续栈展开与 `return`；不要把清理逻辑放在 `once()` 之后。
- 行号会随源码漂移，本文只锚定文件与符号。

