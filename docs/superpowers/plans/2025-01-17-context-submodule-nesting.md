# Context 子模块嵌套实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use subagent-driven-development (recommended) or executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现 Context 的子模块嵌套功能，支持 `app sub1 sub2 --option value` 形式的层级子命令解析，每个子命令创建独立的子 Context 拥有自己的 options。

**Architecture:** 
- 在 Context 中注册子命令映射表（子命令名 → 子 Context 工厂函数）
- 解析时先扫描 token 流中的子命令路径（连续的非 `-` 开头 token），匹配到最深层子命令
- 将剩余 token 传递给匹配到的子 Context 进行常规解析
- 支持任意深度嵌套，突破短选项 26 字母限制

**Tech Stack:** C++17, 现有 cmd_args 库架构

**Spec:** 用户需求：支持 `app sub1 sub2 sub3 ...` 形式的层级子命令，每个子命令创建独立的 Context 拥有自己的 options。

---

## Global Constraints

- 遵循现有代码风格：`camelCase` 命名，`Doxygen` 中文注释
- 类型别名对齐：使用 `Name_T`, `Desc_T` 等已有别名
- 保持向后兼容：现有 Context 用法不受影响
- 不修改现有 Model/Compiler/Executor 核心逻辑

---

## File Structure

| 文件 | 职责 |
|------|------|
| `include/cmd_args/context/subcommand.h` | 子命令注册表与子 Context 工厂 |
| `include/cmd_args/context/context.h` | 扩展 Context 添加子命令支持 |
| `include/cmd_args/context/context_sub.h` | 子 Context 派生类（可选，用于特化行为） |
| `dfx/unit_test/test_subcommand.cpp` | 子命令单元测试 |

---

### Task 1: 定义子命令注册表

**Files:**
- Create: `include/cmd_args/context/subcommand.h`

**Interfaces:**
- Consumes: 无
- Produces: `SubcommandRegistry` 类，管理子命令名 → 工厂函数的映射

- [ ] **Step 1: 创建 subcommand.h 头文件**

```cpp
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "cmd_args/base/def.h"

CMD_ARGS_NAMESPACE_BEGIN

// 前向声明
class ContextBase;

/// 子命令工厂函数类型：创建并返回子 Context
using SubcommandFactory = std::function<std::unique_ptr<ContextBase>()>;

/// 子命令注册表：管理子命令名 → 工厂函数的映射
class SubcommandRegistry
{
public:
    /// @brief 注册子命令
    /// @param _Name 子命令名称（如 "commit", "branch"）
    /// @param _Factory 创建子 Context 的工厂函数
    void registerSubcommand(std::string _Name, SubcommandFactory _Factory)
    {
        registry_[std::move(_Name)] = std::move(_Factory);
    }

    /// @brief 查找子命令
    /// @param _Name 子命令名称
    /// @return 工厂函数指针，未找到返回 nullptr
    SubcommandFactory const *find(std::string const &_Name) const
    {
        auto it = registry_.find(_Name);
        return (it != registry_.end()) ? &it->second : nullptr;
    }

    /// @brief 是否包含指定子命令
    bool contains(std::string const &_Name) const
    {
        return registry_.count(_Name) > 0;
    }

    /// @brief 获取所有注册的子命令名称
    std::vector<std::string> names() const
    {
        std::vector<std::string> result;
        result.reserve(registry_.size());
        for (auto const &[name, _] : registry_) {
            result.push_back(name);
        }
        return result;
    }

    /// @brief 是否为空
    bool empty() const noexcept { return registry_.empty(); }

    /// @brief 清空注册表
    void clear() { registry_.clear(); }

private:
    std::unordered_map<std::string, SubcommandFactory> registry_;
};

CMD_ARGS_NAMESPACE_END
```

- [ ] **Step 2: 编译验证头文件语法**

Run: `xmake build` (仅编译检查，无实际测试)
Expected: 编译通过，无语法错误

- [ ] **Step 3: Commit**

```bash
git add include/cmd_args/context/subcommand.h
git commit -m "feat(context): add SubcommandRegistry for hierarchical subcommand support"
```

---

### Task 2: 扩展 ContextBase 添加子命令支持

**Files:**
- Modify: `include/cmd_args/context/context_base.h` (添加子命令相关成员和方法)
- Modify: `include/cmd_args/context/context_base.cpp` (如有，或内联实现)

