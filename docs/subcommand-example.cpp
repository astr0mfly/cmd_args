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
        "--name=origin",
        "--url=https://github.com/user/repo.git",
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
