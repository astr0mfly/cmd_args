/*
 * example / chain
 *
 * 链式 SPI（Service Provider Interface）风格：
 *   build<ArgumentNamed>() / build<ArgumentPosition>() 链式调用
 *   addOption<T>() / addFlag() 返回 Self_T& 支持串联
 *   这是 cmd_args 最初的 API 风格，适合参数少、逻辑简单的场景。
 *
 * 运行示例（xmake）：
 *   xmake r chain -- --name Alice --output result.txt -v
 *   xmake r chain -- -?
 */
#include <iostream>
#include <string>

#include "cmd_args/cmd_args.hpp"

int main(int _Argc, char *_Argv[], char *_Envp[])
{
    using namespace cmd_args;

    Context app("chain_demo");

    /* ---------- 链式 SPI：build + addOption + addFlag 串联 ---------- */

    app.build<ArgumentNamed>("name", "your name")
        .build<ArgumentNamed>("greeting", "greeting message")
        .build<ArgumentPosition>("output", "output file")
        .addOption<int>('t', "timeout", "timeout in seconds", 30)
        .addOption<std::string>('o', "format", "output format", "json")
        .addFlag('v', "verbose", "enable verbose output")
        .addFlag('q', "quiet", "suppress output");

    /* ---------- 解析 ---------- */

    std::error_code ec;
    app.parse(ec, _Argc, const_cast<char const **>(_Argv), _Envp);
    if (ec) {
        std::cerr << "(parse error) " << ec.message() << "\n";
        return static_cast<int>(ec.value());
    }

    /* ---------- 读取结果 ---------- */

    std::cout << "name     = " << app.getArgument<std::string>("name") << "\n";
    std::cout << "greeting = " << app.getArgument<std::string>("greeting") << "\n";
    std::cout << "output   = " << app.getArgument<std::string>("output") << "\n";
    std::cout << "timeout  = " << app.getOption<int>("timeout") << "\n";
    std::cout << "format   = " << app.getOption<std::string>("format") << "\n";
    std::cout << "verbose  = " << (app.getOption<bool>("verbose") ? "on" : "off") << "\n";
    std::cout << "quiet    = " << (app.getOption<bool>("quiet") ? "on" : "off") << "\n";

    return 0;
}