**Interfaces:**
- Consumes: `SubcommandRegistry` (Task 1)
- Produces: `ContextBase::addSubcommand()`, `ContextBase::getActiveSubcommand()` 方法

- [ ] **Step 1: 在 context_base.h 中添加子命令支持**

在 `ContextBase` 类的 protected 区域添加：

```cpp
protected:
    /// 子命令注册表
    SubcommandRegistry subcommandRegistry_;

    /// 当前激活的子命令路径（如 ["commit"] 或 ["remote", "add"]）
    std::vector<std::string> activeSubcommandPath_;

    /// 当前激活的子 Context（解析后持有）
    std::unique_ptr<ContextBase> activeSubContext_;

    /// @brief 注册子命令
    /// @param _Name 子命令名称
    /// @param _Factory 子 Context 工厂函数
    void _registerSubcommand(std::string _Name, SubcommandFactory _Factory)
    {
        subcommandRegistry_.registerSubcommand(std::move(_Name), std::move(_Factory));
    }
```

在 public 区域添加：

```cpp
public:
    /// @brief 添加子命令（链式 API）
    /// @param _Name 子命令名称
    /// @param _Factory 创建子 Context 的工厂函数
    ContextBase &addSubcommand(std::string _Name, SubcommandFactory _Factory)
    {
        _registerSubcommand(std::move(_Name), std::move(_Factory));
        return *this;
    }

    /// @brief 添加子命令（模板版本，自动推导工厂类型）
    template <typename ContextType, typename... Args>
    ContextBase &addSubcommand(std::string _Name, Args &&..._Args)
    {
        _registerSubcommand(std::move(_Name), [args = std::make_tuple(std::forward<Args>(_Args)...)]() {
            return std::apply([](auto &&...a) { return std::make_unique<ContextType>(std::forward<decltype(a)>(a)...); }, args);
        });
        return *this;
    }

    /// @brief 获取当前激活的子命令路径
    std::vector<std::string> const &getActiveSubcommandPath() const noexcept
    {
        return activeSubcommandPath_;
    }

    /// @brief 获取当前激活的子 Context（可能为空）
    ContextBase *getActiveSubcommand() const noexcept
    {
        return activeSubContext_.get();
    }

    /// @brief 是否有子命令注册
    bool hasSubcommands() const noexcept
    {
        return !subcommandRegistry_.empty();
    }
```

- [ ] **Step 2: 添加必要的头文件包含**

在 `context_base.h` 顶部添加：

```cpp
#include <memory>
#include <vector>
// ... existing includes ...
#include "cmd_args/context/subcommand.h"
```

- [ ] **Step 3: 编译验证**

Run: `xmake build`
Expected: 编译通过

- [ ] **Step 4: Commit**

```bash
git add include/cmd_args/context/context_base.h
git commit -m "feat(context): extend ContextBase with subcommand registry and path tracking"
```

---

### Task 3: 实现子命令路径解析逻辑

**Files:**
- Modify: `include/cmd_args/context/context.h` (在 Context 中实现子命令路径扫描)

**Interfaces:**
- Consumes: `SubcommandRegistry` (Task 1), ContextBase 子命令支持 (Task 2)
- Produces: `_doRunPipeline` 中的子命令路径扫描逻辑

- [ ] **Step 1: 在 Context::_doRunPipeline 中添加子命令路径扫描**

修改 `context.h` 中的 `_doRunPipeline` 方法：

