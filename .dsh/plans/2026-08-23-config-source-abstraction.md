# cmd_args 三类输入源统一抽象与自然转换（含 once/loop 语义精确化）

> 计划文档：2026-08-23 创建；实施结果见文末「实施结果」。
> 关联文件 `.dsh/Agent.md`；批准与执行均在 2026-08-23 会话完成。

## 1. 目标与成功标准

用户要求：`Context` 统一三类"外界环境"的**参数格式解析与存储**，并支持**自然转换**：

1. **环境变量**（已是一等公民）——解析、展示、交互、回写；
2. **实时命令行输入**——解析后**可存储到环境变量或脚本**；
3. **脚本文件（YAML 配置，JSON 未来）**——build 完成后把环境变量参数设置**解析并存储为 YAML** 供下次调用；调用时 YAML **反哺设置到环境变量**。

同时精确化 once/loop 语义：**once = 只接收一次参数，处理完退出进程；loop = 不显式 `exit` 就不退出，持续接收命令行参数，且每次调用等同于一次 once 的输入（绝不把多次输入合并成一条命令）**。

成功标准：

- 三类输入源经统一抽象（参数格式 schema / 参数快照 snapshot / 极简 YAML 存储）可互相转换，优先级链 **CLI 显式值 > 配置文件(YAML) > 环境变量 > 内置默认值**；
- loop 每次调用独立解析、互不合并、残留值回默认（等价于一次 once 输入）；
- header-only、C++20、MSVC + Linux 双平台可编译；unit_test 全绿并新增覆盖；
- 计划文档写入 `.dsh/plans/`（第一交付物）。

## 2. 现状盘点（已核实）

- 环境变量已实现：`environment_store`（parse/get/set/unset/expand/apply/env_string，标记 `*` 修改 / `>` 引用 / `+` 新建）、`.env("VAR")` 绑定、兜底优先级 CLI > env > 默认、内置 `-?`/`-!`/`--env-set`/`--env-unset`/`--env-apply`。
- `once()` 已实现；`loop()` 逐行 `Lexer::tokensFromLine` → `parse(ec, toks, {})`。
- **差距 1**：无脚本（YAML）输入源；**差距 2**：loop 中参数/选项值**跨行残留**（`Argument`/`option_value` 无默认值缓存、无"来自外部"标记），不满足"每次调用 = once 输入"；**差距 3**：无循环语义测试；**差距 4**：无统一格式抽象。
- 关键事实：命名参数与选项**可同名**（重名检查分开在 `mgrName` 与 `opts` 内），YAML 必须分区序列化；`type_string<std::string>() == "string"`；`Errors::setMsg` 可复用记录 warning；`Context` 私有继承 `Errors` 并公开 `dump()`。

## 3. 核心抽象设计

### 3.1 三类输入源

```
        ┌───────────────────────────────────────────────┐
        │         parameter_format（build 后固定）        │
        └───────────────┬───────────────────────────────┘
                        │
 ① 环境变量  ────────────┼──► env 兜底（已有）
 ② 实时命令行 ── parse ──┼──► 参数赋值（已有）
 ③ YAML 配置 ── 新增 ────┼──► set_external（新增）
                        ▼
                parameter_snapshot（某次调用的结果）
                        │
        ┌───────────────┼───────────────┐
        ▼               ▼               ▼
    回写环境变量     存为 YAML       下次调用加载
   (set_env/apply)  (save_config)   (load_config 反哺)
```

### 3.2 参数格式 `parameter_schema`（`config/parameter.h`）

```cpp
struct parameter_entry {
    std::string name;      // 命名参数名 / 选项长名 / 方位参数名
    std::string kind;      // "named" | "positional" | "option" | "flag" | "short_circuit"
    std::string type;      // "bool"|"int"|"double"|"string"|""（未定型）
    std::string value;     // 当前值（默认值或解析后值）
    std::string env;       // 绑定的环境变量名（可能为空）
    bool explicit_set{false}; // 本会话被 CLI 显式赋值
    bool external_set{false}; // 本会话被配置(YAML)外部赋值（新增）
};
struct parameter_schema { std::string program; std::vector<parameter_entry> entries; };
struct parameter_snapshot { /* schema 同构，values 为当前取值 + envs 键值对 */ };
```

