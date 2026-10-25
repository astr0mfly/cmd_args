# config — YAML 配置脚本、参数快照与 Model 桥接

> **摘要**：config 实现 cmd_args 的“第三类外界环境”——配置脚本。`yaml.h` 提供零依赖的扁平点号 YAML 子集解析/发射；`parameter.h` 定义 `parameter_entry`/`parameter_schema`/`parameter_snapshot` 中间格式；`config_store.h` 完成 `Model` → schema/snapshot 提取、`Model` ↔ YAML 导入导出（`to_yaml`/`from_yaml`/`save_config`/`load_config`）以及环境变量 ↔ YAML 定向桥接。导入走 `setExternal`，与 CLI 显式赋值、环境变量兜底形成四级优先级。
> **联系**：上级索引 [`../cmd_args.md`](../cmd_args.md)；根说明 [`../../../README_zh.md`](../../../README_zh.md) 的“配置脚本（YAML）：第三类外界环境”“YAML 存储格式（扁平点号分区键）”“YAML 子集边界”三节。依赖 [`../context/model.h`](../context/model.h)、[`../env/env_builder.h`](../env/env_builder.h)、[`../error/errors.h`](../error/errors.h)；由 [`../context/context_base.h`](../context/context_base.h) 的 `schema()`/`snapshot()`/`to_yaml()`/`from_yaml()`/`save_config()`/`load_config()` 暴露；行为测试见 [`../../../dfx/unit_test/test_config.cpp`](../../../dfx/unit_test/test_config.cpp)。

## truth

| 文件 | 行数 | 职责 |
| --- | --- | --- |
| `yaml.h` | 274 | `yaml_doc`：扁平 `key: value` 文本的解析与发射 |
| `parameter.h` | 79 | `parameter_entry`、`parameter_schema`、`parameter_snapshot` 纯数据结构 |
| `config_store.h` | 340 | `schema_of`/`snapshot_of`/`apply_snapshot`；`to_yaml`/`from_yaml`/`env_to_yaml`/`yaml_to_env`；`read_file`/`write_file` |

### yaml_doc（`yaml.h`）

- 存储：`using Entries = std::vector<std::pair<std::string, std::string>>`；保留插入顺序，**允许重复 key**；`get`/`set`/`erase`/`has` 只命中第一个。
- `static std::optional<yaml_doc> parse(std::string_view _Text, std::string *_Error = nullptr)`（`yaml.h:50`）：逐行去掉行尾 `\r`（兼容 CRLF）→ `util::trim` → 跳过空行与首字符 `#` 的注释行 → 解析 `key: value`。
- 键与值定位：只认第一个**裸冒号**，引号内与 `\\` 转义的冒号不计（`yaml.h:201` 起）。
- 解析失败返回 `nullopt`，错误文本（均以 `yaml: line N: ` 开头）：
  - `missing ':' separator`
  - `empty key`
  - `unterminated double-quoted string`
  - `trailing characters after quoted string`
- 值形式：
  - 空值，或 `key: #...` → 空串；
  - 双引号字符串：反转义仅 `\"`、`\\`、`\n`、`\t`、`\r`，其余 `\x` 原样保留（`yaml.h:170-192`）；
  - plain 值：行内 `#` 仅当前一字符是空格/制表才截断注释，因此 `a#b` 保留 `#`（`yaml.h:149-158`）；
  - 单引号不作为字符串语法，引号被当作普通字符保留。
- `dump()`（`yaml.h:82-92`）：每个条目输出 `key: "value"`；**值**强制双引号并转义 `"`/`\\`/`\n`/`\t`/`\r`；**键**原样输出，不转义、不加引号。
- 明确不支持（`yaml.h:31-38` 注释；README_zh 同）：嵌套 map/list、`---` 文档分隔、锚点、flow 集合、多行块。
- `get(key, fallback = {})` 缺失返回 fallback（默认空串）；`entries()` 返回 `Entries const&`。