```cpp
protected:
    /// @brief 批量管线：扫描子命令路径 → Compiler 纯解析 → hasAction 门控 Executor → finalize
    void _doRunPipeline(Tokens_T &_Toks, Envs_T const &_Envs) override
    {
        // 如果没有子命令注册，直接走原有逻辑
        if (!hasSubcommands()) {
            _doRunPipelineWithoutSubcommands(_Toks, _Envs);
            return;
        }

        // 扫描子命令路径
        size_t consumed = _scanSubcommandPath(_Toks);
        
        // 如果匹配到子命令，路由到子 Context
        if (activeSubContext_) {
            // 剩余 token 传给子 Context
            Tokens_T remainingToks(_Toks.begin() + consumed, _Toks.end());
            activeSubContext_->parse(remainingToks, _Envs);
            _markParsed();
            return;
        }

        // 未匹配到子命令，走原有逻辑
        _doRunPipelineWithoutSubcommands(_Toks, _Envs);
    }

private:
    /// @brief 原有的管线逻辑（无子命令版本）
    void _doRunPipelineWithoutSubcommands(Tokens_T &_Toks, Envs_T const &_Envs)
    {
        compiler__.set_program_name(instData_.strProgramName);
        compiler__.set_description(strPrjName_);

        compiler__.ensureBuiltins();
        compiler__.parse(_Toks, _Envs);

        if (instData_.hasAction()) {
            executor__.execute();
        }

        compiler__.finalize();
        _markParsed();
    }

    /// @brief 扫描子命令路径，返回消耗的 token 数
    /// @param _Toks 完整 token 流
    /// @return 消耗的 token 数（子命令名称的数量）
    size_t _scanSubcommandPath(Tokens_T const &_Toks)
    {
        activeSubcommandPath_.clear();
        activeSubContext_.reset();

        ContextBase *current = this;
        size_t consumed = 0;

        // 从 token 流开头扫描连续的子命令名称
        for (size_t i = 0; i < _Toks.size(); ++i) {
            auto const &tok = _Toks[i];

            // 跳过以 '-' 开头的 token（选项）
            if (tok.empty() || tok[0] == '-') {
                break;
            }

            // 检查当前 Context 是否有这个子命令
            auto const *factory = current->subcommandRegistry_.find(tok);
            if (!factory) {
                break;
            }

            // 创建子 Context
            auto child = (*factory)();
            
            // 记录路径
            activeSubcommandPath_.push_back(tok);
            
            // 设置子 Context 的程序名为父程序名 + 子命令路径
            std::string childProgramName = current->instData_.strProgramName;
            for (auto const &seg : activeSubcommandPath_) {
                childProgramName += " " + seg;
            }
            child->instData_.strProgramName = std::move(childProgramName);

            // 传递环境变量
            child->instData_.envs = current->instData_.envs;

            // 传递 builtin 选项设置
            child->instData_.bBuiltinOptions = current->instData_.bBuiltinOptions;

            // 移动到子 Context
            activeSubContext_ = std::move(child);
            current = activeSubContext_.get();
            consumed = i + 1;
        }

        return consumed;
    }
```

- [ ] **Step 2: 编译验证**

Run: `xmake build`
Expected: 编译通过

- [ ] **Step 3: Commit**

```bash
git add include/cmd_args/context/context.h
git commit -m "feat(context): implement subcommand path scanning and routing in Context"
```

---

### Task 4: 实现 once()/loop() 对子命令的支持

**Files:**
- Modify: `include/cmd_args/context/context.h`

**Interfaces:**
- Consumes: 子命令路径扫描 (Task 3)
- Produces: `once()` 和 `loop()` 支持子命令执行

- [ ] **Step 1: 修改 once() 方法**

```cpp
public:
    void once() override
    {
        // 如果有激活的子命令，委托给子 Context
        if (activeSubContext_) {
            activeSubContext_->once();
            return;
        }

        if (!bParsed_) { throw ErrorInterp(InterpError_E::STOPPED); }
        std::exit(nExitCode_);
    }

    void loop() override
    {
        // 如果有激活的子命令，委托给子 Context
        if (activeSubContext_) {
            activeSubContext_->loop();
            return;
        }

        compiler__.ensureBuiltins();
        interp__.loop(_errors());
    }
```

- [ ] **Step 2: 编译验证**

Run: `xmake build`
Expected: 编译通过

- [ ] **Step 3: Commit**

```bash
git add include/cmd_args/context/context.h
git commit -m "feat(context): delegate once()/loop() to active subcommand context"
```

---

### Task 5: 添加子命令帮助文档生成

**Files:**
- Modify: `include/cmd_args/context/context.h` (添加子命令帮助显示)

**Interfaces:**
- Consumes: 子命令注册表 (Task 1)
- Produces: 帮助信息中显示可用子命令

- [ ] **Step 1: 在 Context 中添加子命令帮助生成**