### 3.3 极简 YAML 子集 `yaml_doc`（`config/yaml.h`，header-only）

仅支持扁平 map（满足参数配置需求，规避完整 YAML 复杂度）：

- `key: value` 顶层键值；`#` 行注释；空行忽略；
- 标量：双引号字符串（`\"` `\\` `\n` 转义）、plain 值、整数、浮点、`true/false`、空值；
- 键名宽松（允许 `.`/`-`/`_`/`/`）；
- 明确不支持（文档标注，留给未来 JSON/扩展）：嵌套 map/list、`---` 文档分隔、锚点、flow 集合、多行块。

接口：`static std::optional<yaml_doc> parse(std::string_view)`（失败返回 nullopt + 错误文本记录）、`dump()`、`has/get/set(key, value)`。

### 3.4 YAML 存储格式（`save_config`/`to_yaml` 输出）

```yaml
# cmd_args config for: once_demo
# format: yaml v1
arguments:
  hello: "hi"
  world: "env-says-hi"
positionals:
  name: "peter"
  count: "3"
options:
  output: "out.txt"
  timeout: 30
flags:
  verbose: false
environment:
  CMD_ARGS_DEMO_WORLD: "env-says-hi"
```

分区避免命名参数/选项同名冲突。`environment` 分区只包含**本程序相关**条目（`referenced__ || modified__ || new__` 的 store 条目 ∪ 所有 schema 中 `.env()` 绑定键，绑定键缺失时值空并注释 `# missing`），**不倾倒全量 OS 环境**。

### 3.5 转换矩阵与优先级

| 方向 | 机制 | 状态 |
|---|---|---|
| ②CLI → 快照 | 既有 parse 赋值 | 已有 |
| ①env → 快照 | `__seedEnvDefaults` 兜底 | 已有 |
| ②CLI → env | `--env-set` | 已有 |
| ③YAML → env | `--load-config`/`yaml_to_env`：environment 分区 → `set_env`（可 `apply_env` 反哺 OS） | 新增 |
| ③YAML → 快照 | `from_yaml`：arguments/positionals/options/flags 分区 → `set_external` | 新增 |
| 快照 → YAML | `save_config`/`to_yaml` | 新增 |
| 快照 → env | `apply_snapshot` 的 env 部分 → `set_env` | 新增（薄封装） |

**优先级链：CLI 显式 > 配置(YAML) > 环境变量 > 内置默认**。实现：新增 `bExternal__` 标记（`Argument` 与 `option_value` 各一），`set_external(value)` 设置值并置标；`__seedEnvDefaults` 的跳过条件由 `bExplicit__` 扩展为 `bExplicit__ || bExternal__`（配置加载后不再被 env 覆盖，但 CLI 后续出现仍无条件覆盖——CLI 赋值路径本来就直接 `set`）。

### 3.6 once/loop 语义精确化

- **once**：接收一次参数 → 解析 → （可选）存储/回写 → `once()` 以退出码退出（现状已满足，不动）。
- **loop**：逐行独立调用；**每次调用前 `reset()`**：
  - 选项 `value` 还原为内置默认、`bExplicit__=false`、`bExternal__=false`；
  - 参数 `value` 还原为构造默认、清除外部/显式标记；
  - `Errors` 清空；
  - `environment_store` 会话态**保留**（多次调用共享）。
- 为支持还原，`Argument` 增 `m_strDefault__`（构造 `_Default` 缓存），`option_value` 增 `m_strDefault__`（`push()` 时保存）；`Lexer` 内 `tokensFromLine` 逻辑（每行独立分词）保持不变——绝无跨行 token 合并。