### 参数格式与快照（`parameter.h`）

`parameter_entry` 字段：

| 字段 | 含义 |
| --- | --- |
| `name` | 参数名 / 选项长名 / 方位参数名 |
| `kind` | `"named"`、`"positional"`、`"option"`、`"flag"`、`"short_circuit"` |
| `type` | `"bool"`/`"int"`/`"double"`/`"string"` 等，未定型为空 |
| `value` | 当前值（pre-parse 时为内置默认） |
| `env` | 绑定的环境变量名，未绑定为空 |
| `explicit_set` | 本会话被 CLI 显式赋值 |
| `external_set` | 本会话被配置（YAML）外部赋值 |
| `required`/`default_set`/`uses_default` | 必选标记 / 有非空默认 / 当前值来自默认 |
| `validators` | 校验器名称列表（仅名字，供展示） |

`parameter_schema` = `program` + `entries` + `find(name)`（线性查找，返回 `parameter_entry const*` 或 `nullptr`）。`parameter_snapshot` = `values`（与 schema 对齐）+ `envs`（本程序相关环境键值对）。

### schema 导出（`schema_of`，`config_store.h:56-141`）

| Model 来源 | kind | 备注 |
| --- | --- | --- |
| `args.mgrName.doConVisit` | `named` | 取 name/type/value/env/external/required/default/validators |
| `args.mgrPos.doConVisit` | `positional` | 同上 |
| `opts.mgrValues.forEachValue` | `flag`（`strType == "bool"`）或 `option` | `bBuiltin__` 为真时**跳过**（内置交互选项不导出） |
| `opts.mgrActions.forEachAction` | `short_circuit` | 只填 `name`，`type` 为空；无 value/env/required |

实际条目顺序：named → positional → 值选项（option 与 flag 按**注册顺序**混排）→ short_circuit。`parameter.h` 注释写的“option → flag”分组顺序与实现不完全一致（见“教训”）。

### YAML 分区键（`__yamlSection`，`config_store.h:174-184`）

| kind | 键前缀 |
| --- | --- |
| `named` | `arguments.` |
| `positional` | `positionals.` |
| `option` | `options.` |
| `flag` | `flags.` |
| `short_circuit` | 无（不入配置） |
| 环境变量 | `environment.` + 变量名 |

扁平点号键的目的（`config_store.h:35-36` 注释）：避免命名参数与选项同名冲突。

### 导出与导入

- `to_yaml(Model const&)`（`config_store.h:186-205`）：先写两行注释头 `# cmd_args config for: <program 或 unknown>`、`# format: yaml v1`，再 `doc.dump()`；键按 schema 顺序 + `__relevant_envs` 顺序。
- `env_to_yaml(Model const&)`：只导出 `environment.` 分区。
- `from_yaml(Model&, std::string const &_Text, Errors *_Warnings = nullptr)`（`config_store.h:239-300`）：
  - `environment.`（`substr(12)`）→ `EnvBuilder(&_M.envs).setEnv`，**不写 OS 环境变量**；
  - `arguments.`（`substr(10)`）/ `positionals.`（`substr(12)`）→ `mutGet`；类型检查失败告警并跳过；`hasValue()` 为真则跳过（CLI 已赋值，CLI > 配置）；否则 `setExternal`；
  - `options.`（`substr(8)`）→ `mutFind`；类型检查 + `bExplicit__` 跳过；否则 `setExternal`；
  - `flags.`（`substr(6)`）→ `mutFind`；仅 `bExplicit__` 跳过，**无类型检查**；否则 `setExternal`；
  - 未匹配任何分区 → 告警 `config: unknown key '<k>', ignored`；
  - 未知名字 → 告警 `config: unknown named argument/position argument/option/flag '<name>'`；
  - 解析失败 → 把 `yaml: line ...` 写入 `_Warnings` 并返回 `false`；**成功即有 warning 也返回 `true`**。