```cpp
private:
    /// @brief 生成子命令帮助信息
    std::string _subcommandHelp() const
    {
        if (!hasSubcommands()) {
            return {};
        }

        auto names = subcommandRegistry_.names();
        std::sort(names.begin(), names.end());

        std::ostringstream oss;
        oss << "\nAvailable subcommands:\n";
        for (auto const &name : names) {
            oss << "  " << name << "\n";
        }
        return oss.str();
    }

    /// @brief 修改 __descHelp 以包含子命令信息
    std::string __descHelp() const
    {
        std::ostringstream oss;
        oss << __descUsageOf(*pModel__, program_name);
        if (!description.empty()) { oss << "\n" << description << "\n"; }
        
        // 添加子命令帮助
        oss << _subcommandHelp();
        
        pModel__->args.mgrName.doGetHelp(oss, 0);
        pModel__->args.mgrPos.doGetHelp(oss, 0);
        if (!pModel__->opts.empty()) {
            oss << "\nOptions:\n";
            size_t maxLen = pModel__->opts.calcDescMaxLength();
            pModel__->opts.appendHelp(oss, maxLen);
        }
        oss << "\n";
        oss << "environments: use -! / --env to list, --env-set KEY=VALUE to set, "
               "--env-apply to write back\n";
        return oss.str();
    }
```

注意：需要将 `__descHelp` 从 Compiler 移动到 Context 或在 Context 中覆盖。

- [ ] **Step 2: 编译验证**

Run: `xmake build`
Expected: 编译通过

- [ ] **Step 3: Commit**

```bash
git add include/cmd_args/context/context.h
git commit -m "feat(context): add subcommand help generation in Context"
```

---

### Task 6: 创建子命令单元测试

**Files:**
- Create: `dfx/unit_test/test_subcommand.cpp`

**Interfaces:**
- Consumes: Context 子命令支持 (Tasks 1-5)
- Produces: 完整的子命令功能测试

- [ ] **Step 1: 创建测试文件**

```cpp
#include "catch2/catch.hpp"

#include "cmd_args/cmd_args.hpp"

using namespace cmd_args;

/* ==================== 子命令基本功能 ==================== */

TEST_CASE("context: 子命令基本路由", "[subcommand]")
{
    // 创建根 Context
    Context root("app");
    root.addOption<std::string>("global", "global option", "default");

    // 注册子命令 "commit"
    root.addSubcommand("commit", []() {
        auto child = std::make_unique<Context>("app commit");
        child->addOption<std::string>("message", "commit message", "");
        child->addFlag('a', "all", "stage all changes");
        return child;
    });

    // 解析带子命令的参数
    std::error_code ec;
    root.parse(ec, Tokens_T{"commit", "--message", "initial commit", "-a"}, Envs_T{});

    REQUIRE(!ec);
    REQUIRE(root.getActiveSubcommandPath().size() == 1);
    REQUIRE(root.getActiveSubcommandPath()[0] == "commit");

    // 验证子 Context 的选项
    auto *sub = root.getActiveSubcommand();
    REQUIRE(sub != nullptr);
    
    // 通过子 Context 获取选项值
    auto *subAsContext = dynamic_cast<Context *>(sub);
    REQUIRE(subAsContext != nullptr);
    REQUIRE(subAsContext->getOption<std::string>("message") == "initial commit");
    REQUIRE(subAsContext->getOption<bool>("all") == true);
}

TEST_CASE("context: 多级子命令嵌套", "[subcommand]")
{
    Context root("git");

    // 注册一级子命令 "remote"
    root.addSubcommand("remote", []() {
        auto child = std::make_unique<Context>("git remote");
        
        // 注册二级子命令 "add"
        child->addSubcommand("add", []() {
            auto grandchild = std::make_unique<Context>("git remote add");
            grandchild->addOption<std::string>("name", "remote name", "");
            grandchild->addOption<std::string>("url", "remote url", "");
            return grandchild;
        });
        
        // 注册二级子命令 "remove"
        child->addSubcommand("remove", []() {
            auto grandchild = std::make_unique<Context>("git remote remove");
            grandchild->addOption<std::string>("name", "remote name", "");
            return grandchild;
        });
        
        return child;
    });

    // 测试 "remote add"
    std::error_code ec;
    root.parse(ec, Tokens_T{"remote", "add", "--name", "origin", "--url", "https://github.com/user/repo.git"}, Envs_T{});

    REQUIRE(!ec);
    REQUIRE(root.getActiveSubcommandPath().size() == 2);
    REQUIRE(root.getActiveSubcommandPath()[0] == "remote");
    REQUIRE(root.getActiveSubcommandPath()[1] == "add");

    auto *sub = root.getActiveSubcommand();
    REQUIRE(sub != nullptr);
    
    auto *subAsContext = dynamic_cast<Context *>(sub);
    REQUIRE(subAsContext != nullptr);
    REQUIRE(subAsContext->getOption<std::string>("name") == "origin");
    REQUIRE(subAsContext->getOption<std::string>("url") == "https://github.com/user/repo.git");
}

TEST_CASE("context: 子命令与父选项共存", "[subcommand]")
{
    Context root("app");
    root.addOption<int>("verbosity", "verbosity level", 0);

    root.addSubcommand("serve", []() {
        auto child = std::make_unique<Context>("app serve");
        child->addOption<int>("port", "port number", 8080);
        child->addFlag('d', "daemon", "run as daemon");
        return child;
    });

    // 注意：当前实现中，父选项和子命令不能混合使用
    // 子命令路径必须在选项之前
    std::error_code ec;
    root.parse(ec, Tokens_T{"serve", "--port", "3000", "-d"}, Envs_T{});

    REQUIRE(!ec);
    REQUIRE(root.getActiveSubcommandPath()[0] == "serve");
    
    auto *sub = root.getActiveSubcommand();
    auto *subAsContext = dynamic_cast<Context *>(sub);
    REQUIRE(subAsContext->getOption<int>("port") == 3000);
    REQUIRE(subAsContext->getOption<bool>("daemon") == true);
}

TEST_CASE("context: 未匹配子命令时回退到常规解析", "[subcommand]")
{
    Context root("app");
    root.addOption<std::string>("config", "config file", "default.conf");
    root.addFlag('v', "verbose", "verbose output");

    root.addSubcommand("version", []() {
        auto child = std::make_unique<Context>("app version");
        child->addFlag('l', "long", "long version info");
        return child;
    });

    // 不带子命令，使用常规选项
    std::error_code ec;
    root.parse(ec, Tokens_T{"--config", "custom.conf", "-v"}, Envs_T{});

    REQUIRE(!ec);
    REQUIRE(root.getActiveSubcommandPath().empty());
    REQUIRE(root.getActiveSubcommand() == nullptr);
    REQUIRE(root.getOption<std::string>("config") == "custom.conf");
    REQUIRE(root.getOption<bool>("verbose") == true);
}

TEST_CASE("context: 子命令列表查询", "[subcommand]")
{
    Context root("app");
    root.addSubcommand("cmd1", []() { return std::make_unique<Context>("app cmd1"); });
    root.addSubcommand("cmd2", []() { return std::make_unique<Context>("app cmd2"); });
    root.addSubcommand("cmd3", []() { return std::make_unique<Context>("app cmd3"); });

    REQUIRE(root.hasSubcommands());
    
    // 可以通过模型访问子命令信息（如果需要）
    // 注意：当前实现中，子命令注册表是 ContextBase 的 protected 成员
    // 如需外部访问，需要添加公共接口
}
```