## 4. API 变更清单（按子系统）

### 4.1 新增文件

- `include/cmd_args/config/yaml.h` —— `yaml_doc`（解析/发射/查询，极简子集）。
- `include/cmd_args/config/parameter.h` —— `parameter_entry` / `parameter_schema` / `parameter_snapshot`。
- `include/cmd_args/config/config_store.h` —— 桥接：从 `Model&` 提取 schema/snapshot、`to_yaml`/`from_yaml`、env↔yaml 互转（供 Context 委托）。
- `example/config/main.cpp` + `example/config/xmake.lua` —— 新 target `config_demo`（`set_default(false)`），演示 build → save_config → load_config → parse → env 反哺 → once。
- `.dsh/plans/2026-08-23-config-source-abstraction.md` —— 本计划文档（含实施结果）。

### 4.2 修改文件

- `include/cmd_args/context/context.h` —— 新增公开 API（§4.3）；`loop()` 每次解析前调 `reset()`。
- `include/cmd_args/context/parser.h` —— `__ensureBuiltins` 增 `--load-config`/`--save-config`；`__seedEnvDefaults` 跳过条件加 `bExternal__`；`__finalize` 末尾若有 save 路径则写出 YAML。
- `include/cmd_args/argument/argument.h` —— `Argument` 增 `mutable bool bExternal__`、`m_strDefault__` 缓存、`set_external(...)`。
- `include/cmd_args/option/option.h` —— `option_value` 增 `bExternal__`、`m_strDefault__` 缓存、`set_external(Value_T&&)`。
- `include/cmd_args/option/option_builder.h` / `argument_builder.h` —— `push()` 时保存默认值到 `m_strDefault__`。
- `include/cmd_args/argument/argument_data.h` —— `named_arg_mgr`/`position_arg_mgr` 增公开 const 遍历（供 `schema()` 提取）。
- `dfx/unit_test/head.cpp` —— 新增 TEST_CASE（§6）。
- `README.md` + `README_zh.md` —— 新增「配置脚本（YAML）」章节、once/loop 语义、目录结构；镜像同步。
- `.dsh/Agent.md` —— §8/§9 增补 YAML 配置与 reset 约定（顺手）。

### 4.3 Context 新增公开 API

```cpp
// 参数格式
parameter_schema schema() const;
std::string      schema_string() const;          // 人类可读（帮助式）
// 快照
parameter_snapshot snapshot() const;
void apply_snapshot(parameter_snapshot const &); // 参数走 set_external，env 走 set_env
// 配置脚本（YAML）
std::string to_yaml() const;                     // 参数 + 环境 全量
bool save_config(std::string const &path) const; // to_yaml 写文件
bool from_yaml(std::string const &text);         // 反哺；未知键/类型不匹配记 warning（可经 dump() 查看）不中断
bool load_config(std::string const &path);       // 读文件 + from_yaml
std::string env_to_yaml() const;                 // 仅 environment 分区
bool yaml_to_env(std::string const &text);       // 仅 environment → set_env
// 执行状态
Context &reset();                                // loop 每次调用前（内部还原默认值/清标记/清错误）
```

**内置选项**（懒注册，长名避冲突，`set_builtin_options(false)` 仍可整体关闭）：

- `--load-config PATH` —— `on_set` 立即 `from_yaml`（先于后续 CLI token 处理，CLI 可覆盖）；
- `--save-config PATH` —— `on_set` 记录路径，`__finalize` 末尾写出（此时值已完整）。

## 5. 数据流/时序

`parse(ec, argc, argv, envp)`：Lexer 出 token → Parser 分配（其中 `--load-config` 按其出现顺序反哺配置）→ `__seedEnvDefaults`（跳过 external 已有值者）→ `__expandEnvValues` → `--save-config` 写出。`loop()`：每行 = reset() + 上述完整流程；`exit`/`quit`/EOF 退出。

