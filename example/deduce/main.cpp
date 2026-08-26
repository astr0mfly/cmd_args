/*
 * example / deduce
 *
 * 简化链式风格：addOption() 类型推导重载。
 *
 * 为 int / double / std::string 提供非模板重载，
 * 从默认值参数自动推导 T，省去 <T> 显式指定。
 *
 * 对比传统写法：
 *   addOption<int>("timeout", "desc", 30)    →  addOption("timeout", "desc", 30)
 *   addOption<std::string>("output", "desc", "out.txt")
 *                                            →  addOption("output", "desc", std::string("out.txt"))
 *   addOption<double>("ratio", "desc", 0.5)  →  addOption("ratio", "desc", 0.5)
 *
 * 注意：string 字面量（const char*）无法直接推导为 std::string，
 * 需显式构造 std::string 或在 OptionDef<T> 中指定类型。
 *
 * 运行示例（xmake）：
 *   xmake r deduce -- --timeout 60 --ratio 0.8 --output result.txt
 *   xmake r deduce -- -?
 */
#include <iostream>
#include <string>

#include "cmd_args/cmd_args.hpp"

int main(int _Argc, char *_Argv[], char *_Envp[])
{
    using namespace cmd_args;

    Context app("deduce_demo");

    /* ---------- 简化链式：无需 <T> 模板参数 ---------- */

    // int 推导
    app.addOption("timeout", "timeout in seconds", 30);

    // double 推导
    app.addOption("ratio", "ratio value", 0.75);

    // std::string 推导（需显式构造 string）
    app.addOption("output", "output file", std::string("out.txt"));

    // 含短名的版本同样支持推导
    app.addOption('j', "jobs", "parallel jobs", 4);

    // 标志选项（不涉及推导）
    app.addFlag('v', "verbose", "enable verbose output");
    app.addFlag('q', "quiet", "suppress output");

    /* ---------- 也可以混用传统写法 ---------- */

    app.addOption<std::string>("format", "output format", "json");
    app.build<ArgumentPosition>("file", "input file");

    /* ---------- 解析 ---------- */

    std::error_code ec;
    app.parse(ec, _Argc, const_cast<char const **>(_Argv), _Envp);
    if (ec) {
        std::cerr << "(parse error) " << ec.message() << "\n";
        return static_cast<int>(ec.value());
    }

    /* ---------- 读取结果 ---------- */

    std::cout << "timeout = " << app.getOption<int>("timeout") << "\n";
    std::cout << "ratio   = " << app.getOption<double>("ratio") << "\n";
    std::cout << "output  = " << app.getOption<std::string>("output") << "\n";
    std::cout << "jobs    = " << app.getOption<int>("jobs") << "\n";
    std::cout << "format  = " << app.getOption<std::string>("format") << "\n";
    std::cout << "file    = " << app.getArgument<std::string>("file") << "\n";
    std::cout << "verbose = " << (app.getOption<bool>("verbose") ? "on" : "off") << "\n";
    std::cout << "quiet   = " << (app.getOption<bool>("quiet") ? "on" : "off") << "\n";

    return 0;
}