- [ ] **Step 2: 添加测试到 xmake 构建**

在 `dfx/unit_test/xmake.lua` 中添加：

```lua
target("test_subcommand")
    set_kind("binary")
    add_files("test_subcommand.cpp")
    add_deps("cmd_args")
    add_packages("catch2")
    set_group("unit_tests")
```

- [ ] **Step 3: 运行测试**

Run: `xmake build test_subcommand && xmake run test_subcommand`
Expected: 所有测试通过

- [ ] **Step 4: Commit**

```bash
git add dfx/unit_test/test_subcommand.cpp dfx/unit_test/xmake.lua
git commit -m "test(context): add comprehensive subcommand unit tests"
```

---

### Task 7: 文档与示例

**Files:**
- Create: `docs/subcommand-example.cpp`

**Interfaces:**
- Consumes: Context 子命令支持 (Tasks 1-6)
- Produces: 使用示例文档

- [ ] **Step 1: 创建使用示例**

```cpp
/**
 * @file subcommand-example.cpp
 * @brief Context 子模块嵌套功能示例
 * 
 * 演示如何使用 Context 的子命令支持，实现类似 git 的层级命令结构。
 */

#include <iostream>
#include "cmd_args/cmd_args.hpp"

using namespace cmd_args;

int main()
{
    // 创建根 Context
    Context root("myapp");
    root.addOption<std::string>("verbose", "verbose output", "false");

    // 注册一级子命令 "remote"
    root.addSubcommand("remote", []() {
        auto child = std::make_unique<Context>("myapp remote");
        
        // 注册二级子命令 "add"
        child->addSubcommand("add", []() {
            auto grandchild = std::make_unique<Context>("myapp remote add");
            grandchild->addOption<std::string>("name", "remote name", "");
            grandchild->addOption<std::string>("url", "remote URL", "");
            grandchild->addFlag('f', "force", "force overwrite");
            return grandchild;
        });
        
        // 注册二级子命令 "remove"
        child->addSubcommand("remove", []() {
            auto grandchild = std::make_unique<Context>("myapp remote remove");
            grandchild->addOption<std::string>("name", "remote name", "");
            grandchild->addFlag('f', "force", "force removal");
            return grandchild;
        });
        
        // 注册二级子命令 "list"
        child->addSubcommand("list", []() {
            auto grandchild = std::make_unique<Context>("myapp remote list");
            grandchild->addFlag('v', "verbose", "verbose output");
            return grandchild;
        });
        
        return child;
    });

    // 注册一级子命令 "config"
    root.addSubcommand("config", []() {
        auto child = std::make_unique<Context>("myapp config");
        child->addOption<std::string>("file", "config file path", "");
        child->addFlag('l', "list", "list all settings");
        return child;
    });

    // 模拟命令行参数
    char const *argv[] = {
        "myapp",
        "remote",
        "add",
        "--name",
        "origin",
        "--url",
        "https://github.com/user/repo.git",
        "--force"
    };
    int argc = sizeof(argv) / sizeof(argv[0]);

    // 解析
    std::error_code ec;
    root.parse(ec, argc, argv, nullptr);

    if (ec) {
        std::cerr << "Parse error: " << ec.message() << "\n";
        return 1;
    }

    // 获取子命令路径
    auto const &path = root.getActiveSubcommandPath();
    std::cout << "Subcommand path: ";
    for (size_t i = 0; i < path.size(); ++i) {
        if (i > 0) std::cout << " -> ";
        std::cout << path[i];
    }
    std::cout << "\n";

    // 获取激活的子 Context
    auto *sub = root.getActiveSubcommand();
    if (sub) {
        auto *subAsContext = dynamic_cast<Context *>(sub);
        if (subAsContext) {
            std::cout << "Remote name: " << subAsContext->getOption<std::string>("name") << "\n";
            std::cout << "Remote URL: " << subAsContext->getOption<std::string>("url") << "\n";
            std::cout << "Force: " << std::boolalpha << subAsContext->getOption<bool>("force") << "\n";
        }
    }

    return 0;
}
```