## 6. 测试计划（20 → 约 28 个 TEST_CASE）

1. `yaml: dump/parse 往返（注释、引号、转义、空值）`
2. `yaml: 非法输入返回 nullopt 不崩溃`
3. `schema: 导出完整性（四个分区 + 顺序 + env 绑定）`
4. `config: save_config → load_config 往返，值一致`
5. `config: from_yaml 反哺 env（environment 分区 set_env 命中）`
6. `config: 优先级 CLI > YAML > env > 内置默认（四种组合断言）`
7. `config: 未知键记 warning 不中断（dump 可查）`
8. `loop: 两次独立输入互不合并、值各自生效、未给项回默认`
9. `loop: 行错误不影响下一行、exit/quit 退出`
10. `parser: --load-config / --save-config 内置 e2e`

## 7. 实施步骤

1. 写计划文档（本文件，第一交付物）。
2. 新增 `config/yaml.h`（极简 YAML 子集）。
3. 新增 `config/parameter.h` + `config/config_store.h`（schema/snapshot/转换桥接）。
4. 数据模型演进：`Argument`/`option_value` 加默认值缓存与 `bExternal__`；管理器加 const 遍历；builder `push` 记录默认。
5. `context.h` 新增公开 API + `reset()`；`parser.h` 内置新选项与 seed 跳过条件；`loop()` 接入 reset。
6. 新示例 `config_demo`；`once` 示例补 `--save-config/--load-config` 演示行。
7. `head.cpp` 新增上述 10 个 TEST_CASE。
8. 双平台编译 + `unit_test` 全绿；`once`/`config_demo` 运行验证。
9. README 双语更新（YAML 章节、once/loop 语义、目录结构）。
10. 计划文档补「实施结果」章节；gitmoji 风格提交（`feat(config): ✨ ...`），推送仍受网络限制则仅本地提交。

## 8. 验收标准

- [ ] `xmake build unit_test && xmake run unit_test` 全绿（含新增用例）；
- [ ] `xmake build once`、`xmake build config_demo` + 运行通过；
- [ ] 循环语义：loop 两次输入互相独立、残留回默认、不合并；
- [ ] YAML 往返一致、反哺 env 生效、优先级链符合 §3.5；
- [ ] README 中英镜像同步；计划文档落 `.dsh/plans/`。

## 9. 范围外与风险

- JSON/Lua 等脚本格式：**仅预留接口位置**（未来按扩展名/显式 format 分发），本轮不做；
- 完整 YAML（嵌套/锚点/flow/多文档）不做，文档标注子集边界；
- 未知配置键：warning 不中断（宽松导入），不拒绝启动；
- `_putenv_s`/`setenv` 回写受平台限制，`--load-config` 反哺 env 默认只写会话不自动回写 OS（与现有 `--env-set` 行为一致）；
- 不新增第三方依赖，YAML 解析器自实现（约 200–300 行，含测试）。

## 10. 假设

- 用户默认认可：YAML 为当前唯一脚本格式（JSON 未来）；配置优先级 CLI > YAML > env > 默认；loop 每次调用前重置为"once 输入"语义；新内置选项使用长名 `--load-config`/`--save-config`。
- 若实施中发现与现状冲突，以本计划 §3.6 语义为准并记录偏差。

---

## 实施结果（2026-08-23，批准后执行）

### 实际改动文件

**新增**
- `include/cmd_args/config/yaml.h` —— `yaml_doc`（极简 YAML 子集：解析/发射/查询，扁平 map）
- `include/cmd_args/config/parameter.h` —— `parameter_entry` / `parameter_schema` / `parameter_snapshot`
- `include/cmd_args/config/config_store.h` —— 桥接：`schema_of`/`snapshot_of`/`apply_snapshot`、
  `to_yaml`/`from_yaml`/`env_to_yaml`/`yaml_to_env`、`read_file`/`write_file`
