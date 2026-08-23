# cmd_args 选项 action（system_call 子进程）与全库注释补全

> 计划文档：2026-08-23 创建；批准与执行均在 2026-08-23 会话完成。
> 关联文件 `.dsh/AGENT.md`；实施结果见文末「实施结果」。

## 1. 目标与成功标准

用户（代码检视后）提出两点：

1. **整个项目注释很少** —— 为全部自有代码补充中文注释；
2. **部分选项可以绑定一个 action** —— 该 action 本质上是一次 `system_call`
   （`std::system` 的子进程命令执行）；interpreter 解释脚本时
   **按照先后顺序识别并执行对应的命令**。

成功标准：

- 选项 `.action("cmd {value}")` 可用：解析期**显式命中即执行**，顺序 = 命令行出现顺序；
- `{value}` 占位符替换为本次命中的值；
- 环境变量兜底 / YAML 外部赋值 / 内置默认值**不触发** action；
- `std::system` 非 0 → stderr 告警，**不中断**解析；
- 注释覆盖全部自有代码（`include/cmd_args` 除 vendored `extern/`、`example/`、`dfx/unit_test/`）；
- header-only / C++20 / MSVC+Linux 可编译；unit_test 全绿并新增覆盖；README 双语同步。

## 2. 设计决策（用户确认）

| 决策点 | 结论 |
|---|---|
| 绑定对象 | 仅选项/标志（`option_value`）；命名/方位参数不参与 |
| 执行时机 | **解析期命中即执行**（`__assign` 路径，token 顺序天然有序），
  不引入独立的"解释阶段"；once/loop 的逐行解释沿用同一条路径 |
| 命令构成 | 命令模板 + `{value}` 占位符（无占位符 = 固定命令） |
| 注释范围 | 全部自有代码（中英镜像 README 同步） |

## 3. 实现

### 3.1 数据（`include/cmd_args/option/option.h`）

`option_value` 新增：

- `std::string strActionTemplate__;` —— 命令模板（空 = 未绑定）；
- `option_value &action(std::string _CmdTemplate);` —— 链式绑定。

### 3.2 执行（`include/cmd_args/option/option_parser.h`）

`__assign`（唯一显式赋值路径）在 `fnOnSet__` 之后追加：

```
模板非空 → 替换 {value} → std::system(cmd)；
返回非 0 → std::cerr << "(action) command exit ..." 告警，继续解析。
```

- 长/短/聚合短选项均走 `__assign`，因此顺序 = 命令行出现顺序；
- env 兜底（`__seedEnvDefaults`）与 `set_external`（YAML）不走 `__assign`，天然不触发；
- 替换后命令为空则跳过执行（防御 `system("")` 的平台差异）。

### 3.3 测试（`dfx/unit_test/head.cpp`，+4 TEST_CASE）

1. `context: 选项 action 命中即执行 system_call，{value} 原样替换` —— 临时文件副作用，
   验证真实子进程执行 + 含空格值的替换；并断言不进入 schema/YAML 导出；
2. `context: action 按命令行出现顺序多次执行` —— `--step one --step two` 追加写文件，逐行断言；
3. `context: 环境变量兜底与 YAML 外部赋值不触发 action`；
4. `context: action 命令失败向 stderr 告警且不中断解析` —— 捕获 `std::cerr`，
   `cmd /c exit 1`（Windows） / `false`（POSIX），断言后续选项照常赋值。

测试通过 `<filesystem>::temp_directory_path()` 写临时文件，与 cwd 无关。

### 3.4 示例（`example/once/main.cpp`）

新增 `--notify` 选项演示 `action("echo notify: {value}")`，头注释与运行示例同步。

## 4. 注释补全范围（中文，仅增注释不改逻辑）

- `base/`：`def.h`（宏说明）、`util.h`（type_string/to_string/parse_value/parse_bool/
  starts/ends/trim/replace/parse_line/Group）、`defer.hpp`（RAII 用法）、`cmd_args.hpp`（入口说明）；
- `error/`：七个头文件补齐"错误域三件套"结构与典型码说明；
- `argument/`：`argument.h`（类语义/命名/方位）、`argument_builder.h`、`argument_data.h`、
  `argument_parser.h`；
- `option/`：`option_data.h`（管理器职责）、`option.h`/`option_parser.h`（连同 action 新注释）；
- `env/`：别名兼容说明；`config/`、`context/`、`lexer.h` 等既有注释已较完整，按需微调；
- `functional_util.h`：补文件级说明（当前无引用，保留备用）。
- `dfx/unit_test/head.cpp`、`example/`：已有分节注释，随 action 用例补齐。

## 5. 文档同步

- `README.md` + `README_zh.md`：新增「选项 action（system_call 子进程执行）」章节
  （绑定示例 + 语义约定）；用法示例补 action 行；"已实现"清单与测试统计更新
  （30 用例/170 断言 → 34 用例/187 断言）。
- `.dsh/AGENT.md`：§3 源码地图、§4 API 事实、§6 行为约定、§9 测试门槛同步。

## 6. 验收

- [ ] `unit_test` debug + release 全绿（34 用例 / 187 断言，含 4 个新 action 用例）；
- [ ] `once` / `config_demo` 构建 + 运行通过（`--notify "deploy done"` 演示 action 输出）；
- [ ] README 中英镜像同步；计划文档落 `.dsh/plans/`；注释覆盖全部自有代码。

---

## 实施结果（2026-08-23，批准后执行）

### 实际改动文件

**修改**
- `include/cmd_args/option/option.h` —— `option_value` 增 `strActionTemplate__` + `.action()`
- `include/cmd_args/option/option_parser.h` —— `__assign` 增 `{value}` 替换 + `std::system`
  执行 + 非 0 告警（含 `<cstdlib>`/`<iostream>`）
- `dfx/unit_test/head.cpp` —— 新增 4 个 action TEST_CASE（含 `<filesystem>`/`<iostream>`）
- `example/once/main.cpp` —— 新增 `--notify` action 演示
- `README.md` + `README_zh.md` —— action 章节、用法示例、已实现清单、测试统计
- `.dsh/AGENT.md` —— §3/§4/§6/§9 同步
- 注释补全：`base/def.h`、`base/util.h`、`base/defer.hpp`、`cmd_args.hpp`、
  `error/*`（7 个）、`argument/*`（4 个）、`option/option_data.h`、
  `context/builder.h`、`context/loader.h`、`context/parse_fwd.h`、
  `context/interpreter.h`、`context/def.h`、`env/environment_parser.h`、
  `error/functional_util.h` 等（仅注释，无逻辑改动）

### 验收勾选

- [x] `unit_test` debug + release 全绿：**34 个测试用例 / 187 条断言**
- [x] `once` / `config_demo` 构建 + 运行通过（`--notify "deploy done"` 输出 action 执行行）
- [x] README 中英镜像同步；AGENT.md 事实同步；计划文档落 `.dsh/plans/`

### 偏差记录（相对计划）

1. **API 返回类型**：`Context::add_option` 仍返回 `Context&`（保持链式构筑），
   action 绑定经 `app.option("name")->action("...")` 句柄完成（与 `env()`/`on_set()` 一致）。
2. **执行时机**：按用户确认落在**解析期**（`__assign`），`Interpreter` 基类不改
   （其 `once()/loop()` 实际语义由 `Context` 实现，逐行解释即逐行 parse，同样走执行路径）。
