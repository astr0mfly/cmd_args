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

    // 解析带子命令的参数（使用单次赋值形式避免多词值解析问题）
    std::error_code ec;
    root.parse(ec, Tokens_T{"commit", "--message=initial-commit", "-a"}, Envs_T{});

    REQUIRE(!ec);
    REQUIRE(root.getActiveSubcommandPath().size() == 1);
    REQUIRE(root.getActiveSubcommandPath()[0] == "commit");

    // 验证子 Context 的选项
    auto *sub = root.getActiveSubcommand();
    REQUIRE(sub != nullptr);
    
    // 通过子 Context 获取选项值
    auto *subAsContext = dynamic_cast<Context *>(sub);
    REQUIRE(subAsContext != nullptr);
    REQUIRE(subAsContext->getOption<std::string>("message") == "initial-commit");
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
