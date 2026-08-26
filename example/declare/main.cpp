/*
 * example / declare
 *
 * 声明式风格：declare() 一次性声明所有参数/选项/标志。
 *
 * declare() 接受 ArgumentNamed / ArgumentPosition / OptionDef<T> / FlagDef
 * 混合传入，通过折叠表达式自动分发到对应的 Builder 方法。
 *
 * 对比链式风格的优势：
 *   - 所有声明集中在一处，结构清晰
 *   - 无需反复写 addOption<T> / addFlag
 *   - OptionDef 的 T 从 defaultValue 自动推导
 *
 * 运行示例（xmake）：
 *   xmake r declare -- --name Alice --output result.txt -v
 *   xmake r declare -- -?
 */
#include <iostream>
#include <string>

#include "cmd_args/cmd_args.hpp"

int main(int _Argc, char *_Argv[], char *_Envp[])
{
    using namespace cmd_args;

    Context app("declare_demo");

    /* ---------- 声明式：一次性声明全部参数/选项/标志 ---------- */

    app.declare(
        // 命名参数
        ArgumentNamed{"name", "your name"},
        ArgumentNamed{"greeting", "greeting message"},
        // 方位参数
        ArgumentPosition{"output", "output file"},
        // 值选项（T 从 defaultValue 自动推导）
        OptionDef<int>{'t', "timeout", "timeout in seconds", 30},
        OptionDef<std::string>{'o', "format", "output format", "json"},
        // 布尔标志
        FlagDef{'v', "verbose", "enable verbose output"},
        FlagDef{'q', "quiet", "suppress output"}
    );

    /* ---------- 也可以追加声明（与现有 API 混用） ---------- */

    app.addOption<int>("retry", "retry count", 3)
       .addFlag('d', "dry-run", "dry run mode");

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
    std::cout << "retry    = " << app.getOption<int>("retry") << "\n";
    std::cout << "verbose  = " << (app.getOption<bool>("verbose") ? "on" : "off") << "\n";
    std::cout << "quiet    = " << (app.getOption<bool>("quiet") ? "on" : "off") << "\n";
    std::cout << "dry-run  = " << (app.getOption<bool>("dry-run") ? "on" : "off") << "\n";

    return 0;
}