- 类型检查 `__typeOk(type, value)`（`config_store.h:213-235`）：

| type | 通过条件 |
| --- | --- |
| 空 / `"string"` | 恒真 |
| `"bool"` | 精确等于 `true`/`false`/`TRUE`/`FALSE`/`yes`/`no`/`on`/`off`/`1`/`0` |
| `"int"`/`"int64_t"`/`"size_t"`/`"unsigned int"` | 以 `long long` 走 `istringstream`，`!is.fail()` 即通过（不要求消费完整字符串） |
| `"double"`/`"float"` | 以 `double` 走 `istringstream`，`!is.fail()` |
| `"char"` | 字符串长度恰为 1 |
| 其他未知类型 | 放行（面向未来类型） |

- 环境变量相关导出 `__relevant_envs`（`config_store.h:143-171`）：env store 中 `bReferenced__ || bModified__ || bNew__` 的条目，并补充 schema 里 `.env()` 绑定且仍在 store 中存在的键；store 中不存在的绑定键**不导出**，避免导入时造出空变量。
- `apply_snapshot`（`config_store.h:153-172`）：named/positional → `mutGet` + `setExternal`；option/flag → `mutFind` + `setExternal`；envs → `EnvBuilder::setEnv`。
- 文件读写：`read_file` 返回 `std::optional<std::string>`（`ios::binary` + `ostringstream`）；`write_file` 返回 `bool`（`binary | trunc`）。`ContextBase::save_config(path)` = `write_file(path, to_yaml())`；`load_config(path)` 读失败时写入 `config: cannot read file: <path>` 并返回 `false`。
- 优先级链（`config_store.h:47` 注释）：**CLI 显式值 > 配置文件 YAML > 环境变量 > 内置默认值**；配置导入记录为 `external`，env 兜底会跳过 external 值。

### 测试覆盖（`dfx/unit_test/test_config.cpp`，8 个 `TEST_CASE`）

1. dump/parse 往返（转义、空值、行内注释 `plain: hi # c` → `hi`）；
2. 非法输入返回 `nullopt` 且 `_Error` 非空（无冒号、未闭合引号、引号后尾随字符）；
3. schema 完整性：5 项依次为 named `hello`、positional `name`、option `output`（env `MY_OUTPUT`、value `out.txt`）、option `timeout`（`int`）、flag `verbose`（`bool`）；
4. `save_config` → `load_config` 往返（`--hello hi --timeout 99`）；
5. YAML `environment.CMD_UT_X` 反哺 env store；
6. 优先级 90（CLI）> 70（YAML）> 50（env）> 30（默认）；
7. 未知键产生 warning 且不中断，dump 仍含 `unknown.key`；
8. `--load-config` 端到端：CLI 覆盖配置、配置提供位置参数、`--save-config` 落盘。

## 约束

- **只支持扁平单层键**：不得写入嵌套 map/list、`---`、锚点、flow 集合、多行块；需要新结构时先扩展 `yaml.h` 并同步 README_zh 的“YAML 子集边界”。
- **键名不得含 `:` 与换行**：`dump()` 原样输出键，含裸冒号会破坏再次解析（见“教训”）。键名同样不应含首尾空白，因为解析会对整行 trim。
- **分区前缀是协议**：`arguments.`/`positionals.`/`options.`/`flags.`/`environment.` 与 kind 的映射固定在 `__yamlSection` 与 `from_yaml`；改名必须两侧同步，否则配置静默变“unknown key”。
- **`short_circuit` 不参与配置**：`--help` 一类动作没有可持久化状态；`schema_of` 导出它们只为格式展示。
- **内置选项不导出**：`bBuiltin__` 的 `--load-config`/`--save-config`/`--env-set` 等必须跳过，防止把交互开关写进配置文件。
- **导入必须走 `setExternal`，不得用 `set`**：`set` 会被记为 CLI 显式赋值，破坏“CLI > YAML > env > 默认”的判定与 env 兜底。
- **不得写 OS 环境变量**：环境导入只更新会话内的 env store（`EnvBuilder`）；OS 环境由用户显式操作。
- **`from_yaml` 的返回值只表示“文本解析成功”**：调用方必须同时检查 `Errors` 堆栈（`dump()`/`getLastMsg()`）才能区分“全部生效”与“部分忽略”。
- **`flags.` 的值必须预先按布尔语义给出**：该分支没有 `__typeOk`，非法文本会直接写入 flag 的 external 值，错误延后到取值/校验阶段。
- **不改 `parameter_entry` 字段语义**：`external_set` 表示“来自配置文件”，与 `explicit_set`（CLI）互斥地驱动优先级；新增状态需同时改 `schema_of` 与 `apply_snapshot`。
- **文件 IO 固定二进制模式**，避免 Windows 文本模式改写 `\n`。