- `example/config/main.cpp` + `example/config/xmake.lua` —— `config_demo`（set_default(false)）
- `.dsh/plans/2026-08-23-config-source-abstraction.md` —— 本计划文档

**修改**
- `include/cmd_args/context/context.h` —— 新增 `schema/schema_string/snapshot/apply_snapshot/
  to_yaml/save_config/from_yaml/load_config/env_to_yaml/yaml_to_env/reset`；`loop()` 每次调用前 `reset()`
- `include/cmd_args/context/parser.h` —— 内置 `--load-config`/`--save-config`；`__seedEnvDefaults`
  跳过条件加 `bExternal__`；`__finalize` 末尾写出 `--save-config`
- `include/cmd_args/argument/argument.h` —— `m_strDefault__` 缓存、`bExternal__`、`set_external`、`reset`、`isExternal`
- `include/cmd_args/option/option.h` —— `strDefault__`、`bExternal__`、`bBuiltin__`、`set_external`、`reset`
- `include/cmd_args/argument/argument_data.h` —— `doVisitC` 公开 const 遍历（named/positional）
- `include/cmd_args/env/environment_parser.h` —— `for_each` 公开只读遍历
- `include/cmd_args/argument/argument_parser.h` —— 位置参数数量校验改为 `[未外部赋值数, 总数]` 区间，
  配置已填位可省略、多余 token 覆盖（CLI > 配置）
- `dfx/unit_test/head.cpp` —— 新增 10 个 TEST_CASE（yaml 往返/非法、schema 导出、save/load 往返、
  env 反哺、四层优先级、未知键 warning、loop 独立输入、loop 错误隔离、load/save-config e2e 含位置参数语义）
- `dfx/unit_test/xmake.lua` —— MSVC `/bigobj`（debug 下 Catch2 节数超限 C1128）
- `README.md` + `README_zh.md` —— 三类外界环境、YAML 章节、once/loop 精确语义、优先级链、目录结构
- `.dsh/Agent.md` —— §1/§3/§4/§5/§6/§8/§9/§10 与现状对齐
- `.gitignore` —— `/demo.yaml`（config_demo 示例产物）

### 验收勾选

- [x] `xmake build unit_test && xmake run unit_test` 全绿：**30 个测试用例 / 170 条断言**（debug + release）
- [x] `xmake build once`、`xmake build config_demo` + 运行通过（含 `--load-config` 无位置参数场景）
- [x] 循环语义：loop 两次输入互相独立、残留回默认、不合并、行错误隔离（测试固化）
- [x] YAML 往返一致、反哺 env 生效、优先级链 CLI > 配置 > 环境 > 默认（测试固化）
- [x] README 中英镜像同步；计划文档落 `.dsh/plans/`；gitmoji 风格提交

### 偏差记录（相对计划）

1. **YAML 存储格式**：计划示例为嵌套分区，实际实现采用**扁平点号分区键**
   （`arguments.hello` / `positionals.name` / `options.timeout` / `flags.verbose` / `environment.KEY`），
   与极简 YAML 子集（仅扁平 map）一致；README 同步更新。
2. **缺失绑定键**：`environment` 分区只导出 store 中存在的绑定键，缺失键不再写入空值
   （避免导入时造出空环境变量）。
3. **内置选项过滤**：`option_value` 增加 `bBuiltin__` 标记，内置交互选项
   （env-set/env-unset/load-config/save-config）不参与 `schema()`/`to_yaml()` 导出。
4. **位置参数语义**：配置提供位置参数后 CLI 可省略（`[未赋值数, 总数]` 区间），
   多于未赋值数时按序覆盖配置值（CLI > 配置）。
5. **debug 构建**：Catch2 单文件在 debug（不开优化）下触发 MSVC C1128，`unit_test` target 追加 `/bigobj`。
6. **`xmake run` 传参**：PowerShell 下 `xmake run` 拼接参数偶发 execv 失败（既有现象），
   验证/文档改用直接运行可执行文件路径；示例本体无问题。