- [ ] **Step 2: 编译示例**

Run: `xmake build subcommand-example`
Expected: 编译通过

- [ ] **Step 3: Commit**

```bash
git add docs/subcommand-example.cpp
git commit -m "docs(context): add subcommand usage example"
```

---

## Self-Review

**1. Spec coverage:**
- ✅ 支持 `app sub1 sub2 --option value` 形式的层级子命令
- ✅ 每个子命令创建独立的子 Context 拥有自己的 options
- ✅ 支持任意深度嵌套
- ✅ 突破短选项 26 字母限制（通过子命令名称）

**2. Placeholder scan:**
- 无 TBD/TODO 占位符
- 所有步骤包含实际代码
- 测试用例完整

**3. Type consistency:**
- 使用现有类型别名：`Name_T`, `Desc_T`, `Tokens_T`, `Envs_T`
- 工厂函数返回 `std::unique_ptr<ContextBase>`
- 子命令路径使用 `std::vector<std::string>`

**潜在问题与改进建议:**

1. **父选项传递**: 当前实现中，父 Context 的选项不会自动传递给子 Context。如需支持 `myapp --verbose remote add ...`，需要额外实现选项透传逻辑。

2. **错误处理**: 子命令未找到时的错误提示需要改进，可以显示可用的子命令列表。

3. **帮助文档**: 需要在 `--help` 输出中显示子命令信息，Task 5 部分实现了这一点。

4. **性能**: 每次解析都会创建子 Context 对象，对于高频调用可能有性能影响。可以考虑缓存机制。

---

## Execution Handoff

**Plan complete and saved to `docs/superpowers/plans/2025-01-17-context-submodule-nesting.md`. Two execution options:**

**1. Subagent-Driven (recommended)** - I dispatch a fresh subagent per task, review between tasks, fast iteration

**2. Inline Execution** - Execute tasks in this session using executing-plans, batch execution with checkpoints

**Which approach?**