## 索引

| 文件 | 关键符号 | 职责 |
| --- | --- | --- |
| `yaml.h` | `yaml_doc`、`Entries`、`parse`、`dump`、`get`/`has`/`set`/`erase`/`size`/`empty`/`entries`、`__parseLine`、`__stripComment`、`__escape`/`__unescape` | 扁平 YAML 子集的解析与发射 |
| `parameter.h` | `parameter_entry`、`parameter_schema`、`parameter_snapshot`、`find` | 参数格式/快照的中间数据模型 |
| `config_store.h` | `schema_of`、`snapshot_of`、`apply_snapshot`、`to_yaml`、`from_yaml`、`env_to_yaml`、`yaml_to_env`、`__relevant_envs`、`__yamlSection`、`__typeOk`、`read_file`、`write_file` | Model ↔ schema ↔ YAML ↔ env 的桥接 |

grep 锚点（行号基于 2026-10-25 核对）：

- 子集声明：`yaml.h:31-38`；`parse`：`yaml.h:50`；`dump`：`yaml.h:82-92`；`__stripComment`：`yaml.h:149-158`；`__parseLine`：`yaml.h:201`
- 优先级注释：`config_store.h:47`；`schema_of`：`config_store.h:56`；内置跳过：`config_store.h:91-92`；`__yamlSection`：`config_store.h:174-184`；`from_yaml`：`config_store.h:239-300`；`__typeOk`：`config_store.h:213-235`

## 决策

- **手写扁平 YAML 子集，不引入 yaml-cpp 等第三方库**：项目承诺 header-only、零第三方依赖，配置需求只需“标量键值 + 注释”；代价是不支持嵌套/锚点/多行，且解析器只有 4 类错误、没有行列号以外的诊断。
- **用扁平点号分区键而不是嵌套 map**：与极简解析器自洽，且 `arguments.foo` 与 `options.foo` 不冲突；代价是键名拼接/截取依赖固定前缀长度（`substr(10/12/8/6/12)`），前缀改动必须全量同步。
- **配置导入使用 `setExternal` 而非 `set`**：需要区分“用户命令行显式给的值”和“配置文件给的值”，让 CLI 覆盖配置、env 只兜底未被外部赋值的键；代价是每个参数/选项多一个 `external` 状态位，且必须在 schema 中导出。
- **CLI 已赋值时直接 `continue`**：以 `Argument::hasValue()`/`OptionValue::bExplicit__` 为判据，不解析也不告警；代价是同一键的配置值被静默丢弃，排查时只能靠业务侧对比。
- **内置选项与 short_circuit 不导出**：前者是交互开关、后者是无状态动作，写进配置文件既不可回放也无意义；代价是 schema 与 YAML 的条目集合不完全一一对应。
- **只导出“本程序相关”的环境变量**：避免把整个进程环境写进配置；判据是 env store 的引用/修改/新建标记 + schema 绑定且存在的键；代价是未被读取过的环境变量不会出现在保存结果中。
- **warning 不阻断导入**：未知键、类型不匹配、未知名字都只记录到 `Errors` 并继续，便于配置向后兼容与多程序共用配置文件；代价是拼写错误不会硬失败，调用方必须主动检查诊断。
- **文件头两行固定注释**：`# cmd_args config for:` 与 `# format: yaml v1` 提供来源与版本线索，且被解析器当注释跳过；代价是格式版本目前只展示、不校验。
- **`read_file` 返回 `optional`**：把“读不到文件”与“文件为空”区分开；`load_config` 在读失败时写入明确消息，而不是返回空文档。

## 教训

- **键名含 `:` 会破坏往返**：`dump()` 直接拼接 `k + ": \"" +`，键不转义也不加引号（`yaml.h:82-90`）。名为 `a:b` 的参数保存后再解析会被切成键 `a`、值 `b: ...` 的一部分。约束：`name` 不得含冒号/换行。
- **手写 Windows 路径在双引号里会被转义吞掉**：`"C:\temp"` 解析时 `\t` 变成制表符（`yaml.h:170-192` 只保留未识别的转义，识别的 5 个会被替换）。程序 `dump()` 会先把 `\\` 转成 `\\\\` 所以往返安全，但人工编辑必须写 `\\` 或改用其他转义策略。
- **单引号不是字符串语法**：`key: 'abc'` 得到的是包含两个单引号的 5 字符串面量（`yaml.h` 只处理双引号）。不要按完整 YAML 习惯书写。
- **`#` 只有前面是空白才算注释**：`a#b` 原样保留，`a #b` 截断为 `a ` 再 trim（`yaml.h:149-158`）。写含 `#` 的值时注意前导空格。
- **重复键保留且首个生效**：解析与 `set` 都不去重，`get`/`set` 命中第一个（`yaml.h:120-145`）。导入时后出现的同名键仍会执行（遍历全部 entries），但二者语义不同——排查“配置没生效”先查重复键。
- **`from_yaml` 返回 `true` 不代表全部生效**：未知键/类型不匹配/未知名字都只写 warning（`config_store.h:239-300`）。上层必须检查 `Errors::dump()`/`isOk()`，否则拼写错误会被当作成功。
- **`flags.` 分支缺少类型检查**：`options.` 分支调用 `__typeOk(o->strType, v)`，`flags.` 分支只查 `bExplicit__`（`config_store.h:283-299`）。布尔 flag 收到 `"maybe"` 这类非布尔文本也会被写为 external 值，错误延后到后续读取/校验。
- **`__typeOk` 不要求消费完整字符串**：`"12abc"` 经 `istringstream >> long long` 读得 12 且不置 fail，因此被判为合法 `int`（`config_store.h:213-235`）。需要严格数值校验时用 `is_float` 类谓词或 `Validator`。
- **内置 `--load-config` 丢弃 warning**：`builtin_options.h:80-86` 调用 `config::from_yaml(*m, *text)` 时未传 `Errors*`，`__warn` 遇到空指针静默返回（`config_store.h:207-210`）；只有 `ContextBase::load_config` 会把 `this` 作为 `Errors*` 传入。两条载入路径的诊断能力不同。
- **schema 顺序不是 `parameter.h` 注释的分组顺序**：注释写“named → positional → option → flag → short_circuit”，但 option 与 flag 来自同一个 `mgrValues` 注册序列（`config_store.h:85-112`），实际按注册顺序混排。依赖顺序的测试/序列化不要假设分组。
- **`schema_of` 的 `value` 是当前值**：`save_config` 保存的是运行期已解析值（含默认值），不是“仅用户显式设置项”；因此保存再载入会把默认值固化为 external 值。需要最小化配置时应在保存前过滤 `uses_default`/`explicit_set`。
- **`strProgramName` 为空时头部写 `unknown`**（`config_store.h:203-204`）：程序名来自 Context 初始化时机，未设置时不影响解析，但会让配置头失去可读来源。
